/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* // * Código do Chart Editor */
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

static SongEntry* pickerSongs;

static int pickerCount;

// lista de cantores (artes que existem): pro seletor Boyfriend/Opponent
#define NAME_LIST_MAX 256   /* mods grandes tem centenas de personagens */

static char charNames[NAME_LIST_MAX][SONG_NAME_MAX];

static int charCount = 0;

// lista de girlfriends (so precisa de png + animset, nao tem icone)
static char gfNames[NAME_LIST_MAX][SONG_NAME_MAX];

static int gfCount = 0;

// stages que existem nas musicas (pro seletor de Stage)
static char stageNames[32][SONG_NAME_MAX];

static int stageCount = 0;

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
static void EdModRoot(const char* songDir, char out[256]) {
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
static char CharacterAssetForSong_Exists(const char* name, const char* songDir) {
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
static char GirlfriendAsset_Exists(const char* name, const char* songDir) {
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
static char StageFileForSong_Exists(const char* name, const char* songDir) {
    if(StageJsonForSong_Exists(name, songDir)) return 1;
    char lua[300];
    return Stage_FindLua(name, songDir, lua);
}

static char StageAsset_Exists(const char* name) {
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

static void StageList_Scan(void) {
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
static void CharList_Scan(void) {
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
static char skinNames[32][SONG_NAME_MAX];

static int skinCount = 0;

static TxtBox skinBox;

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

static void NoteSkinList_Scan(void) {
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

// ---------- estado da cena ----------
typedef enum { CE_PICKER, CE_EDIT } CeMode;

// abas estilo Psych (uma ativa por vez, resto colapsado)
typedef enum { TAB_CHARTING, TAB_DATA, TAB_EVENTS, TAB_NOTE, TAB_SECTION, TAB_SONG, TAB_COUNT } ChartTab;

static const char* ChartTab_Name(int t) {
    switch(t) {
        case TAB_CHARTING: return "Charting";
        case TAB_DATA: return "Data";
        case TAB_EVENTS: return "Events";
        case TAB_NOTE: return "Note";
        case TAB_SECTION: return "Section";
        case TAB_SONG: return "Song";
        default: return "?";
    }
}

static CeMode ceMode;

static int activeTab = TAB_SECTION;

static int pickerSel = 0; // 0 = open file, 1..N = songs

static float pickerOffset = 0;

static TxtBox openBox;

static TxtBox saveAsBox;

static TxtBox p1Box;

static TxtBox p2Box;

static TxtBox gfBox;

static TxtBox stageBox;

static char pendingDir[256]; // entrar direto no editor (volta do teste)

static void Editor_UnloadIcons(void) {
    if(edIconOOk) {
        UnloadTexture(edIconO);
        edIconOOk = 0;
    }
    if(edIconPOk) {
        UnloadTexture(edIconP);
        edIconPOk = 0;
    }
}

// "healthicon" do characters/<nome>.json (mod da musica primeiro, depois global); "" = nao tem
static void Editor_HealthIcon(const char* name, const char* modRoot, char out[SONG_NAME_MAX]) {
    out[0] = 0;
    char p[300];
    for(int pass = 0; pass < 2; pass++) {
        if(pass == 0 && modRoot[0] == 0)
            continue;
        if(pass == 0)
            snprintf(p, sizeof(p), "%s/characters/%s.json", modRoot, name);
        else
            snprintf(p, sizeof(p), "assets/characters/%s.json", name);
        yyjson_doc* doc = yyjson_read_file(p, 0, NULL, NULL);
        if(doc == NULL)
            continue;
        yyjson_val* v = yyjson_obj_get(yyjson_doc_get_root(doc), "healthicon");
        if(yyjson_is_str(v) && strchr(yyjson_get_str(v), '/') == NULL && strchr(yyjson_get_str(v), '\\') == NULL
            && strstr(yyjson_get_str(v), "..") == NULL)
            snprintf(out, SONG_NAME_MAX, "%s", yyjson_get_str(v));
        yyjson_doc_free(doc);
        return;
    }
}

// acha icons/<nome>.png no mod da musica e depois no global
static char Editor_FindIconFile(const char* name, const char* modRoot, char out[300]) {
    if(name[0] == 0 || strchr(name, '/') != NULL || strchr(name, '\\') != NULL || strstr(name, "..") != NULL)
        return 0;
    if(modRoot[0] != 0) {
        snprintf(out, 300, "%s/images/icons/%s.png", modRoot, name);
        if(FileExists(out))
            return 1;
    }
    snprintf(out, 300, "assets/images/icons/%s.png", name);
    return FileExists(out);
}

// mesma ordem do jogo: healthicon do json, nome do personagem, fallback (bf/dad)
static Texture2D Editor_TryIcon(const char* name, const char* fallback, char* ok) {
    const char* want = (name != NULL && name[0] != 0) ? name : fallback;
    char modRoot[256];
    EdModRoot(chart.dir, modRoot);
    char health[SONG_NAME_MAX];
    char p[300];
    Editor_HealthIcon(want, modRoot, health);
    if((health[0] != 0 && Editor_FindIconFile(health, modRoot, p)) || Editor_FindIconFile(want, modRoot, p)
        || (fallback != NULL && Editor_FindIconFile(fallback, "", p))) {
        *ok = 1;
        return Render_LoadTexture(p);
    }
    *ok = 0;
    return (Texture2D) {0};
}

static void Editor_LoadIcons(void) {
    Editor_UnloadIcons();
    // p2 vazio (tutorial): sem icone de oponente
    if(chart.p2[0] == 0) {
        edIconOOk = 0;
    } else {
        edIconO = Editor_TryIcon(chart.p2, "dad", &edIconOOk);
    }
    edIconP = Editor_TryIcon(chart.p1, "bf", &edIconPOk);
}

static EdSec copyBuf;

static char copyValid = 0;

static RayGraphicObject bgEd;

static void Picker_Refresh(void) {
    SongList_Free(pickerSongs);
    pickerSongs = NULL;
    pickerCount = SongList_Scan(&pickerSongs);
    pickerSel = 0;
    pickerOffset = 0;
}

static void Editor_EnterDir(const char* dir) {
    if(!EdChart_OpenDir(&chart, dir)) {
        EdStatusMsg("NAO ABRIU ESSA MUSICA");
        return;
    }
    chartLoaded = 1;
    chart.dirty = 0;
    Undo_Clear();
    if(copyValid) {
        free(copyBuf.notes);
        copyValid = 0;
    }
    secIdx = 0;
    cursorMs = 0;
    viewTopMs = 0;
    playing = 0;
    snapDiv = 4;
    strncpy(p1Box.buf, chart.p1, sizeof(p1Box.buf) - 1);
    strncpy(p2Box.buf, chart.p2, sizeof(p2Box.buf) - 1);
    strncpy(gfBox.buf, chart.gf, sizeof(gfBox.buf) - 1);
    strncpy(stageBox.buf, chart.stage, sizeof(stageBox.buf) - 1);
    strncpy(skinBox.buf, chart.noteSkin, sizeof(skinBox.buf) - 1);
    CharList_Scan();
    StageList_Scan();
    NoteSkinList_Scan();
    activeDrop = 0;
    menuOpen = 0;
    activeTab = TAB_SECTION;
    Editor_LoadIcons();
    // audio pra ouvir (se nao tiver, anda no silencio)
    // voz: Voices.ogg, ou o par separado Voices-Opponent/Voices-Player
    char p[300];
    if(edHasAudio) {
        UnloadMusicStream(edInst);
        edHasAudio = 0;
    }
    if(edHasVoices) {
        UnloadMusicStream(edVoices);
        edHasVoices = 0;
    }
    if(edHasSplit) {
        UnloadMusicStream(edSplitO);
        UnloadMusicStream(edSplitP);
        edHasSplit = 0;
    }
    FILE* f;
    snprintf(p, sizeof(p), "%s/Inst.ogg", dir);
    f = fopen(p, "rb");
    if(f != NULL) {
        fclose(f);
        edInst = LoadMusicStream(p);
        SetMusicVolume(edInst, RayGame_MusicVolumeLevel());
        edHasAudio = 1;
    }
    snprintf(p, sizeof(p), "%s/Voices.ogg", dir);
    f = fopen(p, "rb");
    if(f != NULL) {
        fclose(f);
        edVoices = LoadMusicStream(p);
        SetMusicVolume(edVoices, RayGame_MusicVolumeLevel());
        edHasVoices = 1;
    } else {
        char po[300], pp[300];
        snprintf(po, sizeof(po), "%s/Voices-Opponent.ogg", dir);
        snprintf(pp, sizeof(pp), "%s/Voices-Player.ogg", dir);
        FILE* fo = fopen(po, "rb");
        FILE* fp = fopen(pp, "rb");
        if(fo != NULL && fp != NULL) {
            fclose(fo);
            fclose(fp);
            edSplitO = LoadMusicStream(po);
            edSplitP = LoadMusicStream(pp);
            SetMusicVolume(edSplitO, RayGame_MusicVolumeLevel());
            SetMusicVolume(edSplitP, RayGame_MusicVolumeLevel());
            edHasSplit = 1;
        } else {
            if(fo != NULL) fclose(fo);
            if(fp != NULL) fclose(fp);
        }
    }
    ceMode = CE_EDIT;
    ceDlg = DLG_NONE;
    focusBox = NULL;

    // set custom note skin for editor preview
    if(chart.noteSkin[0] != 0) {
        RayAnimationHandler skin = Cache_GetNoteSkin(chart.noteSkin, dir);
        Note_SetCurrentSkin(&skin);
    } else {
        Note_SetCurrentSkin(NULL);
    }

    EdStatusMsg("opened :)");
}

static void Editor_Save(void) {
    strncpy(chart.p1, (p1Box.buf[0] != 0) ? p1Box.buf : "bf", sizeof(chart.p1) - 1);
    // vazio = sem oponente (tutorial): preserva "" no save (sem virar "dad")
    if(p2Box.buf[0] == 0) {
        strncpy(chart.p2, "", sizeof(chart.p2) - 1);
    } else {
        strncpy(chart.p2, p2Box.buf, sizeof(chart.p2) - 1);
    }
    chart.p2[sizeof(chart.p2) - 1] = 0;
    strncpy(chart.stage, (stageBox.buf[0] != 0) ? stageBox.buf : "stage", sizeof(chart.stage) - 1);
    chart.p1[sizeof(chart.p1) - 1] = 0;
    chart.stage[sizeof(chart.stage) - 1] = 0;
    // sem fantasma no save: so entra quem tem png+animset (mod ou global), senao bf/dad/stage
    // p2 vazio e valido (tutorial, so bf) e nao cai na validacao de asset
    if(!CharacterAssetForSong_Exists(chart.p1, chart.dir)) {
        strncpy(chart.p1, "bf", sizeof(chart.p1) - 1);
        strncpy(p1Box.buf, "bf", sizeof(p1Box.buf) - 1);
    }
    if(chart.p2[0] != 0 && !CharacterAssetForSong_Exists(chart.p2, chart.dir)) {
        strncpy(chart.p2, "dad", sizeof(chart.p2) - 1);
        strncpy(p2Box.buf, "dad", sizeof(p2Box.buf) - 1);
    }
    // girlfriend vazia ou inexistente volta para "gf"
    strncpy(chart.gf, (gfBox.buf[0] != 0) ? gfBox.buf : "gf", sizeof(chart.gf) - 1);
    chart.gf[sizeof(chart.gf) - 1] = 0;
    if(!GirlfriendAsset_Exists(chart.gf, chart.dir)) {
        strncpy(chart.gf, "gf", sizeof(chart.gf) - 1);
        strncpy(gfBox.buf, "gf", sizeof(gfBox.buf) - 1);
    }
    if(!StageAsset_Exists(chart.stage) && !StageFileForSong_Exists(chart.stage, chart.dir)) {
        strncpy(chart.stage, "stage", sizeof(chart.stage) - 1);
        strncpy(stageBox.buf, "stage", sizeof(stageBox.buf) - 1);
    }
    strncpy(chart.noteSkin, skinBox.buf, sizeof(chart.noteSkin) - 1);
    chart.noteSkin[sizeof(chart.noteSkin) - 1] = 0;
    if(EdChart_Save(&chart)) {
        chart.dirty = 0;
        Editor_LoadIcons();
        EdStatusMsg("SALVO!");
    } else {
        EdStatusMsg("ERRO AO SALVAR");
    }
}

// importa json solto -> vira pasta de mod -> abre (1 = ok)
static char ImportJSON(const char* jsonPath, const char* modName) {
    char clean[64];
    EdSanitizeName(clean, sizeof(clean), modName);
    if(clean[0] == 0) {
        EdStatusMsg("NOME INVALIDO");
        return 0;
    }
    char dir[256];
    snprintf(dir, sizeof(dir), "assets/mods/%s", clean);
    if(Song_HasChart(dir)) {
        EdStatusMsg("JA EXISTE ESSE NOME");
        return 0;
    }
    MakeDirectory(dir);
    char dst[300];
    snprintf(dst, sizeof(dst), "%s/data.json", dir);
    if(!EdCopyFile(jsonPath, dst)) {
        EdStatusMsg("NAO DEU PRA COPIAR");
        return 0;
    }
    Editor_EnterDir(dir);
    return 1;
}

// SALVAR COMO: copia audio + salva json com outro nome (1 = ok)
static char Editor_SaveAs(const char* modName) {
    char clean[64];
    EdSanitizeName(clean, sizeof(clean), modName);
    if(clean[0] == 0) {
        EdStatusMsg("NOME INVALIDO");
        return 0;
    }
    char dir[256];
    snprintf(dir, sizeof(dir), "assets/mods/%s", clean);
    if(strcmp(dir, chart.dir) == 0) {
        Editor_Save();
        return 1;
    }
    if(Song_HasChart(dir)) {
        EdStatusMsg("JA EXISTE ESSE NOME");
        return 0;
    }
    // mesma musica, outro nome: RENOMEIA a pasta ( so vale pros mods )
    if(strncmp(chart.dir, "assets/mods/", 12) == 0 && Song_HasChart(chart.dir)) {
        if(rename(chart.dir, dir) == 0) {
            strncpy(chart.dir, dir, sizeof(chart.dir) - 1);
            chart.dir[sizeof(chart.dir) - 1] = 0;
            strncpy(chart.name, clean, sizeof(chart.name) - 1);
            chart.name[sizeof(chart.name) - 1] = 0;
            Editor_Save();
            Picker_Refresh();
            EdStatusMsg("PASTA RENOMEADA");
            return 1;
        }
        // rename falhou: cai pro copia normal abaixo
    }
    MakeDirectory(dir);
    char src[300], dst[300];
    snprintf(src, sizeof(src), "%s/Inst.ogg", chart.dir);
    snprintf(dst, sizeof(dst), "%s/Inst.ogg", dir);
    EdCopyFile(src, dst);
    snprintf(src, sizeof(src), "%s/Voices.ogg", chart.dir);
    snprintf(dst, sizeof(dst), "%s/Voices.ogg", dir);
    EdCopyFile(src, dst);
    snprintf(src, sizeof(src), "%s/Voices-Opponent.ogg", chart.dir);
    snprintf(dst, sizeof(dst), "%s/Voices-Opponent.ogg", dir);
    EdCopyFile(src, dst);
    snprintf(src, sizeof(src), "%s/Voices-Player.ogg", chart.dir);
    snprintf(dst, sizeof(dst), "%s/Voices-Player.ogg", dir);
    EdCopyFile(src, dst);
    strncpy(chart.dir, dir, sizeof(chart.dir) - 1);
    strncpy(chart.name, clean, sizeof(chart.name) - 1);
    Editor_Save();
    Picker_Refresh();
    return 1;
}

static char (*dropNames)[SONG_NAME_MAX] = NULL;

static void CharDrop_Draw(void) {
    if(!activeDrop || dropBox == NULL || dropNames == NULL)
        return;
    float bx = dropX, by = dropY, bw = dropW;
    TxtBox* box = dropBox;
    int n = dropCount;
    float ih = 30;
    int rows = n < DROP_ROWS_MAX ? n : DROP_ROWS_MAX;
    float h = (rows > 0 ? rows : 1) * ih + 8;
    if(by + h > 700) by = 700 - h;
    Rectangle r = {bx - bw + 30, by, bw, h};
    if(r.x + r.width > 1270) r.x = 1270 - r.width;
    DrawRectangleRec(r, (Color) {15, 15, 15, 245});
    DrawRectangleLinesEx(r, 1, GREEN);
    Vector2 m = EdMouseWorld();
    const char inside = (m.x >= r.x && m.x <= r.x + r.width && m.y >= r.y && m.y <= r.y + r.height);

    // roda: rola a lista (so com o mouse em cima dela)
    if(dropJustOpened) dropScroll = 0;
    if(inside && n > rows) {
        dropScroll -= (int)GetMouseWheelMove();
        if(dropScroll > n - rows) dropScroll = n - rows;
        if(dropScroll < 0) dropScroll = 0;
    }

    if(n == 0) {
        DrawTextEx(mainFont, "(vazio)", (Vector2) {r.x + 8, r.y + 8}, 20, 1, YELLOW);
    }
    // o clique que abriu a lista nao pode escolher o item que ficou embaixo do mouse
    const char canPick = !dropJustOpened;
    for(int row = 0; row < rows; row++) {
        const int i = dropScroll + row;
        Rectangle ir = {r.x + 4, r.y + 4 + row * ih, r.width - 14, ih - 2};
        char hov = (m.x >= ir.x && m.x <= ir.x + ir.width && m.y >= ir.y && m.y <= ir.y + ir.height);
        if(hov)
            DrawRectangleRec(ir, (Color) {50, 90, 50, 255});
        DrawTextEx(mainFont, dropNames[i], (Vector2) {ir.x + 6, ir.y + 4}, 20, 1, WHITE);
        if(canPick && hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            strncpy(box->buf, dropNames[i], sizeof(box->buf) - 1);
            box->buf[sizeof(box->buf) - 1] = 0;
            chart.dirty = 1;
            if(activeDrop == DROP_BOYFRIEND || activeDrop == DROP_OPPONENT) {
                // icones do topo da grade seguem o personagem escolhido
                strncpy(activeDrop == DROP_BOYFRIEND ? chart.p1 : chart.p2, box->buf, sizeof(chart.p1) - 1);
                Editor_LoadIcons();
            }
            activeDrop = 0;
            return;
        }
    }
    // barra de rolagem
    if(n > rows) {
        const float trackH = h - 8;
        const float thumbH = trackH * rows / n;
        const float thumbY = r.y + 4 + (trackH - thumbH) * dropScroll / (float)(n - rows);
        DrawRectangle((int)(r.x + r.width - 8), (int)thumbY, 4, (int)thumbH, GREEN);
    }
    // clicou fora: fecha (pula o frame que abriu)
    if(dropJustOpened) {
        dropJustOpened = 0;
    } else if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if(!inside) {
            activeDrop = DROP_NONE;
        }
    }
}

// ---------- PICKER ----------
static char importPath[512];

static void Picker_OpenSelected(void) {
    if(pickerSel == 0) {
        openBox.buf[0] = 0;
        ceDlg = DLG_OPEN;
        focusBox = &openBox;
    } else if(pickerSel >= 1 && pickerSel <= pickerCount) {
        Editor_EnterDir(pickerSongs[pickerSel - 1].dir);
    }
}

static void Picker_NewSong(void) {
    EdChart_Default(&chart, "new-song");
    chart.dir[0] = 0;
    chartLoaded = 1;
    chart.dirty = 1;
    Undo_Clear();
    if(copyValid) { free(copyBuf.notes); memset(&copyBuf, 0, sizeof(copyBuf)); copyValid = 0; }
    secIdx = 0;
    cursorMs = 0;
    viewTopMs = 0;
    playing = 0;
    snapDiv = 4;
    strncpy(p1Box.buf, "bf", sizeof(p1Box.buf) - 1);
    strncpy(p2Box.buf, "dad", sizeof(p2Box.buf) - 1);
    strncpy(gfBox.buf, "gf", sizeof(gfBox.buf) - 1);
    strncpy(stageBox.buf, "stage", sizeof(stageBox.buf) - 1);
    CharList_Scan();
    StageList_Scan();
    activeDrop = 0;
    menuOpen = 0;
    activeTab = TAB_SONG;
    Editor_LoadIcons();
    if(edHasAudio) { UnloadMusicStream(edInst); edHasAudio = 0; }
    if(edHasVoices) { UnloadMusicStream(edVoices); edHasVoices = 0; }
    if(edHasSplit) { UnloadMusicStream(edSplitO); UnloadMusicStream(edSplitP); edHasSplit = 0; }
    ceMode = CE_EDIT;
    ceDlg = DLG_NONE;
    focusBox = NULL;
    focusNum = NULL;
    EdStatusMsg("NEW SONG - SALVAR COMO");
}

static void Picker_Draw(void) {
    Render_SetCamera(&camEd);
    Render_DrawGraphicObject(&bgEd);

    DrawTextEx(mainFont, "song selector", (Vector2) {150, 60}, 48, 4, WHITE);

    int rows = 1 + pickerCount;
    for(int i = 0; i < rows; i++) {
        const int fontSize = (i == 0) ? 40 : 56;
        float y = 720 / 2 + ((float)i - pickerOffset) * (10 + fontSize);
        if(y < -60 || y > 780)
            continue;
        char name[128];
        if(i == 0) {
            strncpy(name, "open file directory...", sizeof(name) - 1);
            name[sizeof(name) - 1] = 0;
        } else {
            strncpy(name, pickerSongs[i - 1].name, sizeof(name) - 1);
            name[sizeof(name) - 1] = 0;
            if(strstr(pickerSongs[i - 1].dir, "mods") != NULL)
                strncat(name, "  [mod]", sizeof(name) - strlen(name) - 1);
        }
        float d = fabsf((float)i - pickerOffset);
        Vector2 pos = {(float)(5 + 150 + d * -30), y + 5};
        Color fill = WHITE, out = BLACK;
        fill.a = (pickerSel == i) ? 255 : 150;
        out.a = (pickerSel == i) ? 255 : 100;
        if(i == 0) fill = (pickerSel == 0) ? GREEN : (Color) {150, 255, 150, 150};
        DrawTextEx(mainFont, name, pos, fontSize, 4, out);
        pos.x -= 5;
        pos.y -= 5;
        DrawTextEx(mainFont, name, pos, fontSize, 4, fill);
    }

    DrawTextEx(mainFont, "ENTER open   R update   N new   arraste um .json pra ca   ESC return",
        (Vector2) {150, 690}, 20, 1, (Color) {200, 200, 200, 255});
    if(EdButton((Rectangle) {150, 630, 200, 40}, "OPEN", 22)) Picker_OpenSelected();
    if(EdButton((Rectangle) {360, 630, 200, 40}, "NEW", 22)) Picker_NewSong();
    Render_StopCamera();
}

static void Picker_Update(void) {
    // arrastou arquivo pra janela
    if(IsFileDropped()) {
        FilePathList drop = LoadDroppedFiles();
        for(unsigned int i = 0; i < drop.count; i++) {
            const char* p = drop.paths[i];
            size_t L = strlen(p);
            if(L > 5 && (strcmp(p + L - 5, ".json") == 0 || strcmp(p + L - 5, ".JSON") == 0)) {
                EdChart tmp;
                memset(&tmp, 0, sizeof(tmp));
                if(EdChart_LoadJSON(&tmp, p, "", EdDirName(p))) {
                    EdChart_Free(&tmp);
                    strncpy(importPath, p, sizeof(importPath) - 1);
                    saveAsBox.buf[0] = 0;
                    ceDlg = DLG_SAVEAS;
                    focusBox = &saveAsBox;
                } else {
                    EdStatusMsg("ESSE JSON NAO E MUSICA");
                }
                break;
            }
        }
        UnloadDroppedFiles(drop);
    }

    if(ceDlg != DLG_NONE)
        return;

    int rows = 1 + pickerCount;
    if(IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        pickerSel++;
        if(pickerSel >= rows) pickerSel = 0;
    }
    if(IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        pickerSel--;
        if(pickerSel < 0) pickerSel = rows - 1;
    }
    if(IsKeyPressed(KEY_R))
        Picker_Refresh();
    if(IsKeyPressed(KEY_N))
        Picker_NewSong();
    if(IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) {
        RayGame_ResetMusic();
        RayGame_ToggleMusic(1);
        MenuState_SetScene();
        return;
    }
    if(IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
        Picker_OpenSelected();
        return;
    }
    // clique: 1o seleciona, 2o abre (OPEN ao lado da musica)
    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && ceDlg == DLG_NONE) {
        Vector2 m = EdMouseWorld();
        for(int i = 0; i < rows; i++) {
            int fs = (i == 0) ? 40 : 56;
            float y = 720 / 2 + ((float)i - pickerOffset) * (10 + fs);
            if(m.x >= 150 && m.x <= 1100 && m.y >= y - 5 && m.y <= y + fs) {
                if(pickerSel == i)
                    Picker_OpenSelected();
                else
                    pickerSel = i;
                break;
            }
        }
    }
    pickerOffset = Lerp(pickerOffset, (float)pickerSel, RayGame_DeltaTime() * 10.0f);
}

// volta pra lista de musicas, soltando audio e icones
static void Editor_LeaveToPicker(void) {
    Audio_StopPlay();
    if(edHasAudio) {
        UnloadMusicStream(edInst);
        edHasAudio = 0;
    }
    if(edHasVoices) {
        UnloadMusicStream(edVoices);
        edHasVoices = 0;
    }
    if(edHasSplit) {
        UnloadMusicStream(edSplitO);
        UnloadMusicStream(edSplitP);
        edHasSplit = 0;
    }
    Editor_UnloadIcons();
    ceDlg = DLG_NONE;
    ceMode = CE_PICKER;
    Picker_Refresh();
}

// com alteracoes pendentes pergunta antes (salvar / descartar / cancelar)
static void Editor_RequestLeave(void) {
    if(chart.dirty) {
        ceDlg = DLG_LEAVE;
        focusBox = NULL;
        focusNum = NULL;
    } else {
        Editor_LeaveToPicker();
    }
}

// ---------- dialogs (por cima de tudo) ----------
static void Dialogs_Update(void) {
    if(ceDlg == DLG_NONE)
        return;
    if(IsKeyPressed(KEY_ESCAPE)) {
        ceDlg = DLG_NONE;
        focusBox = NULL;
        importPath[0] = 0;
        return;
    }
    if(IsKeyPressed(KEY_ENTER)) {
        if(ceDlg == DLG_LEAVE) {
            Editor_Save();
            Editor_LeaveToPicker();
            return;
        }
        if(ceDlg == DLG_OPEN) {
            if(openBox.buf[0] == 0) {
                EdStatusMsg("COLE O CAMINHO PRIMEIRO");
                return;
            }
            EdChart tmp;
            memset(&tmp, 0, sizeof(tmp));
            if(EdChart_LoadJSON(&tmp, openBox.buf, "", EdDirName(openBox.buf))) {
                EdChart_Free(&tmp);
                strncpy(importPath, openBox.buf, sizeof(importPath) - 1);
                saveAsBox.buf[0] = 0;
                ceDlg = DLG_SAVEAS;
                focusBox = &saveAsBox;
            } else {
                EdStatusMsg("NAO ABRIU: confira o caminho");
            }
        } else if(ceDlg == DLG_SAVEAS) {
            char ok = 0;
            if(importPath[0] != 0)
                ok = ImportJSON(importPath, saveAsBox.buf);
            else
                ok = Editor_SaveAs(saveAsBox.buf);
            if(ok) {
                ceDlg = DLG_NONE;
                focusBox = NULL;
                importPath[0] = 0;
            }
        }
    }
}

static void Dialogs_Draw(void) {
    if(ceDlg == DLG_NONE)
        return;
    Render_SetCamera(&camEd);
    DrawRectangle(0, 0, 1280, 720, (Color) {0, 0, 0, 180});
    if(ceDlg == DLG_LEAVE) {
        DrawTextEx(mainFont, "Salvar as alteracoes?", (Vector2) {340, 250}, 40, 3, WHITE);
        DrawTextEx(mainFont, "enter salva, esc cancela", (Vector2) {340, 310}, 20, 1, YELLOW);
        if(EdDlgButton((Rectangle) {340, 360, 170, 48}, "SALVAR", GREEN)) {
            Editor_Save();
            Editor_LeaveToPicker();
        } else if(EdDlgButton((Rectangle) {530, 360, 170, 48}, "DESCARTAR", RED)) {
            chart.dirty = 0;
            Editor_LeaveToPicker();
        } else if(EdDlgButton((Rectangle) {720, 360, 170, 48}, "CANCELAR", GRAY)) {
            ceDlg = DLG_NONE;
        }
    } else if(ceDlg == DLG_OPEN) {
        DrawTextEx(mainFont, ".json path", (Vector2) {340, 250}, 40, 3, WHITE);
        Rectangle r = {340, 320, 600, 40};
        TxtBox_Update(&openBox, r, 0);
        TxtBox_Draw(&openBox, r, "C:/.../musica.json");
        DrawTextEx(mainFont, "enter import, esc cancel", (Vector2) {340, 380}, 20, 1, YELLOW);
        DrawTextEx(mainFont, "You can copy and paste the path to the .json file.", (Vector2) {340, 410}, 20, 1, YELLOW);
    } else {
        DrawTextEx(mainFont, "Save (path name):", (Vector2) {140, 190}, 36, 3, WHITE);
        Rectangle r = {140, 260, 600, 40};
        TxtBox_Update(&saveAsBox, r, 0);
        TxtBox_Draw(&saveAsBox, r, "my-music-renameonexplorer");
        DrawTextEx(mainFont, "enter save, esc cancel", (Vector2) {140, 320}, 20, 1, YELLOW);
        DrawTextEx(mainFont, "renames the folder if it already exists", (Vector2) {140, 350}, 20, 1, YELLOW);
        {
            char clean[64];
            EdSanitizeName(clean, sizeof(clean), saveAsBox.buf);
            char where[192];
            char dstDir[128];
            if(clean[0] != 0) {
                snprintf(dstDir, sizeof(dstDir), "assets/mods/%s", clean);
                snprintf(where, sizeof(where), "saves to: %s/data.json", dstDir);
            } else {
                snprintf(dstDir, sizeof(dstDir), "assets/mods/<your-song>");
                snprintf(where, sizeof(where), "saves to: mods/<your-song>");
            }
            DrawTextEx(mainFont, where, (Vector2) {140, 380}, 20, 1, YELLOW);
            if(importPath[0] != 0) {
                char from[96];
                size_t L = strlen(importPath);
                const char* tail = (L > 52) ? importPath + L - 52 : importPath;
                snprintf(from, sizeof(from), "from: ...%s", (L > 52) ? tail : importPath);
                DrawTextEx(mainFont, from, (Vector2) {140, 410}, 18, 1, GRAY);
            }
            // mini arvore do destino (leve: so re-checa quando o nome muda)
            static char lastTree[64] = {0};
            static char tChart = 0, tInst = 0, tVox = 0;
            if(strcmp(lastTree, clean) != 0) {
                strncpy(lastTree, clean, sizeof(lastTree) - 1);
                lastTree[sizeof(lastTree) - 1] = 0;
                tChart = tInst = tVox = 0;
                if(clean[0] != 0) {
                    tChart = Song_HasChart(dstDir);
                    char p[176];
                    // se o destino ja existe mostra o destino, senao mostra o que sera copiado da origem
                    const char* srcDir = (tChart || chart.dir[0] == 0) ? dstDir : chart.dir;
                    snprintf(p, sizeof(p), "%s/Inst.ogg", srcDir);
                    FILE* f = fopen(p, "rb");
                    if(f != NULL) { fclose(f); tInst = 1; }
                    snprintf(p, sizeof(p), "%s/Voices.ogg", srcDir);
                    f = fopen(p, "rb");
                    if(f != NULL) { fclose(f); tVox = 1; }
                }
            }
            float tx = 780;
            float ty = 260;
            DrawTextEx(mainFont, "tree:", (Vector2) {tx, ty}, 20, 1, GREEN);
            ty += 30;
            DrawTextEx(mainFont, "assets/", (Vector2) {tx, ty}, 18, 1, GRAY);
            ty += 24;
            DrawTextEx(mainFont, "  mods/", (Vector2) {tx, ty}, 18, 1, GRAY);
            ty += 24;
            {
                char mod[80];
                if(clean[0] != 0) snprintf(mod, sizeof(mod), "    %.28s/", clean);
                else snprintf(mod, sizeof(mod), "    <your-song>/");
                DrawTextEx(mainFont, mod, (Vector2) {tx, ty}, 18, 1, WHITE);
                DrawTextEx(mainFont, tChart ? "[existe]" : "[novo]", (Vector2) {tx + 240, ty}, 18, 1, tChart ? YELLOW : GREEN);
            }
            ty += 24;
            DrawTextEx(mainFont, "      data.json", (Vector2) {tx, ty}, 18, 1, WHITE);
            ty += 24;
            DrawTextEx(mainFont, tInst ? "      Inst.ogg [ok]" : "      Inst.ogg [--]", (Vector2) {tx, ty}, 18, 1, tInst ? GREEN : GRAY);
            ty += 24;
            DrawTextEx(mainFont, tVox ? "      Voices.ogg [ok]" : "      Voices.ogg [--]", (Vector2) {tx, ty}, 18, 1, tVox ? GREEN : GRAY);
        }
    }
    Render_StopCamera();
}

static void Edit_UpdateKeys(void) {
    int ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    int shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

    if(ctrl && IsKeyPressed(KEY_S)) {
        Editor_Save();
        return;
    }
    if(ctrl && IsKeyPressed(KEY_Z) && focusBox == NULL && focusNum == NULL) {
        Undo_Pop();
        chart.dirty = 1;
        EdStatusMsg("actions reverted");
        return;
    }
    // digitando numa caixinha: nao dispara atalho
    if(menuOpen && IsKeyPressed(KEY_ESCAPE)) {
        menuOpen = 0;
        return;
    }
    if(activeDrop && IsKeyPressed(KEY_ESCAPE)) {
        activeDrop = 0;
        return;
    }
    if(focusNum != NULL) {
        if(IsKeyPressed(KEY_ENTER))
            NumBox_Commit();
        else if(IsKeyPressed(KEY_ESCAPE))
            focusNum = NULL;
        return;
    }
    if(focusBox != NULL) {
        if(IsKeyPressed(KEY_ESCAPE))
            focusBox = NULL;
        return;
    }
    if(IsKeyPressed(KEY_SPACE)) {
        Audio_Toggle();
        return;
    }
    if(IsKeyPressed(KEY_F5)) {
        Audio_StopPlay();
        Editor_Save();
        PlayState_SetSongDir(chart.dir);
        PlayState_SetReturnEditor(1);
        PlayState_SetScene();
        return;
    }
    if(IsKeyPressed(KEY_TAB)) {
        EdTouch();
        chart.secs[secIdx].mustHit = !chart.secs[secIdx].mustHit;
        EdStatusMsg(chart.secs[secIdx].mustHit ? "Turn: You" : "Turn: Opponent");
        return;
    }
    if(IsKeyPressed(KEY_T)) {
        snapDiv = (snapDiv == 4) ? 1 : (snapDiv == 1) ? 2 : 4;
        return;
    }    if(IsKeyPressed(KEY_N)) {
        EdTouch();
        EdSec* last = chart.secs + chart.nsec - 1;
        EdSec* g = realloc(chart.secs, sizeof(EdSec) * (size_t)(chart.nsec + 1));
        if(g != NULL) {
            chart.secs = g;
            EdSec* n = chart.secs + chart.nsec;
            memset(n, 0, sizeof(*n));
            n->mustHit = last->mustHit;
            n->bpm = last->bpm;
            n->lenSteps = 16;
            chart.nsec++;
            secIdx = chart.nsec - 1;
            cursorMs = SecStartMs(secIdx);
            viewTopMs = cursorMs - VIEW_MS * 0.3;
            if(viewTopMs < 0) viewTopMs = 0;
            EdStatusMsg("TRECHO NOVO NO FIM");
        }
        return;
    }
    if(IsKeyPressed(KEY_X)) {
        /* // * part deleter */
        if(chart.nsec > 1) {
            EdTouch();
            free(chart.secs[secIdx].notes);
            memmove(chart.secs + secIdx, chart.secs + secIdx + 1,
                sizeof(EdSec) * (size_t)(chart.nsec - secIdx - 1));
            chart.nsec--;
            if(secIdx >= chart.nsec) secIdx = chart.nsec - 1;
            EdStatusMsg("part deleted");
        } else {
            EdStatusMsg("NOT POSSIBLE TO DELETE HAVE ONLY 1 PART");
        }
        return;
    }
    if(IsKeyPressed(KEY_C)) {
        /* // * part copier */
        if(copyValid) free(copyBuf.notes);
        memset(&copyBuf, 0, sizeof(copyBuf));
        EdSec* s = chart.secs + secIdx;
        copyBuf.mustHit = s->mustHit;
        copyBuf.bpm = s->bpm;
        copyBuf.lenSteps = s->lenSteps;
        if(s->count > 0) {
            copyBuf.notes = malloc(sizeof(EdNote) * (size_t)s->count);
            copyBuf.count = s->count;
            copyBuf.cap = s->count;
            memcpy(copyBuf.notes, s->notes, sizeof(EdNote) * (size_t)s->count);
        }
        copyValid = 1;
        EdStatusMsg("part Copyed");
        return;
    }
    if(IsKeyPressed(KEY_V)) {
        /* // * if pressing control v without control c */
        if(!copyValid) {
            EdStatusMsg("V pressed missing Copy Before (C)");
            return;
        }
        EdTouch();
        double base = SongEndMs();
        double srcStart = SecStartMs(secIdx);
        EdSec* g = realloc(chart.secs, sizeof(EdSec) * (size_t)(chart.nsec + 1));
        if(g != NULL) {
            chart.secs = g;
            EdSec* n = chart.secs + chart.nsec;
            memset(n, 0, sizeof(*n));
            n->mustHit = copyBuf.mustHit;
            n->bpm = chart.secs[chart.nsec - 1].bpm;
            n->lenSteps = copyBuf.lenSteps;
            for(int i = 0; i < copyBuf.count; i++)
                EdSec_Add(n, (float)(base + (copyBuf.notes[i].time - srcStart)), copyBuf.notes[i].lane, copyBuf.notes[i].len);
            chart.nsec++;
            secIdx = chart.nsec - 1;
            EdStatusMsg("Copyed on End");
        }
        return;
    }
    if(IsKeyPressed(KEY_G)) {
        secIdx--;
        if(secIdx < 0) secIdx = 0;
        cursorMs = SecStartMs(secIdx);
        viewTopMs = cursorMs - VIEW_MS * 0.3;
        if(viewTopMs < 0) viewTopMs = 0;
        return;
    }
    if(IsKeyPressed(KEY_H)) {
        secIdx++;
        if(secIdx >= chart.nsec) secIdx = chart.nsec - 1;
        cursorMs = SecStartMs(secIdx);
        viewTopMs = cursorMs - VIEW_MS * 0.3;
        if(viewTopMs < 0) viewTopMs = 0;
        return;
    }
    if(IsKeyPressed(KEY_HOME)) {
        cursorMs = SecStartMs(secIdx);
        return;
    }
    if(IsKeyPressed(KEY_COMMA) || IsKeyPressed(KEY_PERIOD)) {
        EdTouch();
        float d = (IsKeyPressed(KEY_PERIOD) ? 1 : -1) * (shift ? 10 : 1);
        chart.secs[secIdx].bpm += d;
        if(chart.secs[secIdx].bpm < 10) chart.secs[secIdx].bpm = 10;
        if(chart.secs[secIdx].bpm > 300) chart.secs[secIdx].bpm = 300;
        return;
    }
    if(IsKeyPressed(KEY_ESCAPE)) {
        if(focusBox != NULL) {
            focusBox = NULL;
            return;
        }
        Editor_RequestLeave();
        return;
    }
}

static void Edit_Update(void) {
    if(ceDlg != DLG_NONE)
        return;
    // volume global (+/-) vale pro OUVIR tambem (so reaplica se mudou)
    {
        float v = RayGame_MusicVolumeLevel();
        if(v != edVolApplied) {
            edVolApplied = v;
            Audio_SetVolume(v);
        }
    }
    Edit_UpdateKeys();

    // roda a musica / relogio (tempo mestre = Inst, senao voz, senao silencio)
    if(playing) {
        Audio_Update();
        if(edHasAudio)
            cursorMs = GetMusicTimePlayed(edInst) * 1000.0;
        else if(edHasVoices)
            cursorMs = GetMusicTimePlayed(edVoices) * 1000.0;
        else if(edHasSplit)
            cursorMs = GetMusicTimePlayed(edSplitO) * 1000.0;
        else
            cursorMs += RayGame_DeltaTime() * 1000.0;
        if(cursorMs >= SongEndMs() + 2000)
            Audio_StopPlay();
        else if(playFollow)
            viewTopMs = cursorMs - VIEW_MS * 0.25; // trava no receptor, igual ao jogo
    }

    // setas e W/S sobem e descem a grade
    if(IsKeyDown(KEY_UP) || IsKeyDown(KEY_W))
        viewTopMs -= 2600 * RayGame_DeltaTime();
    if(IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S))
        viewTopMs += 2600 * RayGame_DeltaTime();
    if(viewTopMs < -500) viewTopMs = -500;
    {
        double maxTop = SongEndMs() + 1000;
        if(viewTopMs > maxTop) viewTopMs = maxTop;
    }

    // roda do mouse = sobe/desce
    float wheel = GetMouseWheelMove();
    if(wheel != 0) {
        Vector2 m = EdMouseWorld();
        int shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        int ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        if((shift || ctrl) && m.x >= GRID_X0 && m.x <= GRID_X0 + LANE_W * 8 && m.y >= GRID_Y0 && m.y <= GRID_Y1) {
            //na nota embaixo do mouse: muda a nota longa
            int lane = (int)((m.x - GRID_X0) / LANE_W);
            double t = viewTopMs + (m.y - GRID_Y0) / EdPxPerMs();
            int si = SecAtTime(t);
            EdSec* s = chart.secs + si;
            lane = EdDispLane(s, lane); // casa da tela -> casa do arquivo
            float beatMs = 60000.0f / s->bpm;
            for(int i = 0; i < s->count; i++) {
                if(s->notes[i].lane == lane && fabsf(s->notes[i].time - (float)t) < beatMs / 2) {
                    EdTouch();
                    s->notes[i].len += wheel * beatMs / 4;
                    if(s->notes[i].len < 0) s->notes[i].len = 0;
                    if(s->notes[i].len > 10000) s->notes[i].len = 10000;
                    break;
                }
            }
        } else {
            viewTopMs -= wheel * 150;
            if(viewTopMs < -500) viewTopMs = -500;
        }
    }
}

// ---------- acoes de trecho (usadas no menu e nas abas, sem duplicar) ----------
static void Edit_DoNewPart(void) {
    EdTouch();
    EdSec* last = chart.secs + chart.nsec - 1;
    EdSec* g = realloc(chart.secs, sizeof(EdSec) * (size_t)(chart.nsec + 1));
    if(g == NULL) return;
    chart.secs = g;
    EdSec* n = chart.secs + chart.nsec;
    memset(n, 0, sizeof(*n));
    n->mustHit = last->mustHit;
    n->bpm = last->bpm;
    n->lenSteps = 16;
    chart.nsec++;
    secIdx = chart.nsec - 1;
    SecTimes_Invalidate();
    cursorMs = SecStartMs(secIdx);
    viewTopMs = cursorMs - VIEW_MS * 0.3;
    if(viewTopMs < 0) viewTopMs = 0;
    EdStatusMsg("TRECHO NOVO NO FIM");
}

static void Edit_DoDelPart(void) {
    if(chart.nsec <= 1) { EdStatusMsg("NOT POSSIBLE TO DELETE HAVE ONLY 1 PART"); return; }
    EdTouch();
    free(chart.secs[secIdx].notes);
    memmove(chart.secs + secIdx, chart.secs + secIdx + 1, sizeof(EdSec) * (size_t)(chart.nsec - secIdx - 1));
    chart.nsec--;
    if(secIdx >= chart.nsec) secIdx = chart.nsec - 1;
    SecTimes_Invalidate();
    EdStatusMsg("part deleted");
}

static void Edit_DoCopy(void) {
    if(copyValid) free(copyBuf.notes);
    memset(&copyBuf, 0, sizeof(copyBuf));
    EdSec* s = chart.secs + secIdx;
    copyBuf.mustHit = s->mustHit;
    copyBuf.bpm = s->bpm;
    copyBuf.lenSteps = s->lenSteps;
    if(s->count > 0) {
        copyBuf.notes = malloc(sizeof(EdNote) * (size_t)s->count);
        if(copyBuf.notes != NULL) {
            copyBuf.count = copyBuf.cap = s->count;
            memcpy(copyBuf.notes, s->notes, sizeof(EdNote) * (size_t)s->count);
        }
    }
    copyValid = 1;
    EdStatusMsg("TRECHO COPIADO");
}

static void Edit_DoPaste(void) {
    if(!copyValid) { EdStatusMsg("COPIE ANTES (C)"); return; }
    EdTouch();
    double base = SongEndMs();
    double srcStart = SecStartMs(secIdx);
    EdSec* g = realloc(chart.secs, sizeof(EdSec) * (size_t)(chart.nsec + 1));
    if(g == NULL) return;
    chart.secs = g;
    EdSec* n = chart.secs + chart.nsec;
    memset(n, 0, sizeof(*n));
    n->mustHit = copyBuf.mustHit;
    n->bpm = chart.secs[chart.nsec - 1].bpm;
    n->lenSteps = copyBuf.lenSteps;
    // copy ja vem ordenado+dedupado: anexa puro (mantem ordem no shift)
    for(int i = 0; i < copyBuf.count; i++)
        EdSec_AddBulk(n, (float)(base + (copyBuf.notes[i].time - srcStart)), copyBuf.notes[i].lane, copyBuf.notes[i].len);
    chart.nsec++;
    secIdx = chart.nsec - 1;
    SecTimes_Invalidate();
    cursorMs = SecStartMs(secIdx);
    viewTopMs = cursorMs - VIEW_MS * 0.3;
    if(viewTopMs < 0) viewTopMs = 0;
    EdStatusMsg("COLADO NO FIM");
}

static void Edit_DoTestPlay(void) {
    Audio_StopPlay();
    Editor_Save();
    PlayState_SetSongDir(chart.dir);
    PlayState_SetReturnEditor(1);
    PlayState_SetScene();
}

static void Edit_DrawMenuBar(void) {
    DrawRectangle(0, 0, 1280, (int)MENU_H, (Color){16,16,16,255});
    DrawLine(0, (int)MENU_H, 1280, (int)MENU_H, (Color){60,60,60,255});
    Vector2 m = EdMouseWorld();
    const char* names[3] = {"File", "Edit", "View"};
    for(int i = 0; i < 3; i++) {
        Rectangle r = {(float)(PANEL_X + i * 70), 0, 68, MENU_H};
        char hov = (m.x >= r.x && m.x <= r.x + r.width && m.y >= r.y && m.y <= r.y + r.height);
        char open = (menuOpen == i + 1);
        DrawRectangleRec(r, (open || hov) ? (Color){60,60,60,255} : (Color){16,16,16,255});
        DrawTextEx(mainFont, names[i], (Vector2){r.x + 12, 5}, 20, 1, open ? GREEN : WHITE);
        if(hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            menuOpen = open ? 0 : (i + 1);
            activeDrop = 0;
            focusBox = NULL;
            focusNum = NULL;
        }
    }
    if(menuOpen == 0) return;
    float mx = (float)(PANEL_X + (menuOpen - 1) * 70);
    if(menuOpen == 1) {
        Rectangle box = {mx, MENU_H, 230, 5 * 30 + 8};
        DrawRectangleRec(box, (Color){15,15,15,245});
        DrawRectangleLinesEx(box, 1, GREEN);
        if(EdMenuItem((Rectangle){box.x + 4, box.y + 4, 222, 28}, "Save  Ctrl+S")) { menuOpen = 0; Editor_Save(); }
        else if(EdMenuItem((Rectangle){box.x + 4, box.y + 32, 222, 28}, "Save As...")) { menuOpen = 0; saveAsBox.buf[0] = 0; importPath[0] = 0; ceDlg = DLG_SAVEAS; focusBox = &saveAsBox; }
        else if(EdMenuItem((Rectangle){box.x + 4, box.y + 60, 222, 28}, "Save Compressed")) { menuOpen = 0; EdChart_SaveCompressed(&chart); EdStatusMsg("SALVO COMPRIMIDO"); }
        else if(EdMenuItem((Rectangle){box.x + 4, box.y + 88, 222, 28}, "Test Play  F5")) { menuOpen = 0; Edit_DoTestPlay(); }
        else if(EdMenuItem((Rectangle){box.x + 4, box.y + 116, 222, 28}, "Back to list  ESC")) { menuOpen = 0; Editor_RequestLeave(); }
        else if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if(!(m.x >= box.x && m.x <= box.x + box.width && m.y >= 0 && m.y <= box.y + box.height)) menuOpen = 0;
        }
    } else if(menuOpen == 2) {
        Rectangle box = {mx, MENU_H, 230, 5 * 30 + 8};
        DrawRectangleRec(box, (Color){15,15,15,245});
        DrawRectangleLinesEx(box, 1, GREEN);
        if(EdMenuItem((Rectangle){box.x + 4, box.y + 4, 222, 28}, "Undo  Ctrl+Z")) { menuOpen = 0; Undo_Pop(); chart.dirty = 1; }
        else if(EdMenuItem((Rectangle){box.x + 4, box.y + 32, 222, 28}, "Copy part  C")) { menuOpen = 0; Edit_DoCopy(); }
        else if(EdMenuItem((Rectangle){box.x + 4, box.y + 60, 222, 28}, "Paste at end  V")) { menuOpen = 0; Edit_DoPaste(); }
        else if(EdMenuItem((Rectangle){box.x + 4, box.y + 88, 222, 28}, "New part  N")) { menuOpen = 0; Edit_DoNewPart(); }
        else if(EdMenuItem((Rectangle){box.x + 4, box.y + 116, 222, 28}, "Delete part  X")) { menuOpen = 0; Edit_DoDelPart(); }
        else if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if(!(m.x >= box.x && m.x <= box.x + box.width && m.y >= 0 && m.y <= box.y + box.height)) menuOpen = 0;
        }
    } else {
        Rectangle box = {mx, MENU_H, 230, 2 * 30 + 8};
        DrawRectangleRec(box, (Color){15,15,15,245});
        DrawRectangleLinesEx(box, 1, GREEN);
        const char* followName = playFollow ? "Note player : ON" : "Note player : OFF";
        if(EdMenuItem((Rectangle){box.x + 4, box.y + 4, 222, 28}, followName)) { menuOpen = 0; playFollow = !playFollow; }
        else if(EdMenuItem((Rectangle){box.x + 4, box.y + 32, 222, 28}, "Go to part start")) { menuOpen = 0; cursorMs = SecStartMs(secIdx); }
        else if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if(!(m.x >= box.x && m.x <= box.x + box.width && m.y >= 0 && m.y <= box.y + box.height)) menuOpen = 0;
        }
    }
}

// abas laterais (uma ativa por vez)
static void Edit_DrawTabs(void) {
    float x0 = PANEL_X;
    float y0 = 34;
    float tw = 102;
    Vector2 m = EdMouseWorld();
    for(int t = 0; t < TAB_COUNT; t++) {
        Rectangle r = {x0 + t * (tw + 4), y0, tw, 26};
        char active = (activeTab == t);
        char hov = (m.x >= r.x && m.x <= r.x + r.width && m.y >= r.y && m.y <= r.y + r.height);
        DrawRectangleRec(r, active ? (Color){45,90,45,255} : hov ? (Color){60,60,60,255} : (Color){28,28,28,255});
        DrawRectangleLinesEx(r, 1, active ? GREEN : GRAY);
        Vector2 s = MeasureTextEx(mainFont, ChartTab_Name(t), 18, 1);
        DrawTextEx(mainFont, ChartTab_Name(t), (Vector2){r.x + (r.width - s.x) / 2, r.y + 4}, 18, 1, active ? GREEN : WHITE);
        if(hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && ceDlg == DLG_NONE && !activeDrop) {
            activeTab = t;
            menuOpen = 0;
            focusBox = NULL;
            focusNum = NULL;
        }
    }
}

#define SONG_FIELD_X 132.0f // largura da coluna dos rotulos no painel Song

/* Linha "Rotulo: [caixa] [v]" com lista suspensa. */
static void DrawDropField(float x, float y, float boxW, const char* label, TxtBox* box, const char* hint,
                          int dropId, char (*names)[SONG_NAME_MAX], int count, char blocked) {
    DrawTextEx(mainFont, label, (Vector2){x, y + 4}, 22, 1, WHITE);

    const Rectangle field = {x + SONG_FIELD_X, y, boxW, 32};
    TxtBox_Update(box, field, blocked);
    TxtBox_Draw(box, field, hint);

    const Rectangle button = {field.x + boxW + 6, y, 30, 32};
    if(EdDropBtn(button, activeDrop == dropId)) {
        if(activeDrop == dropId) {
            activeDrop = DROP_NONE;
        } else {
            activeDrop = dropId;
            dropBox = box;
            dropNames = names;
            dropCount = count;
            dropX = button.x;
            dropY = button.y + button.height + 4;
            dropW = boxW + 36;
            dropJustOpened = 1;
        }
        focusBox = NULL;
        focusNum = NULL;
    }
}

/* Largura das caixas: a do texto mais longo, entre 140 e 240. */
static float SongFieldWidth(void) {
    const char* texts[5] = {
        p1Box.buf[0] ? p1Box.buf : "bf",
        p2Box.buf[0] ? p2Box.buf : "dad",
        gfBox.buf[0] ? gfBox.buf : "gf",
        stageBox.buf[0] ? stageBox.buf : "stage",
        skinBox.buf[0] ? skinBox.buf : "default",
    };
    float width = 140;
    for(int i = 0; i < 5; i++) {
        const float w = MeasureTextEx(mainFont, texts[i], 20, 1).x + 24;
        if(w > width) width = w;
    }
    return width > 240 ? 240 : width;
}

static void Edit_DrawPanel_Song(float x, float y) {
    const char blocked = (ceDlg != DLG_NONE || activeDrop || menuOpen);
    const float boxW = SongFieldWidth();

    DrawTextEx(mainFont, "SONG", (Vector2){x, y}, 22, 1, GREEN);
    y += 34;
    DrawDropField(x, y, boxW, "Boyfriend:", &p1Box, "bf", DROP_BOYFRIEND, charNames, charCount, blocked);
    y += 38;
    DrawDropField(x, y, boxW, "Opponent:", &p2Box, "dad", DROP_OPPONENT, charNames, charCount, blocked);
    y += 38;
    DrawDropField(x, y, boxW, "Girlfriend:", &gfBox, "gf", DROP_GIRLFRIEND, gfNames, gfCount, blocked);
    y += 38;
    DrawDropField(x, y, boxW, "Stage:", &stageBox, "stage", DROP_STAGE, stageNames, stageCount, blocked);
    y += 38;
    DrawDropField(x, y, boxW, "Note Skin:", &skinBox, "default", DROP_SKIN, skinNames, skinCount, blocked);
    y += 42;

    DrawTextEx(mainFont, "bpm:", (Vector2){x, y + 4}, 22, 1, WHITE);
    {
        Rectangle r = {x + SONG_FIELD_X, y, 80, 32};
        if(focusNum != &nbTopBpm) NumBox_Refresh(&nbTopBpm, chart.bpm, 0);
        NumBox_Update(&nbTopBpm, r, blocked);
        NumBox_Draw(&nbTopBpm, r);
    }
    DrawTextEx(mainFont, "speed:", (Vector2){x + 230, y + 4}, 22, 1, WHITE);
    {
        Rectangle r = {x + 310, y, 70, 32};
        if(focusNum != &nbSpeed) NumBox_Refresh(&nbSpeed, chart.speed, 0);
        NumBox_Update(&nbSpeed, r, blocked);
        NumBox_Draw(&nbSpeed, r);
    }
    y += 44;

    if(EdButton((Rectangle){x, y, 190, 40}, "SALVAR", 22)) Editor_Save();
    if(EdButton((Rectangle){x + 200, y, 190, 40}, "SALVAR COMO", 20)) {
        saveAsBox.buf[0] = 0; importPath[0] = 0; ceDlg = DLG_SAVEAS; focusBox = &saveAsBox;
    }
}

static void Edit_DrawPanel_Section(float x, float y) {
    char blocked = (ceDlg != DLG_NONE || activeDrop || menuOpen);
    char buf[96];
    SecFollowCursor();
    snprintf(buf, sizeof(buf), "SECTION %d/%d", secIdx + 1, chart.nsec);
    DrawTextEx(mainFont, buf, (Vector2){x, y}, 22, 1, GREEN);
    y += 32;
    DrawTextEx(mainFont, "turn (TAB):", (Vector2){x, y + 4}, 22, 1, WHITE);
    if(EdButton((Rectangle){x + 150, y, 180, 32}, chart.secs[secIdx].mustHit ? "boyfriend" : "opponent", 20)) {
        EdTouch(); chart.secs[secIdx].mustHit = !chart.secs[secIdx].mustHit;
    }
    y += 42;
    // steps = tamanho do trecho em passos (16 = padrao, 4 tempos). Era "size:".
    DrawTextEx(mainFont, "steps:", (Vector2){x, y + 4}, 22, 1, WHITE);
    {
        Rectangle r = {x + 150, y, 90, 32};
        if(focusNum != &nbLen) NumBox_Refresh(&nbLen, (float)chart.secs[secIdx].lenSteps, 1);
        NumBox_Update(&nbLen, r, blocked);
        NumBox_Draw(&nbLen, r);
    }
    DrawTextEx(mainFont, "16=4 tempos", (Vector2){x + 250, y + 8}, 18, 1, YELLOW);
    y += 42;
    DrawTextEx(mainFont, "bpm:", (Vector2){x, y + 4}, 22, 1, WHITE);
    {
        Rectangle r = {x + 150, y, 90, 32};
        if(focusNum != &nbSecBpm) NumBox_Refresh(&nbSecBpm, chart.secs[secIdx].bpm, 0);
        NumBox_Update(&nbSecBpm, r, blocked);
        NumBox_Draw(&nbSecBpm, r);
    }
    y += 46;
    if(EdButton((Rectangle){x, y, 120, 36}, "+part", 20)) Edit_DoNewPart();
    if(EdButton((Rectangle){x + 130, y, 120, 36}, "X part", 20)) Edit_DoDelPart();
    y += 44;
    if(EdButton((Rectangle){x, y, 120, 36}, "copy", 20)) Edit_DoCopy();
    if(EdButton((Rectangle){x + 130, y, 120, 36}, "paste", 20)) Edit_DoPaste();
    y += 44;
}

static void Edit_DrawPanel_Note(float x, float y) {
    char buf[128];
    SecFollowCursor();
    DrawTextEx(mainFont, "NOTE", (Vector2){x, y}, 22, 1, GREEN);
    y += 32;
    char tbuf[32];
    EdTimeFmt(tbuf, sizeof(tbuf), cursorMs);
    snprintf(buf, sizeof(buf), "cursor: %s", tbuf);
    DrawTextEx(mainFont, buf, (Vector2){x, y}, 20, 1, WHITE);
    y += 30;
    snprintf(buf, sizeof(buf), "in part: %d notes", chart.secs[secIdx].count);
    DrawTextEx(mainFont, buf, (Vector2){x, y}, 20, 1, WHITE);
    y += 30;
    int total = 0;
    for(int i = 0; i < chart.nsec; i++) total += chart.secs[i].count;
    snprintf(buf, sizeof(buf), "total: %d notes", total);
    DrawTextEx(mainFont, buf, (Vector2){x, y}, 20, 1, WHITE);
    y += 36;
    DrawTextEx(mainFont, "click adds  right-click deletes", (Vector2){x, y}, 18, 1, YELLOW);
    y += 24;
    DrawTextEx(mainFont, "SHIFT/CTRL+wheel = long note", (Vector2){x, y}, 18, 1, YELLOW);
}

static void Edit_DrawPanel_Data(float x, float y) {
    char buf[160];
    DrawTextEx(mainFont, "DATA", (Vector2){x, y}, 22, 1, GREEN);
    y += 32;
    snprintf(buf, sizeof(buf), "parts: %d", chart.nsec);
    DrawTextEx(mainFont, buf, (Vector2){x, y}, 20, 1, WHITE);
    y += 30;
    int total = 0;
    for(int i = 0; i < chart.nsec; i++) total += chart.secs[i].count;
    snprintf(buf, sizeof(buf), "notes: %d", total);
    DrawTextEx(mainFont, buf, (Vector2){x, y}, 20, 1, WHITE);
    y += 30;
    char tbuf[32];
    EdTimeFmt(tbuf, sizeof(tbuf), SongEndMs());
    snprintf(buf, sizeof(buf), "length: %s", tbuf);
    DrawTextEx(mainFont, buf, (Vector2){x, y}, 20, 1, WHITE);
    y += 30;
    snprintf(buf, sizeof(buf), "dir: %s", chart.dir[0] ? chart.dir : "(unsaved)");
    DrawTextEx(mainFont, buf, (Vector2){x, y}, 18, 1, YELLOW);
    y += 28;
    snprintf(buf, sizeof(buf), "bpm %.1f  speed %.2f%s", (double)chart.bpm, (double)chart.speed, chart.dirty ? " *" : "");
    DrawTextEx(mainFont, buf, (Vector2){x, y}, 18, 1, YELLOW);
}

static void Edit_DrawPanel_Charting(float x, float y) {
    DrawTextEx(mainFont, "CHARTING", (Vector2){x, y}, 22, 1, GREEN);
    y += 34;
    if(EdButton((Rectangle){x, y, 190, 40}, playing ? "PAUSAR" : "OUVIR", 22)) Audio_Toggle();
    y += 50;
    if(EdButton((Rectangle){x, y, 190, 40}, "play (F5)", 22)) Edit_DoTestPlay();
    y += 50;
    DrawTextEx(mainFont, "space = play/stop", (Vector2){x, y}, 18, 1, YELLOW);
    y += 24;
    DrawTextEx(mainFont, "drag red bar = seek", (Vector2){x, y}, 18, 1, YELLOW);
    y += 24;
    DrawTextEx(mainFont, "W/S or wheel = scroll", (Vector2){x, y}, 18, 1, YELLOW);
}

static void Edit_DrawPanel_Events(float x, float y) {
    DrawTextEx(mainFont, "EVENTS", (Vector2){x, y}, 22, 1, GREEN);
    y += 34;
    DrawTextEx(mainFont, "events.json ignorado", (Vector2){x, y}, 20, 1, YELLOW);
    y += 30;
    DrawTextEx(mainFont, "(Song.c nao le events,", (Vector2){x, y}, 18, 1, YELLOW);
    y += 24;
    DrawTextEx(mainFont, "compat Psych so p/ notes)", (Vector2){x, y}, 18, 1, YELLOW);
}

static void Edit_DrawPanel(void) {
    float x = PANEL_X;
    float y = 70;
    SecFollowCursor();
    // sem bloco de fundo (pedido do dono): painel direto no bg, ajuda vai p/ baixo
    DrawLine((int)(PANEL_X - 8), 62, (int)(PANEL_X - 8), 672, (Color){60,60,60,255});
    char titleBuf[96];
    snprintf(titleBuf, sizeof(titleBuf), "%s%s", chart.name, chart.dirty ? "*" : "");
    // trunca nome longo p/ nao invadir OPEN nem a grade
    for(int guard = 0; guard < 32; guard++) {
        Vector2 ts = MeasureTextEx(mainFont, titleBuf, 24, 2);
        if(ts.x <= 280 || titleBuf[0] == 0) break;
        size_t L = strlen(titleBuf);
        if(L > 4) { titleBuf[L - 4] = '.'; titleBuf[L - 3] = '.'; titleBuf[L - 2] = '.'; titleBuf[L - 1] = 0; }
        else break;
    }
    DrawTextEx(mainFont, titleBuf, (Vector2){x, 66}, 24, 2, WHITE);
    // OPEN ao lado da musica: volta p/ lista p/ abrir outra (mesmo que ESC)
    if(EdButton((Rectangle){x + 300, 62, 110, 32}, "OPEN", 20)) {
        Editor_RequestLeave();
        return;
    }
    if(activeTab == TAB_SONG) Edit_DrawPanel_Song(x, y + 30);
    else if(activeTab == TAB_SECTION) Edit_DrawPanel_Section(x, y + 30);
    else if(activeTab == TAB_NOTE) Edit_DrawPanel_Note(x, y + 30);
    else if(activeTab == TAB_DATA) Edit_DrawPanel_Data(x, y + 30);
    else if(activeTab == TAB_CHARTING) Edit_DrawPanel_Charting(x, y + 30);
    else Edit_DrawPanel_Events(x, y + 30);
    // ajuda no espaco de baixo, sem bloco: limao com sombra p/ destacar no fundo roxo
    {
        Color help = (Color){190, 255, 90, 255};
        Color shade = (Color){0, 0, 0, 200};
        const char* hs[4] = {
            "space play  F5 test  CTRL+S save  TAB turn  T grid",
            "W/S scroll  N/X part  C/V copy/paste  ,/. bpm  ESC list",
            "click adds  right-click deletes  wheel = scroll",
            "G/H prev/next part  HOME goto part start"
        };
        float hy[4] = {648, 668, 688, 708};
        for(int hi = 0; hi < 4; hi++) {
            DrawTextEx(mainFont, hs[hi], (Vector2){PANEL_X + 2, hy[hi] + 2}, 15, 1, shade);
            DrawTextEx(mainFont, hs[hi], (Vector2){PANEL_X, hy[hi]}, 15, 1, help);
        }
    }
}

// (removido: migrado p/ abas Song/Section)

static void Edit_Draw(void) {
    Render_SetCamera(&camEd);
    Render_DrawGraphicObject(&bgEd);

    // tempo no canto + status
    char tbuf[32];
    EdTimeFmt(tbuf, sizeof(tbuf), cursorMs);
    DrawTextEx(mainFont, tbuf, (Vector2) {1110, 2}, 24, 2, WHITE);
    // status embaixo da grade (antes caia em cima do painel, lado direito)
    if(statusTimer > 0) {
        DrawTextEx(mainFont, statusText, (Vector2) {GRID_X0 + 2, 692}, 20, 1, (Color){0, 0, 0, 200});
        DrawTextEx(mainFont, statusText, (Vector2) {GRID_X0, 690}, 20, 1, YELLOW);
    }

    Edit_DrawGrid();
    Edit_DrawTabs();
    Edit_DrawPanel();
    Edit_DrawMenuBar();
    CharDrop_Draw();
    Render_StopCamera();
}

// ---------- cena ----------
static void ChartEditor_Create([[maybe_unused]] RayScene* scene) {
    Render_DefaultRGT(&bgEd);
    bgEd.objType = RGT_IMAGE;
    bgEd.image = Render_LoadTexture("assets/images/freeplay/menuBGBlue.png");

    camEd.target = (Vector2) {1280 / 2, 720 / 2};
    camEd.zoom = 1;
    camEd.rotation = 0;
    camEd.offset = (Vector2) {0, 0};

    EdArrows_Init();

    if(pendingDir[0] != 0) {
        char tmp[256];
        strncpy(tmp, pendingDir, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = 0;
        pendingDir[0] = 0;
        ceMode = CE_PICKER;
        Picker_Refresh();
        Editor_EnterDir(tmp);
    } else {
        ceMode = CE_PICKER;
        ceDlg = DLG_NONE;
        chartLoaded = 0;
        Picker_Refresh();
    }
    openBox.buf[0] = 0;
    saveAsBox.buf[0] = 0;
    importPath[0] = 0;
    focusBox = NULL;
    statusText[0] = 0;
    statusTimer = 0;
}

static void ChartEditor_Update([[maybe_unused]] RayScene* scene) {
    if(statusTimer > 0)
        statusTimer -= RayGame_DeltaTime();
    if(ceMode == CE_PICKER)
        Picker_Update();
    else
        Edit_Update();
    Dialogs_Update();
}

static void ChartEditor_Draw([[maybe_unused]] RayScene* scene) {
    if(ceMode == CE_PICKER)
        Picker_Draw();
    else
        Edit_Draw();
    Dialogs_Draw();
    if(statusTimer > 0 && ceMode == CE_PICKER)
        DrawTextEx(mainFont, statusText, (Vector2) {150, 640}, 24, 2, YELLOW);
}

static void ChartEditor_Destroy([[maybe_unused]] RayScene* scene) {
    UnloadTexture(bgEd.image);
    SecTimes_Drop();
    SongList_Free(pickerSongs);
    pickerSongs = NULL;
    pickerCount = 0;
    Audio_StopPlay();
    if(edHasAudio) {
        UnloadMusicStream(edInst);
        edHasAudio = 0;
    }
    if(edHasVoices) {
        UnloadMusicStream(edVoices);
        edHasVoices = 0;
    }
    if(edHasSplit) {
        UnloadMusicStream(edSplitO);
        UnloadMusicStream(edSplitP);
        edHasSplit = 0;
    }
    Editor_UnloadIcons();
    EdChart_Free(&chart);
    chartLoaded = 0;
    Undo_Clear();
    if(copyValid) {
        free(copyBuf.notes);
        memset(&copyBuf, 0, sizeof(copyBuf));
        copyValid = 0;
    }
    focusBox = NULL;
}

void ChartEditor_SetSongDir(const char* dir) {
    strncpy(pendingDir, dir, sizeof(pendingDir) - 1);
    pendingDir[sizeof(pendingDir) - 1] = 0;
}

static RayScene chartScene;

Scene_MakeSceneCode(
    chartScene,
    ChartEditor_SetScene,
    ChartEditor_Create,
    ChartEditor_Update,
    ChartEditor_Draw,
    ChartEditor_Destroy
)
