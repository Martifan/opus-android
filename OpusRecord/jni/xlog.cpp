#include <jni.h>
#include <string.h>
#include <stdio.h>
#include "xlog.h"
void android_log(android_LogPriority type, const char *fmt, ...)
{
    static char buf[1024];
    static char *bp = buf;

    va_list vl;
    va_start(vl, fmt);
    int available = sizeof(buf) - (bp - buf);
    int nout = vsnprintf(bp, available, fmt, vl);
    __android_log_write(type, "native", buf);
    va_end(vl);
}

void xassert_failed(const char * file , int line, const char * func, const char * expression)
{
	static char buf[1024];
	snprintf( buf, sizeof(buf), "XASSERT FAILED\n%s : %i\n %s \n %s", file, line, func, expression );
    __android_log_write(ANDROID_LOG_ERROR, "xassert", buf);
}


