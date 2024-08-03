#include "cthreads.h"

#include <stdlib.h>

typedef struct {
    cthread_func_t func;
    void *arg;
} _thr_internal_t;

#if COLLA_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// == THREAD ===========================================

static DWORD _thrFuncInternal(void *arg) {
    _thr_internal_t *params = (_thr_internal_t *)arg;
    cthread_func_t func = params->func;
    void *argument = params->arg;
    free(params);
    return (DWORD)func(argument);
}

cthread_t thrCreate(cthread_func_t func, void *arg) {
    HANDLE thread = INVALID_HANDLE_VALUE;
    _thr_internal_t *params = calloc(1, sizeof(_thr_internal_t));
    
    if(params) {
        params->func = func;
        params->arg = arg;

        thread = CreateThread(NULL, 0, _thrFuncInternal, params, 0, NULL);
    }

    return (cthread_t)thread;
}

bool thrValid(cthread_t ctx) {
    return (HANDLE)ctx != INVALID_HANDLE_VALUE;
}

bool thrDetach(cthread_t ctx) {
    return CloseHandle((HANDLE)ctx);
}

cthread_t thrCurrent(void) {
    return (cthread_t)GetCurrentThread();
}

int thrCurrentId(void) {
    return GetCurrentThreadId();
}

int thrGetId(cthread_t ctx) {
    return GetThreadId((HANDLE)ctx);
}

void thrExit(int code) {
    ExitThread(code);
}

bool thrJoin(cthread_t ctx, int *code) {
    if(!ctx) return false;
    int return_code = WaitForSingleObject((HANDLE)ctx, INFINITE);
    if(code) *code = return_code;
    BOOL success = CloseHandle((HANDLE)ctx);
    return return_code != WAIT_FAILED && success;
}

// == MUTEX ============================================

cmutex_t mtxInit(void) {
    CRITICAL_SECTION *crit_sec = calloc(1, sizeof(CRITICAL_SECTION));
    if(crit_sec) {
        InitializeCriticalSection(crit_sec);
    }
    return (cmutex_t)crit_sec;
}

void mtxFree(cmutex_t ctx) {
    DeleteCriticalSection((CRITICAL_SECTION *)ctx);
    free((CRITICAL_SECTION *)ctx);
}

bool mtxValid(cmutex_t ctx) {
    return (void *)ctx != NULL;
}

bool mtxLock(cmutex_t ctx) {
    EnterCriticalSection((CRITICAL_SECTION *)ctx);
    return true;
}

bool mtxTryLock(cmutex_t ctx) {
    return TryEnterCriticalSection((CRITICAL_SECTION *)ctx);
}

bool mtxUnlock(cmutex_t ctx) {
    LeaveCriticalSection((CRITICAL_SECTION *)ctx);
    return true;
}

// == CONDITION VARIABLE ===============================

#include "tracelog.h"

condvar_t condInit(void) {
    CONDITION_VARIABLE *cond = calloc(1, sizeof(CONDITION_VARIABLE));
    InitializeConditionVariable(cond);
    return (condvar_t)cond;
}

void condFree(condvar_t cond) {
    free((CONDITION_VARIABLE *)cond);
}

void condWake(condvar_t cond) {
    WakeConditionVariable((CONDITION_VARIABLE *)cond);
}

void condWakeAll(condvar_t cond) {
    WakeAllConditionVariable((CONDITION_VARIABLE *)cond);
}

void condWait(condvar_t cond, cmutex_t mtx) {
    SleepConditionVariableCS((CONDITION_VARIABLE *)cond, (CRITICAL_SECTION *)mtx, INFINITE);
}

void condWaitTimed(condvar_t cond, cmutex_t mtx, int milliseconds) {
    SleepConditionVariableCS((CONDITION_VARIABLE *)cond, (CRITICAL_SECTION *)mtx, milliseconds);
}

#else
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>

// == THREAD ===========================================

#define INT_TO_VOIDP(a) ((void *)((uintptr_t)(a)))

static void *_thrFuncInternal(void *arg) {
    _thr_internal_t *params = (_thr_internal_t *)arg;
    cthread_func_t func = params->func;
    void *argument = params->arg;
    free(params);
    return INT_TO_VOIDP(func(argument));
}

cthread_t thrCreate(cthread_func_t func, void *arg) {
    pthread_t handle = (pthread_t)NULL;

    _thr_internal_t *params = calloc(1, sizeof(_thr_internal_t));
    
    if(params) {
        params->func = func;
        params->arg = arg;

        int result = pthread_create(&handle, NULL, _thrFuncInternal, params);
        if(result) handle = (pthread_t)NULL;
    }

    return (cthread_t)handle;
}

bool thrValid(cthread_t ctx) {
    return (void *)ctx != NULL;
}

bool thrDetach(cthread_t ctx) {
    return pthread_detach((pthread_t)ctx) == 0;
}

cthread_t thrCurrent(void) {
    return (cthread_t)pthread_self();
}

int thrCurrentId(void) {
    return (int)pthread_self();
}

int thrGetId(cthread_t ctx) {
    return (int)ctx;
}

void thrExit(int code) {
    pthread_exit(INT_TO_VOIDP(code));
}

bool thrJoin(cthread_t ctx, int *code) {
    void *result = code;
    return pthread_join((pthread_t)ctx, &result) != 0;
}

// == MUTEX ============================================

cmutex_t mtxInit(void) {
    pthread_mutex_t *mutex = calloc(1, sizeof(pthread_mutex_t));

    if(mutex) {
        if(pthread_mutex_init(mutex, NULL)) {
            free(mutex);
            mutex = NULL;
        }
    }

    return (cmutex_t)mutex;
}

void mtxFree(cmutex_t ctx) {
    pthread_mutex_destroy((pthread_mutex_t *)ctx);
}

bool mtxValid(cmutex_t ctx) {
    return (void *)ctx != NULL;
}

bool mtxLock(cmutex_t ctx) {
    return pthread_mutex_lock((pthread_mutex_t *)ctx) == 0;
}

bool mtxTryLock(cmutex_t ctx) {
    return pthread_mutex_trylock((pthread_mutex_t *)ctx) == 0;
}

bool mtxUnlock(cmutex_t ctx) {
    return pthread_mutex_unlock((pthread_mutex_t *)ctx) == 0;
}

// == CONDITION VARIABLE ===============================

condvar_t condInit(void) {
    pthread_cond_t *cond = calloc(1, sizeof(pthread_cond_t));

    if(cond) {
        if(pthread_cond_init(cond, NULL)) {
            free(cond);
            cond = NULL;
        }
    }

    return (condvar_t)cond;
}

void condFree(condvar_t cond) {
    if (!cond) return;
    pthread_cond_destroy((pthread_cond_t *)cond);
    free((pthread_cond_t *)cond);
}

void condWake(condvar_t cond) {
    pthread_cond_signal((pthread_cond_t *)cond);
}

void condWakeAll(condvar_t cond) {
    pthread_cond_broadcast((pthread_cond_t *)cond);
}

void condWait(condvar_t cond, cmutex_t mtx) {
    pthread_cond_wait((pthread_cond_t *)cond, (pthread_mutex_t *)mtx);
}

void condWaitTimed(condvar_t cond, cmutex_t mtx, int milliseconds) {
    struct timespec timeout;
    time(&timeout.tv_sec);
    timeout.tv_nsec += milliseconds * 1000000;
    pthread_cond_timedwait((pthread_cond_t *)cond, (pthread_mutex_t *)mtx, &timeout);
}

#endif
