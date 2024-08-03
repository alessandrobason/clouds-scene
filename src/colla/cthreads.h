#pragma once

#include "collatypes.h"

// == THREAD ===========================================

typedef uintptr_t cthread_t;

typedef int (*cthread_func_t)(void *);

cthread_t thrCreate(cthread_func_t func, void *arg);
bool thrValid(cthread_t ctx);
bool thrDetach(cthread_t ctx);

cthread_t thrCurrent(void);
int thrCurrentId(void);
int thrGetId(cthread_t ctx);

void thrExit(int code);
bool thrJoin(cthread_t ctx, int *code);

// == MUTEX ============================================

typedef uintptr_t cmutex_t;

cmutex_t mtxInit(void);
void mtxFree(cmutex_t ctx);

bool mtxValid(cmutex_t ctx);

bool mtxLock(cmutex_t ctx);
bool mtxTryLock(cmutex_t ctx);
bool mtxUnlock(cmutex_t ctx);

// == CONDITION VARIABLE ===============================

typedef uintptr_t condvar_t;

#define COND_WAIT_INFINITE 0xFFFFFFFF

condvar_t condInit(void);
void condFree(condvar_t cond);

void condWake(condvar_t cond);
void condWakeAll(condvar_t cond);

void condWait(condvar_t cond, cmutex_t mtx);
void condWaitTimed(condvar_t cond, cmutex_t mtx, int milliseconds);
