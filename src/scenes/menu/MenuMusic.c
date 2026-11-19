/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * MenuMusic.c - faixas da musica de menu (assets/music/menu). Escolha salva em GameData.
 */
#include "scenes/menu/MenuMusic.h"
#include "Game.h"
#include "GameData.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MENU_MUSIC_DIR "assets/music/menu"
#define MENU_MUSIC_MAX 64
#define MENU_MUSIC_VOLUME 0.3f

typedef struct {
    char file[MENU_MUSIC_NAME_MAX]; /* sem extensao */
    char label[128];                /* "Titulo by Autor" */
} MenuTrack;

static MenuTrack tracks[MENU_MUSIC_MAX];
static int trackCount;

static int CompareTracks(const void* a, const void* b) {
    return strcmp(((const MenuTrack*)a)->file, ((const MenuTrack*)b)->file);
}

/* Copia trocando '_' por espaco. */
static void CopySpaced(char* out, size_t size, const char* from, size_t len) {
    if (len >= size) len = size - 1;
    for (size_t i = 0; i < len; i++) out[i] = from[i] == '_' ? ' ' : from[i];
    out[len] = 0;
}

static void BuildLabel(MenuTrack* t) {
    const char* by = strstr(t->file, "_by_");
    if (by == NULL) {
        CopySpaced(t->label, sizeof(t->label), t->file, strlen(t->file));
        return;
    }

    char title[96], author[64];
    CopySpaced(title, sizeof(title), t->file, (size_t)(by - t->file));
    CopySpaced(author, sizeof(author), by + 4, strlen(by + 4));
    (void)snprintf(t->label, sizeof(t->label), "%s by %s", title, author);
}

void MenuMusic_Init(void) {
    trackCount = 0;

    FilePathList list = LoadDirectoryFilesEx(MENU_MUSIC_DIR, ".ogg", false);
    for (unsigned int i = 0; i < list.count && trackCount < MENU_MUSIC_MAX; i++) {
        MenuTrack* t = &tracks[trackCount];
        (void)snprintf(t->file, sizeof(t->file), "%s", GetFileNameWithoutExt(list.paths[i]));
        BuildLabel(t);
        trackCount++;
    }
    UnloadDirectoryFiles(list);

    qsort(tracks, (size_t)trackCount, sizeof(MenuTrack), CompareTracks);
    MenuMusic_RestoreSaved();
}

int MenuMusic_Count(void) { return trackCount; }

const char* MenuMusic_Label(int index) { return tracks[index].label; }

const char* MenuMusic_FileName(int index) { return tracks[index].file; }

int MenuMusic_SavedIndex(void) {
    for (int pass = 0; pass < 2; pass++) {
        const char* wanted = pass == 0 ? gameOptions.menuMusic : MENU_MUSIC_DEFAULT;
        for (int i = 0; i < trackCount; i++)
            if (wanted[0] != 0 && strcmp(tracks[i].file, wanted) == 0) return i;
    }
    return 0;
}

void MenuMusic_Play(int index, char loop) {
    if (index < 0 || index >= trackCount) return;

    char path[256];
    (void)snprintf(path, sizeof(path), "%s/%s.ogg", MENU_MUSIC_DIR, tracks[index].file);
    RayGame_SetMusic(path, MENU_MUSIC_VOLUME, loop);
}

void MenuMusic_RestoreSaved(void) {
    if (trackCount > 0) MenuMusic_Play(MenuMusic_SavedIndex(), 1);
}
