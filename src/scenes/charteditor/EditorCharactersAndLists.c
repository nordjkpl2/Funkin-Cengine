/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: listas de personagens, stages e noteskins (varredura de pastas) e as caixas ligadas a elas. */
#include"scenes/AllScenes.h"
#include"scenes/Song.h"
#include"scenes/SongList.h"
#include "scenes/ModPath.h"
#include "scenes/Character.h"
#include"scenes/Stage.h"
#include"scenes/Note.h"
#include"yyjson.h"
#include "Game.h"
#include "GameData.h"
#include "raymath.h"
#include "EditorDataAndIO.h"
#include "EditorTimeline.h"
#include "EditorAudio.h"
#include "EditorWidgets.h"
#include "EditorGrid.h"
#include "EditorCharactersAndLists.h"

SongEntry* pickerSongs;

int pickerCount;

char charNames[NAME_LIST_MAX][SONG_NAME_MAX];

int charCount = 0;

// lista de girlfriends (so precisa de png + animset, nao tem icone)
char gfNames[NAME_LIST_MAX][SONG_NAME_MAX];

int gfCount = 0;

// stages que existem nas musicas (pro seletor de Stage)
char stageNames[32][SONG_NAME_MAX];

int stageCount = 0;

static char StageOfDir(const char* dir, char out[SONG_NAME_MAX]) {
    char p[300];
    snprintf(p, sizeof(p), "%s/data.json", dir);
    FILE* f = fopen(p, "rb");
    if(f != NULL) {
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        if(size > 0 && size < 8 * 1024 * 1024) {
            char* buf = malloc((size_t)size + 1);
            if(buf != NULL) {
                if(fread(buf, 1, (size_t)size, f) == (size_t)size) {
                    buf[size] = 0;
                    yyjson_doc* doc = yyjson_read(buf, (size_t)size, 0);
                    free(buf);
                    if(doc != NULL) {
                        yyjson_val* root = yyjson_doc_get_root(doc);
                        if(root != NULL) {
                            yyjson_val* js = yyjson_obj_get(root, "song");
                            if(js == NULL) js = root;
                            yyjson_val* jst = yyjson_obj_get(js, "stage");
                            if(yyjson_is_str(jst)) {
                                const char* s = yyjson_get_str(jst);
                                strncpy(out, s, SONG_NAME_MAX - 1);
                                out[SONG_NAME_MAX - 1] = 0;
                                yyjson_doc_free(doc);
                                fclose(f);
                                return 1;
                            }
                        }
                        yyjson_doc_free(doc);
                    }
                } else {
                    free(buf);
                }
            }
        }
        fclose(f);
    }
    // binario: pula 2 strings, le a 3a (stage)
    snprintf(p, sizeof(p), "%s/data.song", dir);
    f = fopen(p, "rb");
    if(f == NULL)
        return 0;
    for(int s = 0; s < 3; s++) {
        int len = 0;
        if(fread(&len, sizeof(int), 1, f) != 1 || len < 1 || len > SONG_NAME_MAX - 1) {
            fclose(f);
            return 0;
        }
        char tmp[SONG_NAME_MAX];
        if(fread(tmp, 1, (size_t)len, f) != (size_t)len) {
            fclose(f);
            return 0;
        }
        tmp[len] = 0;
        if(s == 2) {
            strncpy(out, tmp, SONG_NAME_MAX - 1);
            out[SONG_NAME_MAX - 1] = 0;
        }
    }
    fclose(f);
    return 1;
}

// raiz do mod de um songDir (assets/mods/<mod>/...), "" = base
void EdModRoot(const char* songDir, char out[256]) {
    (void)ModPath_Root(songDir, out, 256);
}

// so entra na lista quem tem png + animset + icone (sem fantasma).
// fallback de preview/load continua bf/dad via Editor_TryIcon e Character_Load.
static char CharacterAsset_Exists(const char* name) {
    if(name == NULL || name[0] == 0) return 0;
    if(strchr(name, '/') != NULL || strchr(name, '\\') != NULL || strstr(name, "..") != NULL) return 0;
    char p[300];
    snprintf(p, sizeof(p), "assets/images/characters/%s.png", name);
    FILE* f = fopen(p, "rb");
    if(f == NULL) return 0;
    fclose(f);
    snprintf(p, sizeof(p), "assets/images/characters/%s.animset", name);
    f = fopen(p, "rb");
    if(f == NULL) return 0;
    fclose(f);
    return 1;   // icone nao e exigido: sem ele o jogo usa o do bf/dad
}

// vale no mod da musica (png+animset no mod) ou no global
char CharacterAssetForSong_Exists(const char* name, const char* songDir) {
    if(CharacterAsset_Exists(name))
        return 1;
    if(name == NULL || name[0] == 0)
        return 0;
    if(strchr(name, '/') != NULL || strchr(name, '\\') != NULL || strstr(name, "..") != NULL)
        return 0;
    char modRoot[256];
    EdModRoot(songDir, modRoot);
    if(modRoot[0] == 0)
        return 0;
    char p[300];
    snprintf(p, sizeof(p), "%s/images/characters/%s.png", modRoot, name);
    FILE* f = fopen(p, "rb");
    if(f == NULL)
        return 0;
    fclose(f);
    snprintf(p, sizeof(p), "%s/images/characters/%s.animset", modRoot, name);
    f = fopen(p, "rb");
    if(f == NULL)
        return 0;
    fclose(f);
    return 1;
}

// girlfriend vale se tem png + animset (global ou no mod da musica); "none"/"nogf" = sem girlfriend
char GirlfriendAsset_Exists(const char* name, const char* songDir) {
    if(name == NULL || name[0] == 0)
        return 0;
    if(strcmp(name, "none") == 0 || strcmp(name, "nogf") == 0)
        return 1;
    if(strchr(name, '/') != NULL || strchr(name, '\\') != NULL || strstr(name, "..") != NULL)
        return 0;
    char modRoot[256];
    EdModRoot(songDir, modRoot);
    const char* roots[2] = {"assets", modRoot};
    for(int i = 0; i < 2; i++) {
        if(roots[i][0] == 0)
            continue;
        char png[300], animset[300];
        snprintf(png, sizeof(png), "%s/images/characters/%s.png", roots[i], name);
        snprintf(animset, sizeof(animset), "%s/images/characters/%s.animset", roots[i], name);
        if(FileExists(png) && FileExists(animset))
            return 1;
    }
    return 0;
}

// json: assets/stages/<nome>.json, assets/data/stages/<nome>.json.
// "stage" builtin continua valido. Fantasmas sem arquivo caem aqui.
static char StageJsonGlobal_Exists(const char* name) {
    char p[300];
    snprintf(p, sizeof(p), "assets/stages/%s.json", name);
    FILE* f = fopen(p, "rb");
    if(f != NULL) { fclose(f); return 1; }
    snprintf(p, sizeof(p), "assets/data/stages/%s.json", name);
    f = fopen(p, "rb");
    if(f != NULL) { fclose(f); return 1; }
    return 0;
}

// json do mod da musica: <mod>/stages/<nome>.json (layout Psych mods/stages)
static char StageJsonForSong_Exists(const char* name, const char* songDir) {    const char* pm = strstr(songDir, "assets/mods/");
    if(pm == NULL) return 0;
    const char* after = pm + 12;
    const char* slash = strchr(after, '/');
    char modRoot[256] = {0};
    if(slash != NULL) {
        const char* songs = strstr(slash, "/songs/");
        size_t n = (size_t)((songs != NULL ? songs : slash) - songDir);
        if(n >= sizeof(modRoot)) return 0;
        memcpy(modRoot, songDir, n);
        modRoot[n] = 0;
    } else {
        return 0;
    }
    char p[300];
    snprintf(p, sizeof(p), "%s/stages/%s.json", modRoot, name);
    FILE* f = fopen(p, "rb");
    if(f != NULL) { fclose(f); return 1; }
    return 0;
}

// .lua vale porque o exe converte na hora (Stage_ParseLua, sem LuaJIT)
char StageFileForSong_Exists(const char* name, const char* songDir) {
    if(StageJsonForSong_Exists(name, songDir)) return 1;
    char lua[300];
    return Stage_FindLua(name, songDir, lua);
}

char StageAsset_Exists(const char* name) {
    if(name == NULL || name[0] == 0) return 0;
    if(strchr(name, '/') != NULL || strchr(name, '\\') != NULL || strstr(name, "..") != NULL) return 0;
    if(strcmp(name, "stage") == 0) {
        FILE* a = fopen("assets/images/stage/stageback.png", "rb");
        FILE* b = fopen("assets/images/stage/stagefront.png", "rb");
        char ok = (a != NULL && b != NULL);
        if(a != NULL) fclose(a);
        if(b != NULL) fclose(b);
        return ok;
    }
    if(StageJsonGlobal_Exists(name)) return 1;
    {
        // .lua global tambem vale: o exe parseia na hora
        char p[300];
        snprintf(p, sizeof(p), "assets/stages/%s.lua", name);
        FILE* f = fopen(p, "rb");
        if(f != NULL) { fclose(f); return 1; }
        snprintf(p, sizeof(p), "assets/data/stages/%s.lua", name);
        f = fopen(p, "rb");
        if(f != NULL) { fclose(f); return 1; }
    }
    char p[300];
    snprintf(p, sizeof(p), "assets/images/stage/%sback.png", name);
    FILE* f = fopen(p, "rb");
    if(f != NULL) { fclose(f); return 1; }
    snprintf(p, sizeof(p), "assets/images/stage/%s.png", name);
    f = fopen(p, "rb");
    if(f != NULL) { fclose(f); return 1; }
    snprintf(p, sizeof(p), "assets/images/stage/%sfront.png", name);
    f = fopen(p, "rb");
    if(f != NULL) { fclose(f); return 1; }
    return 0;
}

void StageList_Scan(void) {
    stageCount = 0;
    for(int i = 0; i < pickerCount && stageCount < 32; i++) {
        char st[SONG_NAME_MAX] = {0};
        if(!StageOfDir(pickerSongs[i].dir, st) || st[0] == 0)
            continue;
        if(!StageAsset_Exists(st) && !StageFileForSong_Exists(st, pickerSongs[i].dir))
            continue;
        char dup = 0;
        for(int k = 0; k < stageCount; k++) {
            if(strcmp(stageNames[k], st) == 0) {
                dup = 1;
                break;
            }
        }
        if(!dup) {
            strncpy(stageNames[stageCount], st, SONG_NAME_MAX - 1);
            stageNames[stageCount][SONG_NAME_MAX - 1] = 0;
            stageCount++;
        }
    }
    if(stageCount == 0 && StageAsset_Exists("stage") && stageCount < 32) {
        strncpy(stageNames[0], "stage", SONG_NAME_MAX - 1);
        stageNames[0][SONG_NAME_MAX - 1] = 0;
        stageCount = 1;
    }
}

/* Lista os personagens de uma pasta (png + animset). O icone nao e exigido: sem ele o jogo usa o do bf/dad. */
static void NameList_ScanDir(char (*names)[SONG_NAME_MAX], int* count, const char* charDir) {
    const int capacity = NAME_LIST_MAX;
    if(!DirectoryExists(charDir))
        return;
    FilePathList list = LoadDirectoryFiles(charDir);
    for(unsigned int i = 0; i < list.count && *count < capacity; i++) {
        const char* p = list.paths[i];
        size_t L = strlen(p);
        if(L < 5 || strcmp(p + L - 4, ".png") != 0)
            continue;
        const char* s1 = strrchr(p, '/');
        const char* s2 = strrchr(p, '\\');
        const char* b = (s1 > s2) ? s1 : s2;
        b = (b != NULL) ? b + 1 : p;
        size_t nlen = L - (size_t)(b - p) - 4;
        if(nlen < 1 || nlen > SONG_NAME_MAX - 1)
            continue;
        if(b[0] == '_' || strstr(b, "Title") != NULL)
            continue;
        char dup = 0;
        for(int k = 0; k < *count && !dup; k++)
            dup = strncmp(names[k], b, nlen) == 0 && names[k][nlen] == 0;
        if(dup)
            continue;
        // sem fantasma: precisa do .animset (e do icone, quando pedido)
        char ap[300];
        snprintf(ap, sizeof(ap), "%s/%.*s.animset", charDir, (int)nlen, b);
        if(!FileExists(ap))
            continue;
        memcpy(names[*count], b, nlen);
        names[*count][nlen] = 0;
        (*count)++;
    }
    UnloadDirectoryFiles(list);
}

/* Refaz as listas de personagens (Boyfriend/Opponent) e de girlfriends (global + mod da musica). */
void CharList_Scan(void) {
    charCount = 0;
    gfCount = 0;
    strcpy(gfNames[gfCount++], "none"); // primeira opcao: sem girlfriend

    NameList_ScanDir(charNames, &charCount, "assets/images/characters");
    NameList_ScanDir(gfNames, &gfCount, "assets/images/characters");

    // chars do mod da musica (importador gera .animset + icones no mod)
    char modRoot[256];
    EdModRoot(chart.dir, modRoot);
    if(modRoot[0] != 0) {
        char cd[300];
        snprintf(cd, sizeof(cd), "%s/images/characters", modRoot);
        NameList_ScanDir(charNames, &charCount, cd);
        NameList_ScanDir(gfNames, &gfCount, cd);
    }
}

// note skin list (scan assets/images/notes/ e mod/images/notes/)
char skinNames[32][SONG_NAME_MAX];

int skinCount = 0;

TxtBox skinBox;

static void NoteSkinList_ScanDir(const char* notesDir) {
    if(!DirectoryExists(notesDir))
        return;
    FilePathList list = LoadDirectoryFiles(notesDir);
    for(unsigned int i = 0; i < list.count && skinCount < 32; i++) {
        const char* p = list.paths[i];
        // so considera diretorios (pastas de skin)
        if(!DirectoryExists(p))
            continue;
        const char* s1 = strrchr(p, '/');
        const char* s2 = strrchr(p, '\\');
        const char* b = (s1 > s2) ? s1 : s2;
        b = (b != NULL) ? b + 1 : p;
        size_t nlen = strlen(b);
        if(nlen < 1 || nlen > SONG_NAME_MAX - 1)
            continue;
        // verifica se tem pelo menos as 4 sprites basicas
        char fp[300];
        int ok = 1;
        const char* req[4] = {"left.png", "down.png", "up.png", "right.png"};
        for(int r = 0; r < 4 && ok; r++) {
            snprintf(fp, sizeof(fp), "%s/%s", p, req[r]);
            FILE* f = fopen(fp, "rb");
            if(f == NULL) ok = 0;
            else fclose(f);
        }
        if(!ok) continue;
        char dup = 0;
        for(int k = 0; k < skinCount; k++) {
            if(strcmp(skinNames[k], b) == 0) { dup = 1; break; }
        }
        if(!dup) {
            memcpy(skinNames[skinCount], b, nlen);
            skinNames[skinCount][nlen] = 0;
            skinCount++;
        }
    }
    UnloadDirectoryFiles(list);
}

void NoteSkinList_Scan(void) {
    skinCount = 0;
    NoteSkinList_ScanDir("assets/images/notes");
    // skins do mod da musica
    char modRoot[256];
    EdModRoot(chart.dir, modRoot);
    if(modRoot[0] != 0) {
        char nd[300];
        snprintf(nd, sizeof(nd), "%s/images/notes", modRoot);
        NoteSkinList_ScanDir(nd);
    }
    // garante "default" no topo se existir
    for(int i = 0; i < skinCount; i++) {
        if(strcmp(skinNames[i], "default") == 0) {
            if(i != 0) {
                char tmp[SONG_NAME_MAX];
                memcpy(tmp, skinNames[i], sizeof(tmp));
                memmove(skinNames[1], skinNames[0], i * sizeof(skinNames[0]));
                memcpy(skinNames[0], tmp, sizeof(tmp));
            }
            break;
        }
    }
}

TxtBox p1Box;

TxtBox p2Box;

TxtBox gfBox;

TxtBox stageBox;
