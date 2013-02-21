#include <jni.h>
#include <string.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <pthread.h>
#include "xlog.h"
#include "recorder.h"
#include <math.h>
// engine interfaces
static SLObjectItf engineObject = NULL;
static SLEngineItf engineEngine;
// recorder interfaces
static SLObjectItf recorderObject = NULL;
static SLRecordItf recorderRecord;
static SLAndroidSimpleBufferQueueItf recorderBufferQueue;

//const int SRATE=8000;
//const int NBUF=2;
//const int BUFFER_LEN = 2048;
//static short recBuffer[NBUF][BUFFER_LEN];

int g_srate = 0;
int g_totalbufferlen = 0;
short* g_buffer = 0;
const int g_bufsplit=8;
int g_splitlen = 0;
int g_buffer_samples = 0;
short* g_cursplit = 0;

pthread_mutex_t g_buffer_mutex  = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t g_cond= PTHREAD_COND_INITIALIZER;

static void initBuffer( int srate ){
	pthread_mutex_lock( &g_buffer_mutex );
	g_srate = srate;
	g_totalbufferlen = srate;
	xassert( g_buffer == 0 );
	g_buffer = new short[g_totalbufferlen];
	g_splitlen = g_totalbufferlen / g_bufsplit;
	g_buffer_samples = 0;
	pthread_mutex_unlock( &g_buffer_mutex );

	for( int i=0; i<2; ++i ){
    	SLresult result = (*recorderBufferQueue)->Enqueue(recorderBufferQueue, g_buffer+i*g_splitlen, g_splitlen*2 );
    	LOGI( "enqueue: %i", i );
    	xassert(SL_RESULT_SUCCESS == result);
    }
}

static void putBuffer( short* cursplit )
{
	pthread_mutex_lock( &g_buffer_mutex );
	g_buffer_samples += g_splitlen;
	g_cursplit = cursplit;
	pthread_cond_broadcast( &g_cond );
	pthread_mutex_unlock( &g_buffer_mutex );
}

static void killBuffer()
{
	pthread_mutex_lock( &g_buffer_mutex );
	g_buffer = 0;
	pthread_cond_broadcast( &g_cond );
	pthread_mutex_unlock( &g_buffer_mutex );
}



static float getVolume()
{
	pthread_mutex_lock( &g_buffer_mutex );
	pthread_cond_wait( &g_cond, &g_buffer_mutex );
	if( g_buffer==0 ){
		pthread_mutex_unlock( &g_buffer_mutex );
		return 289;
	}
	float s;
	for( int i=0; i<g_splitlen; ++i ){
		float h = g_cursplit[i];
		s += h*h;
	}
	pthread_mutex_unlock( &g_buffer_mutex );
	return log1p(s /g_splitlen) * 0.055;
}

int getSamples( short* p, int n0, int& total )
{
	int n1 = 0; // copied
	xassert( n0>0 );
	pthread_mutex_lock( &g_buffer_mutex );
	if( g_buffer == 0){
		pthread_mutex_unlock(&g_buffer_mutex );
		return 0;
	}
	if( total + (g_bufsplit-1) * g_splitlen < g_buffer_samples ){ // buffer overflow
		if( total != REC_BUFFER_INIT )
			LOGE("buffer overflow: %i %i", total, g_totalbufferlen );
		total = g_buffer_samples;
	}
	short* p_from = g_buffer + total % g_totalbufferlen;
	while( n1 < n0 ){
		if( total == g_buffer_samples ){
			pthread_cond_wait( &g_cond, &g_buffer_mutex );
			if( g_buffer == 0 )
				break;
		}
		int nc = g_buffer_samples - total;
		int to_buffer_end = g_totalbufferlen - (p_from-g_buffer);
		if( nc > to_buffer_end )
			nc = to_buffer_end;
		if( nc > n0-n1 )
			nc = n0-n1;
		xassert( nc>0 );
		memcpy( p, p_from, nc* 2);
		n1 += nc;
		p += nc;
		total += nc;
		if( nc == to_buffer_end)
			p_from = g_buffer;
		else
			p_from += nc;
	}
	pthread_mutex_unlock(&g_buffer_mutex );
	return n1;
}

static void createEngine()
{
    SLresult result;

    // create engine
    result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    xassert(SL_RESULT_SUCCESS == result);

    // realize the engine
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    xassert(SL_RESULT_SUCCESS == result);

    // get the engine interface, which is needed in order to create other objects
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);
    xassert(SL_RESULT_SUCCESS == result);
}

// this callback handler is called every time a buffer finishes recording
static int last_buffer_index = 0;
static void bqRecorderCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
    xassert(bq == recorderBufferQueue);
    xassert(NULL == context);
	SLAndroidSimpleBufferQueueState state;
    SLresult res = (*bq) ->GetState( bq, &state );
    xassert(SL_RESULT_SUCCESS == res);
	xassert( state.index == ++last_buffer_index );

	const int n_current = (state.index - 1) % g_bufsplit;
	const int n_next = (state.index + 1 ) % g_bufsplit;
	short* b_current = g_buffer + n_current * g_splitlen;
	short* b_next = g_buffer + n_next * g_splitlen;
	res = (*recorderBufferQueue)->Enqueue(recorderBufferQueue, b_next, g_splitlen * sizeof( *g_buffer ) );

	putBuffer( b_current );

    xassert(SL_RESULT_SUCCESS == res);
}


// create audio recorder
static jboolean createAudioRecorder( int srate )
{
	LOGI("starting Recorder ...");
	xassert( recorderObject == NULL );
    SLresult result;

    // configure audio source
    SLDataLocator_IODevice loc_dev = {SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT,
            SL_DEFAULTDEVICEID_AUDIOINPUT, NULL};
    SLDataSource audioSrc = {&loc_dev, NULL};

    // configure audio sink
    SLDataLocator_AndroidSimpleBufferQueue loc_bq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
    SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM, 1, (unsigned)(srate*1000),
        SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
        SL_SPEAKER_FRONT_CENTER, SL_BYTEORDER_LITTLEENDIAN};

    SLDataSink audioSnk = {&loc_bq, &format_pcm};

    // create audio recorder
    // (requires the RECORD_AUDIO permission)
    const SLInterfaceID id[1] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
    const SLboolean req[1] = {SL_BOOLEAN_TRUE};
    result = (*engineEngine)->CreateAudioRecorder(engineEngine, &recorderObject, &audioSrc,
            &audioSnk, 1, id, req);
    if (SL_RESULT_SUCCESS != result) {
        return JNI_FALSE;
    }

    // realize the audio recorder
    result = (*recorderObject)->Realize(recorderObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        return JNI_FALSE;
    }

    // get the record interface
    result = (*recorderObject)->GetInterface(recorderObject, SL_IID_RECORD, &recorderRecord);
    xassert(SL_RESULT_SUCCESS == result);

    // get the buffer queue interface
    result = (*recorderObject)->GetInterface(recorderObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
            &recorderBufferQueue);
    xassert(SL_RESULT_SUCCESS == result);

    // register callback on the buffer queue
    result = (*recorderBufferQueue)->RegisterCallback(recorderBufferQueue, bqRecorderCallback,
            NULL);
    xassert(SL_RESULT_SUCCESS == result);

    initBuffer( srate );

    result = (*recorderRecord)->SetRecordState(recorderRecord, SL_RECORDSTATE_RECORDING);
    xassert(SL_RESULT_SUCCESS == result);
    last_buffer_index = 0;
	LOGI("Recorder Started.");

	return JNI_TRUE;
}

static void stopRecorder()
{
	LOGI("stopping Recorder ...");
    (*recorderObject)->Destroy(recorderObject);
    recorderObject = NULL;
    recorderRecord = NULL;
    recorderBufferQueue = NULL;
	LOGI("Recorder stopped");
	killBuffer();
}

bool recorder_is_running()
{
	return recorderObject != 0;
}

extern "C"
void Java_anofax_opusrecord_Native_startRecorder(JNIEnv* env, jclass clazz, int srate )
{
	createAudioRecorder(srate);
}

extern "C"
void Java_anofax_opusrecord_Native_stopRecorder(JNIEnv* env, jclass clazz )
{
	stopRecorder();
}

extern "C"
void Java_anofax_opusrecord_Native_initAudio(JNIEnv* env, jclass clazz )
{
	createEngine();
	LOGI("Audio init");
}

extern "C"
jfloat Java_anofax_opusrecord_Native_volume(JNIEnv* env, jclass clazz )
{
	float res = getVolume();
	LOGV("Volume: %f", res );
	return res;
}
