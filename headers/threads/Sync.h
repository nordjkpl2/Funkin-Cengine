/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Sincronizacao: mutex e variavel de condicao. Nao inclui raylib. */
#pragma once
#ifndef THREADS_SYNC_H
#define THREADS_SYNC_H

typedef struct Mutex Mutex;
typedef struct Cond Cond;

Mutex* Mutex_New(void);
void Mutex_Free(Mutex* m);
void Mutex_Lock(Mutex* m);
void Mutex_Unlock(Mutex* m);

Cond* Cond_New(void);
void Cond_Free(Cond* c);
// Solta o mutex, espera o sinal e pega o mutex de volta (pode acordar sem sinal: sempre rechecar a condicao).
void Cond_Wait(Cond* c, Mutex* m);
void Cond_Signal(Cond* c);
void Cond_Broadcast(Cond* c);

#endif
