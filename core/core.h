#ifndef CORE_H_
#define CORE_H_
//#include <windows.h>

#if defined(WIN32) || defined(_WIN32) 
#define EXPORT_FUNC __declspec(dllexport)
#else
#define EXPORT_FUNC 
#endif

#ifdef __cplusplus
extern "C" {
#endif
	EXPORT_FUNC int getOne();

#ifdef __cplusplus
	}
#endif
#endif // CORE_H_