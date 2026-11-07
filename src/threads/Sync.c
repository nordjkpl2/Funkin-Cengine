/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Sincronizacao: Windows (SRWLOCK + CONDITION_VARIABLE) e POSIX (pthread). */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdlib.h>
#include "Sync.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

struct Mutex { SRWLOCK l; };
struct Cond { CONDITION_VARIABLE c; };

Mutex* Mutex_New(void) {
    Mutex* m = (Mutex*)malloc(sizeof(*m));
    if(m != NULL)
        InitializeSRWLock(&m->l);
    return m;
}
void Mutex_Free(Mutex* m) { free(m); }
void Mutex_Lock(Mutex* m) { AcquireSRWLockExclusive(&m->l); }
void Mutex_Unlock(Mutex* m) { ReleaseSRWLockExclusive(&m->l); }

Cond* Cond_New(void) {
    Cond* c = (Cond*)malloc(sizeof(*c));
    if(c != NULL)
        InitializeConditionVariable(&c->c);
    return c;
}
void Cond_Free(Cond* c) { free(c); }
void Cond_Wait(Cond* c, Mutex* m) { SleepConditionVariableSRW(&c->c, &m->l, INFINITE, 0); }
void Cond_Signal(Cond* c) { WakeConditionVariable(&c->c); }
void Cond_Broadcast(Cond* c) { WakeAllConditionVariable(&c->c); }

#else
#include <pthread.h>

struct Mutex { pthread_mutex_t m; };
struct Cond { pthread_cond_t c; };

Mutex* Mutex_New(void) {
    Mutex* m = (Mutex*)malloc(sizeof(*m));
    if(m != NULL)
        pthread_mutex_init(&m->m, NULL);
    return m;
}
void Mutex_Free(Mutex* m) {
    if(m != NULL) {
        pthread_mutex_destroy(&m->m);
        free(m);
    }
}
void Mutex_Lock(Mutex* m) { pthread_mutex_lock(&m->m); }
void Mutex_Unlock(Mutex* m) { pthread_mutex_unlock(&m->m); }

Cond* Cond_New(void) {
    Cond* c = (Cond*)malloc(sizeof(*c));
    if(c != NULL)
        pthread_cond_init(&c->c, NULL);
    return c;
}
void Cond_Free(Cond* c) {
    if(c != NULL) {
        pthread_cond_destroy(&c->c);
        free(c);
    }
}
void Cond_Wait(Cond* c, Mutex* m) { pthread_cond_wait(&c->c, &m->m); }
void Cond_Signal(Cond* c) { pthread_cond_signal(&c->c); }
void Cond_Broadcast(Cond* c) { pthread_cond_broadcast(&c->c); }
#endif
