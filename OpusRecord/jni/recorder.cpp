#include <jni.h>
#include <string.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include "xlog.h"

void setBuffer( short* b, int nb );

// engine interfaces
static SLObjectItf engineObject = NULL;
static SLEngineItf engineEngine;
// recorder interfaces
static SLObjectItf recorderObject = NULL;
static SLRecordItf recorderRecord;
static SLAndroidSimpleBufferQueueItf recorderBufferQueue;

// 5 seconds of recorded audio at 16 kHz mono, 16-bit signed little endian
const int SRATE=16000;
const int NBUF=2;
const int BUFFER_LEN = SRATE;
static short recBuffer[NBUF][BUFFER_LEN];

// create the engine and output mix objects
void createEngine()
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
void bqRecorderCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
    xassert(bq == recorderBufferQueue);
    xassert(NULL == context);
    // for streaming recording, here we would call Enqueue to give recorder the next buffer to fill
    // but instead, this is a one-time buffer so we stop recording
	SLAndroidSimpleBufferQueueState state;
    SLresult res = (*bq) ->GetState( bq, &state );
    xassert(SL_RESULT_SUCCESS == res);
	xassert( state.index == ++last_buffer_index );

	const int current = (state.index - 1) % NBUF;

	setBuffer( recBuffer[current], BUFFER_LEN);


	LOGI("Recording Finished: %i", state.index );
	res = (*recorderBufferQueue)->Enqueue(recorderBufferQueue, recBuffer[current], sizeof(recBuffer[0]) );
    xassert(SL_RESULT_SUCCESS == res);
}


// create audio recorder
jboolean createAudioRecorder()
{
    SLresult result;

    // configure audio source
    SLDataLocator_IODevice loc_dev = {SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT,
            SL_DEFAULTDEVICEID_AUDIOINPUT, NULL};
    SLDataSource audioSrc = {&loc_dev, NULL};

    // configure audio sink
    SLDataLocator_AndroidSimpleBufferQueue loc_bq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
    SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM, 1, SRATE*1000,
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

    return JNI_TRUE;
}


extern "C"
void Java_anofax_opusrecord_Native_startRecorder(JNIEnv* env, jclass clazz )
{
	xassert( recorderObject == NULL );
	LOGI("starting ...");
	createAudioRecorder();
    SLresult result;

    xassert( sizeof(recBuffer[0] ) == 32000 );
    for( int i=0; i<NBUF; ++i ){
    	result = (*recorderBufferQueue)->Enqueue(recorderBufferQueue, recBuffer[i], sizeof(recBuffer[i]) );
    	xassert(SL_RESULT_SUCCESS == result);
    }

    // start recording
    result = (*recorderRecord)->SetRecordState(recorderRecord, SL_RECORDSTATE_RECORDING);
    xassert(SL_RESULT_SUCCESS == result);
    last_buffer_index = 0;
	LOGI("Recorder Started.");
}

extern "C"
void Java_anofax_opusrecord_Native_initAudio(JNIEnv* env, jclass clazz )
{
	createEngine();
	LOGI("Audio init");
}

extern "C"
void Java_anofax_opusrecord_Native_stopRecorder(JNIEnv* env, jclass clazz )
{
	LOGI("stopping ...");
    (*recorderObject)->Destroy(recorderObject);
    recorderObject = NULL;
    recorderRecord = NULL;
    recorderBufferQueue = NULL;
    setBuffer( 0, 0 );
	LOGI("Recorder stopped");
}

