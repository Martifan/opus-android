#include <jni.h>
#include "xlog.h"
#include "recorder.h"

extern "C"{
#include "opusenc/xopus_file_encode.h"
}

static int total;

long getBuffer(void *src, short *buffer, int n)
{
	return getSamples( buffer, n, total );
}

extern "C"
int Java_anofax_opusrecord_Native_encode(JNIEnv* env, jclass clazz, jstring s, int srate )
{
	total = REC_BUFFER_INIT;
	LOGI("starting Encoder");
	const char* outFile = env->GetStringUTFChars( s, 0 );
	LOGI("Started Encoding: %s", outFile );
	opus_file_encode( outFile, srate, 1, 0, 16, getBuffer );
	LOGI("Stopped encoding");
	env->ReleaseStringUTFChars( s, outFile );
	LOGI("finished Encoding");
	return 0;
}
