/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Threads: Windows (API nativa, sem dependencias) e POSIX (pthread) para testes em outros sistemas. */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdlib.h>
#include "Thread.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

struct Thread {
    HANDLE h;
    ThreadFn fn;
    void* arg;
};

static DWORD WINAPI Thread_Entry(LPVOID p) {
    struct Thread* t = (struct Thread*)p;
    t->fn(t->arg);
    return 0;
}

Thread* Thread_Start(ThreadFn fn, void* arg) {
    struct Thread* t = (struct Thread*)malloc(sizeof(*t));
    if(t == NULL)
        return NULL;
    t->fn = fn;
    t->arg = arg;
    t->h = CreateThread(NULL, 0, Thread_Entry, t, 0, NULL);
    if(t->h == NULL) {
        free(t);
        return NULL;
    }
    return t;
}

void Thread_Join(Thread* t) {
    if(t == NULL)
        return;
    WaitForSingleObject(t->h, INFINITE);
    CloseHandle(t->h);
    free(t);
}

int Thread_CoreCount(void) {
    DWORD n = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    return (n > 0) ? (int)n : 1;
}

#else
#include <pthread.h>
#include <unistd.h>

struct Thread {
    pthread_t h;
    ThreadFn fn;
    void* arg;
};

static void* Thread_Entry(void* p) {
    struct Thread* t = (struct Thread*)p;
    t->fn(t->arg);
    return NULL;
}

Thread* Thread_Start(ThreadFn fn, void* arg) {
    struct Thread* t = (struct Thread*)malloc(sizeof(*t));
    if(t == NULL)
        return NULL;
    t->fn = fn;
    t->arg = arg;
    if(pthread_create(&t->h, NULL, Thread_Entry, t) != 0) {
        free(t);
        return NULL;
    }
    return t;
}

void Thread_Join(Thread* t) {
    if(t == NULL)
        return;
    pthread_join(t->h, NULL);
    free(t);
}

int Thread_CoreCount(void) {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return (n > 0) ? (int)n : 1;
}
#endif
