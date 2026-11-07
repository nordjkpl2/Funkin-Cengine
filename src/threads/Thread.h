/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Threads: camada de plataforma (criar, esperar, contar nucleos). Nao inclui raylib. */
#pragma once
#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

typedef struct Thread Thread;
typedef void (*ThreadFn)(void* arg);

// Cria e inicia uma thread. NULL se falhar.
Thread* Thread_Start(ThreadFn fn, void* arg);
// Espera a thread terminar e libera o handle.
void Thread_Join(Thread* t);
// Nucleos logicos da maquina (minimo 1).
int Thread_CoreCount(void);

#endif
