/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Jobs: fila de tarefas + pool de threads trabalhadoras. */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "Jobs.h"
#include "threads/Sync.h"
#include "threads/Thread.h"

typedef struct JobNode {
    JobFn fn;
    JobDone done;
    void* data;
    struct JobNode* next;
} JobNode;

typedef struct {
    JobNode* head;
    JobNode* tail;
} JobList;

static Mutex* mtx = NULL;
static Cond* cv = NULL;
static JobList todo;     // esperando uma thread
static JobList finished; // prontas para o JobDone
static Thread* workers[JOBS_MAX_THREADS];
static char alive[JOBS_MAX_THREADS]; // 1 = thread ainda no loop (protegido por mtx)
static int target = 0;               // quantas threads podem trabalhar
static int stopping = 0;
static int pending = 0;
static unsigned long long completed = 0;

static void List_Push(JobList* l, JobNode* n) {
    n->next = NULL;
    if(l->tail != NULL)
        l->tail->next = n;
    else
        l->head = n;
    l->tail = n;
}

static JobNode* List_Pop(JobList* l) {
    JobNode* n = l->head;
    if(n != NULL) {
        l->head = n->next;
        if(l->head == NULL)
            l->tail = NULL;
        n->next = NULL;
    }
    return n;
}

static void Worker_Loop(void* arg) {
    int idx = (int)(intptr_t)arg;
    Mutex_Lock(mtx);
    for(;;) {
        if(stopping || idx >= target)
            break;
        JobNode* j = List_Pop(&todo);
        if(j == NULL) {
            Cond_Wait(cv, mtx);
            continue;
        }
        Mutex_Unlock(mtx);
        if(j->fn != NULL)
            j->fn(j->data);
        Mutex_Lock(mtx);
        List_Push(&finished, j);
    }
    alive[idx] = 0;
    Mutex_Unlock(mtx);
}

// com mtx travado: garante a thread 'idx' rodando
static void Worker_Ensure(int idx) {
    if(workers[idx] != NULL && alive[idx])
        return; // ainda rodando (vai ver o novo target)
    if(workers[idx] != NULL) {
        Thread_Join(workers[idx]); // ja saiu do loop
        workers[idx] = NULL;
    }
    alive[idx] = 1;
    workers[idx] = Thread_Start(Worker_Loop, (void*)(intptr_t)idx);
    if(workers[idx] == NULL)
        alive[idx] = 0;
}

static int Jobs_ResolveCount(int n) {
    if(n <= 0)
        n = Thread_CoreCount() - 1;
    if(n < 1)
        n = 1;
    if(n > JOBS_MAX_THREADS)
        n = JOBS_MAX_THREADS;
    return n;
}

void Jobs_Init(int maxThreads) {
    if(mtx != NULL)
        return;
    mtx = Mutex_New();
    cv = Cond_New();
    if(mtx == NULL || cv == NULL) {
        Mutex_Free(mtx);
        Cond_Free(cv);
        mtx = NULL;
        cv = NULL;
        return;
    }
    stopping = 0;
    Jobs_SetThreads(maxThreads);
}

void Jobs_SetThreads(int maxThreads) {
    if(mtx == NULL)
        return;
    int n = Jobs_ResolveCount(maxThreads);
    Mutex_Lock(mtx);
    target = n;
    for(int i = 0; i < n; i++)
        Worker_Ensure(i);
    Cond_Broadcast(cv); // acorda as que precisam sair
    Mutex_Unlock(mtx);
}

int Jobs_Submit(JobFn fn, void* data, JobDone done) {
    if(mtx == NULL || fn == NULL)
        return 0;
    JobNode* n = (JobNode*)malloc(sizeof(*n));
    if(n == NULL)
        return 0;
    n->fn = fn;
    n->done = done;
    n->data = data;
    Mutex_Lock(mtx);
    List_Push(&todo, n);
    pending++;
    Cond_Signal(cv);
    Mutex_Unlock(mtx);
    return 1;
}

void Jobs_Poll(void) {
    if(mtx == NULL)
        return;
    Mutex_Lock(mtx);
    JobNode* list = finished.head;
    finished.head = NULL;
    finished.tail = NULL;
    Mutex_Unlock(mtx);

    int n = 0;
    while(list != NULL) {
        JobNode* j = list;
        list = j->next;
        if(j->done != NULL)
            j->done(j->data);
        free(j);
        n++;
    }
    if(n > 0) {
        Mutex_Lock(mtx);
        pending -= n;
        completed += (unsigned long long)n;
        Mutex_Unlock(mtx);
    }
}

void Jobs_Shutdown(void) {
    if(mtx == NULL)
        return;
    Mutex_Lock(mtx);
    stopping = 1;
    Cond_Broadcast(cv);
    Mutex_Unlock(mtx);
    for(int i = 0; i < JOBS_MAX_THREADS; i++) {
        if(workers[i] != NULL) {
            Thread_Join(workers[i]);
            workers[i] = NULL;
        }
    }
    JobNode* n;
    while((n = List_Pop(&todo)) != NULL)
        free(n);
    while((n = List_Pop(&finished)) != NULL)
        free(n);
    pending = 0;
    target = 0;
    completed = 0;
    Mutex_Free(mtx);
    Cond_Free(cv);
    mtx = NULL;
    cv = NULL;
}

int Jobs_ThreadCount(void) {
    return target;
}

int Jobs_Pending(void) {
    if(mtx == NULL)
        return 0;
    Mutex_Lock(mtx);
    int p = pending;
    Mutex_Unlock(mtx);
    return p;
}

unsigned long long Jobs_Completed(void) {
    if(mtx == NULL)
        return 0;
    Mutex_Lock(mtx);
    unsigned long long c = completed;
    Mutex_Unlock(mtx);
    return c;
}
