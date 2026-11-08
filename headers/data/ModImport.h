#pragma once
#ifndef MODIMPORT_H
#define MODIMPORT_H

#include <stddef.h>

typedef struct ModImportStats {
    int songs;
    int charts;
    int weeks;
    int stages;
    int images;
    int characters;
    int animsets;
    int icons;
    long long bytes;
    int skippedEvents;
    int skippedLua;
} ModImportStats;

/* arquivos pulados no ultimo ModImport_Run (mesmo caminho e tamanho no destino) */
int ModImport_SkippedDuplicates(void);

typedef struct ModImportUsage {
    int mods;
    int songs;
    long long bytes;
} ModImportUsage;

/* srcMod e uma pasta de mod-fonte; dstRoot deve ser exatamente assets/mods.
 * Se srcMod/modName for "mods", importa o conteudo diretamente em dstRoot.
 * Se essa pasta for um container, importa cada mod filho em dstRoot/<modName>.
 */
char ModImport_Run(const char *srcMod, const char *dstRoot,
                   const char *modName, char *msg, size_t msgSize,
                   ModImportStats *stats);

/* Import em segundo plano: a copia roda numa thread propria e os arquivos grandes
 * (>= 2 MB) vao para o Jobs em paralelo.
 * Start: 0 se ja ha um import rodando. Poll (todo frame): atualiza prog e devolve 1
 * quando terminou (msg, stats e *ok preenchidos). Cancel: para o mais rapido possivel;
 * o que ja foi copiado fica (reimportar pula os arquivos iguais). */
typedef struct ModImportProgress {
    int filesDone;
    int filesTotal;       /* arquivos na pasta de origem (estimativa; pode nao chegar a 100%) */
    long long bytesDone;
    long long bytesTotal;
    int failed;
    int duplicates;
    char cancelling;
} ModImportProgress;

char ModImport_Start(const char *srcMod, const char *dstRoot, const char *modName);
char ModImport_Busy(void);
void ModImport_Shutdown(void); /* cancela e espera; chamar antes de Jobs_Shutdown */
void ModImport_Cancel(void);
char ModImport_Poll(ModImportProgress *prog, char *msg, size_t msgSize,
                    ModImportStats *stats, char *ok);

/* Lista o conteudo de assets/mods sem incluir a propria pasta raiz. */
char ModImport_ScanMods(const char *dstRoot, ModImportUsage *out);

/* Mesma varredura, em thread de fundo (Jobs). Nao trava a tela.
 * Start: dispara (se ja houver uma rodando, refaz ao terminar).
 * Poll: chamar a cada frame; devolve 1 quando uma varredura terminou (out preenchido).
 * Busy: 1 enquanto estiver varrendo. */
char ModImport_ScanStart(const char *dstRoot);
char ModImport_ScanPoll(ModImportUsage *out);
char ModImport_ScanBusy(void);

/* Remove o conteudo de assets/mods, preservando a pasta raiz. */
char ModImport_DeleteAll(const char *dstRoot, char *msg, size_t msgSize,
                         int *removedMods, long long *freedBytes);

#endif /* MODIMPORT_H */
