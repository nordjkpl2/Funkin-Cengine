/* DirWalk: varredura rapida de pastas. Nao inclui raylib e e thread-safe (pode rodar em Jobs).
 * Caminhos em UTF-8. Links simbolicos/juncoes de pasta sao ignorados (sem laco infinito).
 * Windows: o tamanho vem da propria listagem (FindFirstFileEx), sem abrir cada arquivo.
 * Outros SO: readdir + lstat.
 */
#pragma once
#ifndef CORE_DIRWALK_H
#define CORE_DIRWALK_H

typedef struct DirWalkTotals {
    long long bytes;
    int files;
    int dirs;
} DirWalkTotals;

/* Soma tamanho e quantidade de tudo abaixo de path (ate maxDepth niveis).
 * Retorna 1 se a pasta pode ser lida, 0 se nao (out zerado). */
int DirWalk_Totals(const char* path, int maxDepth, DirWalkTotals* out);

/* Chama cb(nome, user) para cada subpasta direta (nome sem caminho).
 * cb retorna != 0 para parar. Retorna 1 se a pasta pode ser lida. */
typedef int (*DirWalkNameFn)(const char* name, void* user);
int DirWalk_ForEachDir(const char* path, DirWalkNameFn cb, void* user);

#endif
