#ifndef XLOG_H_
#define XLOG_H_

#include <android/log.h>

#ifdef __cplusplus
extern "C"
#endif
void android_log(android_LogPriority type, const char *fmt, ...);
#define LOGI(...) android_log(ANDROID_LOG_INFO, __VA_ARGS__)
#define LOGW(...) android_log(ANDROID_LOG_WARN, __VA_ARGS__)
#define LOGE(...) android_log(ANDROID_LOG_ERROR, __VA_ARGS__)


#ifdef __cplusplus
extern "C"
#endif
void xassert_failed(const char *, int, const char *, const char *);

#define	xassert(e)	((e) ? (void)0 : xassert_failed(__FILE__, __LINE__, __func__, #e))
#endif /* XLOG_H_ */
