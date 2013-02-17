#include <jni.h>
#include <string.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <assert.h>
#include <pthread.h>
#include "xlog.h"

extern "C"{
#include "opusenc/xopus_file_encode.h"
}


extern "C"
jint Java_anofax_opusrecord_Native_ntest(JNIEnv* env, jclass clazz, jint a, jstring s )
{
	LOGI("native is super");
	return 12;
	/*
	const JNIEnv e = *env;
	const char* ss = e->GetStringUTFChars( env, s, 0 );
	android_log( "logging %s:", ss);
	optest(ss);
	android_log( "logging %d:", 3);
	*/
}


extern "C"
void Java_anofax_opusrecord_Native_initEncoder(JNIEnv* env, jclass clazz)
{

}

static pthread_mutex_t enc_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t enc_cond = PTHREAD_COND_INITIALIZER;
static int enc_error = 0;
static short* enc_buffer = 0;
static int enc_nbuf = 0;
static clock_t enc_start_clocks;
static clock_t enc_total_clocks;

void setBuffer( short* b, int nb )
{
    pthread_mutex_lock(&enc_mutex);
    if(b == 0 ){  // Stop recording
    	enc_error = 2;
    } else if( enc_buffer != 0 ){ // we only touch things when enc_buffer==0
    	enc_error = 1;
    	LOGW("Buffer overflow");
    } else {  // set the new buffer
    	enc_buffer = b;
    	enc_nbuf = nb;
    }
	LOGI("Set buffer %i", nb);
    pthread_cond_signal(&enc_cond);
    pthread_mutex_unlock(&enc_mutex);
}


long getBuffer(void *src, short *buffer, int n)
{
	if( enc_nbuf == 0 ){
		LOGI("encoder: empty");
		enc_total_clocks += (clock() - enc_start_clocks );
		pthread_mutex_lock(&enc_mutex);  // we only need the lock when empty
		enc_buffer = 0;
		if( enc_error == 0 ){
			pthread_cond_wait(&enc_cond, &enc_mutex);
		}
	    pthread_mutex_unlock(&enc_mutex);
	    enc_start_clocks = clock();
	}
	if( enc_buffer == 0 )
		return 0;
	if( enc_nbuf < n ){
		LOGE("Invalid buffer size");
		enc_error=3;
		return 0;
	}
	memcpy( buffer, enc_buffer, n*sizeof(*buffer) );
	enc_nbuf -= n;
	enc_buffer += n;
	return n;
}


extern "C"
int Java_anofax_opusrecord_Native_encode(JNIEnv* env, jclass clazz, jstring s )
{
	enc_total_clocks = 0;
	enc_start_clocks = clock();
	const char* outFile = env->GetStringUTFChars( s, 0 );
	LOGI("Started Encoding: %s", outFile );
	opus_file_encode( outFile, 16000, 1, 0, 16, getBuffer );
	LOGI("Stopped encoding: %i", enc_error);
	LOGI("Encoder Run time: %f s", enc_total_clocks / float(CLOCKS_PER_SEC) );
	int res = enc_error;
	enc_error = 0;
	enc_buffer = 0;
	enc_nbuf = 0;
	env->ReleaseStringUTFChars( s, outFile );
	return res;
}

