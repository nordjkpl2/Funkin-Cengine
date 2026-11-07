/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: listas de personagens, stages e noteskins (varredura de pastas) e as caixas ligadas a elas. */
#include"scenes/AllScenes.h"
#include"data/Song.h"
#include"data/SongList.h"
#include "core/ModPath.h"
#include "core/ModList.h"
#include "logic/Character.h"
#include"logic/Stage.h"
#include"logic/Note.h"
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

// Personagem valido numa raiz: .animset + (png do mesmo nome OU characters/<nome>.json, que aponta
// para outra imagem, como "fatigue dave2"). Sem fantasma; icone nao e exigido (cai no bf/dad).
static char CharFilesOk(const char* root, const char* name) {
    char p[300];
    snprintf(p, sizeof(p), "%s/images/characters/%s.animset", root, name);
    if(!FileExists(p)) return 0;
    snprintf(p, sizeof(p), "%s/images/characters/%s.png", root, name);
    if(FileExists(p)) return 1;
    snprintf(p, sizeof(p), "%s/characters/%s.json", root, name);
    return FileExists(p);
}

static char SafeName(const char* name) {
    return name != NULL && name[0] != 0 && strchr(name, '/') == NULL && strchr(name, '\\') == NULL &&
           strstr(name, "..") == NULL;
}

// vale no mod da musica, no global ou em qualquer mod ativo
char CharacterAssetForSong_Exists(const char* name, const char* songDir) {
    if(!SafeName(name))
        return 0;
    if(CharFilesOk("assets", name))
        return 1;
    char modRoot[256];
    EdModRoot(songDir, modRoot);
    if(modRoot[0] != 0 && CharFilesOk(modRoot, name))
        return 1;
    char other[256];
    return ModList_FindCharacterRoot(name, other, sizeof(other));
}

// girlfriend vale se tem animset + imagem (global, mod da musica ou mod ativo); "none"/"nogf" = sem girlfriend
char GirlfriendAsset_Exists(const char* name, const char* songDir) {
    if(name == NULL || name[0] == 0)
        return 0;
    if(strcmp(name, "none") == 0 || strcmp(name, "nogf") == 0 || strcmp(name, "__not_have_a_gf") == 0)
        return 1;
    return CharacterAssetForSong_Exists(name, songDir);
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
    if(strcmp(name, "__not_have_a_stage") == 0) return 1;   // teste de render: sem stage
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
    strcpy(stageNames[stageCount++], "__not_have_a_stage"); // primeira opcao: sem stage
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
    if(stageCount == 1 && StageAsset_Exists("stage")) {
        strncpy(stageNames[1], "stage", SONG_NAME_MAX - 1);
        stageNames[1][SONG_NAME_MAX - 1] = 0;
        stageCount = 2;
    }
}

/* Lista os personagens de uma pasta (<raiz>/images/characters): cada .animset com imagem propria
   (png do mesmo nome) ou com characters/<nome>.json. O icone nao e exigido. */
static void NameList_ScanDir(char (*names)[SONG_NAME_MAX], int* count, const char* charDir) {
    const int capacity = NAME_LIST_MAX;
    if(!DirectoryExists(charDir))
        return;
    // raiz = charDir sem o sufixo "/images/characters"
    char root[300];
    snprintf(root, sizeof(root), "%s", charDir);
    const size_t rl = strlen(root);
    static const char tail[] = "/images/characters";
    if(rl > sizeof(tail) - 1 && strcmp(root + rl - (sizeof(tail) - 1), tail) == 0)
        root[rl - (sizeof(tail) - 1)] = 0;
    FilePathList list = LoadDirectoryFilesEx(charDir, ".animset", false);
    for(unsigned int i = 0; i < list.count && *count < capacity; i++) {
        const char* p = list.paths[i];
        size_t L = strlen(p);
        const char* s1 = strrchr(p, '/');
        const char* s2 = strrchr(p, '\\');
        const char* b = (s1 > s2) ? s1 : s2;
        b = (b != NULL) ? b + 1 : p;
        size_t nlen = L - (size_t)(b - p) - 8;   // ".animset"
        if(nlen < 1 || nlen > SONG_NAME_MAX - 1)
            continue;
        if(b[0] == '_' || strstr(b, "Title") != NULL)
            continue;
        char name[SONG_NAME_MAX];
        memcpy(name, b, nlen);
        name[nlen] = 0;
        char dup = 0;
        for(int k = 0; k < *count && !dup; k++)
            dup = strcmp(names[k], name) == 0;
        if(dup || !CharFilesOk(root, name))
            continue;
        memcpy(names[*count], name, nlen + 1);
        (*count)++;
    }
    UnloadDirectoryFiles(list);
}

/* Refaz as listas de personagens (Boyfriend/Opponent) e de girlfriends (global + mod da musica). */
void CharList_Scan(void) {
    charCount = 0;
    gfCount = 0;
    ModList_Rescan();
    strcpy(gfNames[gfCount++], "none"); // sem girlfriend
    strcpy(gfNames[gfCount++], "__not_have_a_gf"); // teste de render: sem girlfriend

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

    // demais mods ativos (o do chart ja foi; repetidos sao ignorados)
    for(int i = 0; i < ModList_Count(); i++) {
        if(!ModList_Enabled(i))
            continue;
        char cd[300];
        snprintf(cd, sizeof(cd), "%s/images/characters", ModList_Root(i));
        NameList_ScanDir(charNames, &charCount, cd);
        NameList_ScanDir(gfNames, &gfCount, cd);
    }
}

// note skin list (scan assets/images/notes/ e mod/images/notes/)
char skinNames[SKIN_LIST_MAX][SONG_NAME_MAX];

int skinCount = 0;

TxtBox skinBox;

static void NoteSkinList_ScanDir(const char* notesDir) {
    if(!DirectoryExists(notesDir))
        return;
    FilePathList list = LoadDirectoryFiles(notesDir);
    for(unsigned int i = 0; i < list.count && skinCount < SKIN_LIST_MAX; i++) {
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

/* Skins no formato do jogo: <pasta>/NOTE_<nome>_assets.png + .xml (ou .animset). */
static void NoteSkinList_ScanAssets(const char* imagesDir) {
    if(!DirectoryExists(imagesDir))
        return;
    FilePathList list = LoadDirectoryFiles(imagesDir);
    for(unsigned int i = 0; i < list.count && skinCount < SKIN_LIST_MAX; i++) {
        const char* p = list.paths[i];
        const char* s1 = strrchr(p, '/');
        const char* s2 = strrchr(p, '\\');
        const char* b = (s1 > s2) ? s1 : s2;
        b = (b != NULL) ? b + 1 : p;
        const size_t len = strlen(b);
        static const char pre[] = "NOTE_", suf[] = "_assets.png";
        if(len <= sizeof(pre) - 1 + sizeof(suf) - 1 || strncmp(b, pre, sizeof(pre) - 1) != 0 ||
           strcmp(b + len - (sizeof(suf) - 1), suf) != 0)
            continue;
        const size_t nlen = len - (sizeof(pre) - 1) - (sizeof(suf) - 1);
        if(nlen > SONG_NAME_MAX - 1)
            continue;
        char name[SONG_NAME_MAX];
        memcpy(name, b + sizeof(pre) - 1, nlen);
        name[nlen] = 0;
        char fp[300];
        snprintf(fp, sizeof(fp), "%s/NOTE_%s_assets.xml", imagesDir, name);
        char ok = FileExists(fp);
        if(!ok) {
            snprintf(fp, sizeof(fp), "%s/NOTE_%s_assets.animset", imagesDir, name);
            ok = FileExists(fp);
        }
        if(!ok)
            continue;
        char dup = 0;
        for(int k = 0; k < skinCount && !dup; k++)
            dup = strcmp(skinNames[k], name) == 0;
        if(!dup) {
            memcpy(skinNames[skinCount], name, nlen + 1);
            skinCount++;
        }
    }
    UnloadDirectoryFiles(list);
}

void NoteSkinList_Scan(void) {
    skinCount = 0;
    NoteSkinList_ScanDir("assets/images/notes");
    NoteSkinList_ScanAssets("assets/images");
    // skins do mod da musica
    char modRoot[256];
    EdModRoot(chart.dir, modRoot);
    if(modRoot[0] != 0) {
        char nd[300];
        snprintf(nd, sizeof(nd), "%s/images/notes", modRoot);
        NoteSkinList_ScanDir(nd);
        snprintf(nd, sizeof(nd), "%s/images", modRoot);
        NoteSkinList_ScanAssets(nd);
    }
    // demais mods ativos (o jogo tambem acha a skin neles)
    for(int i = 0; i < ModList_Count(); i++) {
        if(!ModList_Enabled(i))
            continue;
        char nd[300];
        snprintf(nd, sizeof(nd), "%s/images", ModList_Root(i));
        NoteSkinList_ScanAssets(nd);
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
