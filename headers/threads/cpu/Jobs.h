/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Jobs: tarefas em threads de fundo (CPU). Nao inclui raylib e nao conhece o jogo.
 *
 * Regras:
 *  - JobFn roda numa thread de fundo: nunca chamar GPU/raylib de desenho nem ler/escrever estado global do jogo.
 *    Recebe 'data', faz o trabalho e deixa o resultado dentro de 'data'.
 *  - JobDone roda na thread principal, dentro de Jobs_Poll(): e aqui que se cria a textura na GPU.
 *  - 'data' pertence a quem submeteu; liberar no JobDone (ou depois dele).
 */
#pragma once
#ifndef THREADS_CPU_JOBS_H
#define THREADS_CPU_JOBS_H

#define JOBS_MAX_THREADS 64

typedef void (*JobFn)(void* data);   // thread de fundo
typedef void (*JobDone)(void* data); // thread principal (pode ser NULL)

// Liga o sistema. maxThreads <= 0 = automatico (nucleos - 1, minimo 1).
void Jobs_Init(int maxThreads);
// Muda o limite de threads em tempo de execucao (<= 0 = automatico). Threads a mais saem quando ficam ociosas.
void Jobs_SetThreads(int maxThreads);
// Envia uma tarefa. 1 = enfileirada, 0 = falhou (sistema desligado ou sem memoria).
int Jobs_Submit(JobFn fn, void* data, JobDone done);
// Chamar uma vez por frame na thread principal: executa os JobDone das tarefas concluidas.
void Jobs_Poll(void);
// Desliga: espera as tarefas em execucao, descarta as da fila (sem chamar JobDone).
void Jobs_Shutdown(void);

// Estatisticas (para o overlay e teste de estresse)
int Jobs_ThreadCount(void);              // limite atual de threads
int Jobs_Pending(void);                  // enviadas e ainda sem JobDone chamado
unsigned long long Jobs_Completed(void); // total com JobDone ja chamado

#endif
