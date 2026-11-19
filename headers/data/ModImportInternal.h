/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * ModImportInternal.h - Tipos, constantes e estado compartilhado pelos modulos de importacao de mod.
 */

#pragma once
#ifndef MODIMPORT_INTERNAL_H
#define MODIMPORT_INTERNAL_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>

#include "data/ModImport.h"
#include "yyjson.h"

#ifdef _WIN32
    #include <direct.h>
    #define MI_MKDIR(path) _mkdir(path)
    #define MI_RMDIR(path) _rmdir(path)
#else
    #include <sys/stat.h>
    #include <unistd.h>
    #define MI_MKDIR(path) mkdir((path), 0777)
    #define MI_RMDIR(path) rmdir(path)
#endif

#define MI_PATH_MAX          1024
#define MI_NAME_MAX          128
#define MI_MAX_XML_FRAMES    16384
#define MI_MAX_CHAR_ANIMS    64
#define MI_MAX_ANIM_FRAMES   256
#define MI_MAX_ICON_NAMES    128
#define MI_MAX_DEPTH         32
#define MI_COPY_BUFFER       65536u


/* Formato binario consumido pelo AnimationSet_LoadAnimations():
 * int32 animationCount; por animacao: int32 nameLength, name bytes,
 * int32 dataCount (= frames * 6), seguido por seis int32 por frame.
 */
typedef struct MI_XmlFrame {
    char name[128];
    int32_t x, y, w, h, fx, fy;
} MI_XmlFrame;

typedef struct MI_CharAnim {
    char role[32];
    char prefix[64];
    int32_t indices[MI_MAX_ANIM_FRAMES];
    int indexCount;
} MI_CharAnim;

typedef struct MI_CharInfo {
    char healthicon[64];
    char imagePath[192];
    MI_CharAnim anims[MI_MAX_CHAR_ANIMS];
    int animCount;
} MI_CharInfo;

typedef struct MI_FrameCandidate {
    int frameIndex;
    uint64_t order;
} MI_FrameCandidate;

typedef struct MI_NameList {
    char names[MI_MAX_ICON_NAMES][64];
    int count;
} MI_NameList;

typedef struct MI_ImportContext {
    const char *srcRoot;
    const char *dstRoot;
    ModImportStats *stats;
    MI_NameList iconNames;
} MI_ImportContext;

extern atomic_int mi_dupSkipped;
extern atomic_int mi_cancel;
extern atomic_int mi_useDefer;
extern atomic_int mi_failed;
extern atomic_int mi_filesDone;
extern atomic_llong mi_bytesDone;
extern atomic_llong mi_deferredBytes;

#endif /* MODIMPORT_INTERNAL_H */
