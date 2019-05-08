#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef HANDLE pthread_t;
typedef void pthread_attr_t;

typedef struct __pthread_thread_param {
  void *(*start_routine) (void *);
  void *arg;
} __pthread_thread_param_t;

DWORD WINAPI pthread_thread_routine(LPVOID lpParam) {
  __pthread_thread_param_t *param = (__pthread_thread_param_t*)lpParam;
  param->start_routine(param->arg);
  free(param);
  return 0;
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                  void *(*start_routine) (void *), void *arg) {
  __pthread_thread_param_t *param = (__pthread_thread_param_t*)malloc(sizeof(__pthread_thread_param_t));
  param->start_routine = start_routine;
  param->arg = arg;
  *thread = CreateThread(NULL, 0, pthread_thread_routine, param, 0, NULL);
  if (*thread == INVALID_HANDLE_VALUE || *thread == 0)
  	return -1;

  return 0;
}

int pthread_join(pthread_t thread, void **ret) {
	WaitForSingleObject(thread, INFINITE);
}

#ifdef __cplusplus
}
#endif
