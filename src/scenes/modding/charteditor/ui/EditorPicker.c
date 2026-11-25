/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: lista de musicas, abrir/salvar/importar e o estado da sessao de edicao. */
#include "scenes/api/FontLoader.h"
#include "scenes/menu/MenuState.h"
#include"data/song/Song.h"
#include"data/song/SongList.h"
#include "core/ModPath.h"
#include "Game.h"
#include "raymath.h"
#include "scenes/modding/charteditor/io/EditorDataAndIO.h"
#include "scenes/modding/charteditor/grid/EditorTimeline.h"
#include "scenes/modding/charteditor/audio/EditorAudio.h"
#include "scenes/modding/charteditor/ui/EditorWidgets.h"
#include "scenes/modding/charteditor/ui/EditorPicker.h"
#include "scenes/modding/charteditor/lists/EditorCharactersAndLists.h"
#include "scenes/modding/charteditor/ui/EditorIcons.h"
#include "scenes/menu/freeplay/FreeplayIcon.h"
#include "ui/SearchBox.h"

const char* ChartTab_Name(int t) {
    switch(t) {
        case TAB_CHARTING: return "Charting";
        case TAB_DATA: return "Data";
        case TAB_EVENTS: return "Events";
        case TAB_SECTION: return "Section";
        case TAB_SONG: return "Song";
        default: return "?";
    }
}

CeMode ceMode;

int activeTab = TAB_SECTION;

static int pickerSel = 0; // 0 = open file, 1..N = songs

static float pickerOffset = 0;

TxtBox openBox;

TxtBox saveAsBox;

char pendingDir[256]; // entrar direto no editor (volta do teste)

EdSec copyBuf;

char copyValid = 0;

RayGraphicObject bgEd;

// ---- lista de musicas: icones vem do FreeplayIcon (resolve e decodifica em fundo; so o envio roda aqui)
#define PICK_ICON_SIZE 46.0f
#define PICK_ROW_STEP 66.0f      /* altura de cada linha */
#define PICK_ROW_CY 372.0f       /* centro vertical da linha selecionada */
#define PICK_ROW_X 150.0f        /* x da coluna de icones */
#define PICK_WINDOW 9            /* linhas para cada lado da selecao; alem disso ja saiu da tela */
#define PICK_WARM 14             /* vizinhos pedidos em fundo quando a selecao muda */
#define PICK_PAGE 10             /* linhas por pagina (PgUp/PgDn) */
#define PICK_MAX_VISIBLE 32
#define PICK_TRACK_X 890.0f      /* barra de rolagem da lista */

/* Geometria de uma linha da lista, calculada uma vez por frame e usada nas passadas de desenho. */
typedef struct {
    const char* name;            /* rotulo pre-montado (pickLabels), sem copia por frame */
    const SongEntry* song;       /* NULL na linha "open file directory" */
    Vector2 pos;                 /* canto do texto */
    Rectangle box;               /* icone (so musicas) */
    Texture2D icon;              /* id 0 = ainda sem textura */
    float size;
    float sel;                   /* 1 = selecionada, 0 = longe: dirige destaque, recuo e alpha */
    unsigned char a;
    Color fill, out;
} PickRow;

static SearchBox pickSearch;
static bool pickSearchInit;
static int* pickShown;          // indices de pickerSongs que passam no filtro
static int pickShownCount;
static char (*pickLabels)[128]; // rotulo pronto por musica ("[mod]" incluso), montado so ao escanear
static int pickWarmSel = -1;    // selecao ja aquecida (vizinhos so são pedidos quando ela muda)

// Solta o cache de icones e os rotulos (a textura fica no cache do FreeplayIcon ate ser despejada)
static void Picker_FreeIcons(void) {
    FreeplayIcon_Unload();
    free(pickLabels);
    pickLabels = NULL;
    pickWarmSel = -1;
}

void Picker_Free(void) {
    StageList_Invalidate();
    Picker_FreeIcons();
    free(pickShown);
    pickShown = NULL;
    pickShownCount = 0;
}

static void Picker_ApplyFilter(void) {
    pickShownCount = 0;
    for(int i = 0; i < pickerCount; i++)
        if(SearchBox_Match(&pickSearch, pickerSongs[i].name))
            pickShown[pickShownCount++] = i;
    pickerSel = 0;
    pickerOffset = 0;
}

void Picker_Refresh(void) {
    Picker_Free();
    SongList_Free(pickerSongs);
    pickerSongs = NULL;
    pickerCount = SongList_Scan(&pickerSongs);
    if(!pickSearchInit) {
        SearchBox_Init(&pickSearch, (Rectangle) {640.0f, 52.0f, 460.0f, 44.0f});
        pickSearchInit = true;
    }
    if(pickerCount > 0) {
        pickShown = malloc(sizeof(int) * (size_t)pickerCount);
        pickLabels = malloc(sizeof(*pickLabels) * (size_t)pickerCount);
        if(!pickShown || !pickLabels) {
            Picker_Free();
            SongList_Free(pickerSongs);
            pickerSongs = NULL;
            pickerCount = 0;
        } else {
            for(int i = 0; i < pickerCount; i++)
                snprintf(pickLabels[i], sizeof(pickLabels[i]), "%s%s", pickerSongs[i].name,
                    strstr(pickerSongs[i].dir, "mods") != NULL ? "  [mod]" : "");
            FreeplayIcon_Init(pickerSongs, pickerCount);
        }
    }
    Picker_ApplyFilter();
}

static Rectangle Picker_SquareCrop(Texture2D t) {
    const float side = (float)(t.height < t.width ? t.height : t.width);
    return (Rectangle) {0.0f, 0.0f, side, side};
}

// Existe <dir>/<base>.(ogg|mp3|wav)? Mesma regra da gameplay (PlayStateAudio).
static char PlayFileExists(const char* dir, const char* base) {
    static const char* const exts[] = {"ogg", "mp3", "wav"};
    char p[300];
    for(size_t i = 0; i < sizeof(exts) / sizeof(exts[0]); i++) {
        snprintf(p, sizeof(p), "%s/%s.%s", dir, base, exts[i]);
        if(FileExists(p)) return 1;
    }
    return 0;
}

void Editor_EnterDir(const char* dir) {
    if(!EdChart_OpenDir(&chart, dir)) {
        EdStatusMsg("COULD NOT OPEN THIS SONG");
        return;
    }
    Picker_FreeIcons();
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
    Lists_Invalidate();   // listas do dropdown: so escaneiam quando abrir
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
    // arquivos do test play (mesmas extensoes que a gameplay aceita): so pintam os botoes PLAY ONLY
    edSoloMix = 0;
    {
        const char hasMain = PlayFileExists(dir, "Voices");
        const char hasPlayer = PlayFileExists(dir, "Voices-Player");
        const char hasOpp = PlayFileExists(dir, "Voices-Opponent");
        edPlayHasInst = PlayFileExists(dir, "Inst");
        edPlayHasVocals = hasMain || hasPlayer || hasOpp;
        edPlayHasBf = hasPlayer;
        edPlayHasOpp = hasOpp;
    }
    ceMode = CE_EDIT;
    ceDlg = DLG_NONE;
    focusBox = NULL;

    // set custom note skin for editor preview
    Editor_ApplyNoteSkin(chart.noteSkin, dir);

    EdStatusMsg("opened :)");
}

void Editor_Save(void) {
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
        EdStatusMsg("SAVED!");
    } else {
        EdStatusMsg("ERROR SAVING");
    }
}

// importa json solto -> vira pasta de mod -> abre (1 = ok)
char ImportJSON(const char* jsonPath, const char* modName) {
    char clean[64];
    EdSanitizeName(clean, sizeof(clean), modName);
    if(clean[0] == 0) {
        EdStatusMsg("INVALID NAME");
        return 0;
    }
    char dir[256];
    snprintf(dir, sizeof(dir), "assets/mods/%s", clean);
    if(Song_HasChart(dir)) {
        EdStatusMsg("NAME ALREADY EXISTS");
        return 0;
    }
    MakeDirectory(dir);
    char dst[300];
    snprintf(dst, sizeof(dst), "%s/data.json", dir);
    if(!EdCopyFile(jsonPath, dst)) {
        EdStatusMsg("COULD NOT COPY");
        return 0;
    }
    Editor_EnterDir(dir);
    return 1;
}

// SAVE AS: copia audio + salva json com outro nome (1 = ok)
char Editor_SaveAs(const char* modName) {
    char clean[64];
    EdSanitizeName(clean, sizeof(clean), modName);
    if(clean[0] == 0) {
        EdStatusMsg("INVALID NAME");
        return 0;
    }
    char dir[256];
    snprintf(dir, sizeof(dir), "assets/mods/%s", clean);
    if(strcmp(dir, chart.dir) == 0) {
        Editor_Save();
        return 1;
    }
    if(Song_HasChart(dir)) {
        EdStatusMsg("NAME ALREADY EXISTS");
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
            EdStatusMsg("FOLDER RENAMED");
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

/* Pesquisa da lista suspensa: digitou, filtra na hora (sem Enter). So existe com a lista aberta;
   fechada, CharDrop_Draw sai logo no inicio e nada daqui roda. */
static char dropQuery[24];
static int dropQueryLen;
static int dropFilt[NAME_LIST_MAX];   /* indices de dropNames que combinam */
static int dropFiltN;

static char DropFold(char c) {
    if(c >= 'A' && c <= 'Z') return (char)(c - 'A' + 'a');
    if(c == '-' || c == '_') return ' ';
    return c;
}

static char DropMatch(const char* name, const char* q) {
    for(const char* s = name; *s; ++s) {
        const char* a = s;
        const char* b = q;
        while(*b && *a && DropFold(*a) == DropFold(*b)) { ++a; ++b; }
        if(!*b) return 1;
    }
    return 0;
}

void CharDrop_Draw(void) {
    if(!activeDrop || dropBox == NULL || dropNames == NULL)
        return;
    float bx = dropX, by = dropY, bw = dropW;
    TxtBox* box = dropBox;
    char dirty = 0;

    if(dropJustOpened) {
        dropQuery[0] = 0;
        dropQueryLen = 0;
        dirty = 1;
        while(GetCharPressed() > 0) {}   // descarta teclas digitadas antes de abrir
    } else {
        int ch;
        while((ch = GetCharPressed()) > 0) {
            if(ch >= 32 && ch <= 126 && dropQueryLen < (int)sizeof(dropQuery) - 1) {
                dropQuery[dropQueryLen++] = (char)ch;
                dropQuery[dropQueryLen] = 0;
                dirty = 1;
            }
        }
        if((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && dropQueryLen > 0) {
            dropQuery[--dropQueryLen] = 0;
            dirty = 1;
        }
    }
    if(dirty) {
        dropFiltN = 0;
        if(dropQueryLen > 0) {
            for(int i = 0; i < dropCount && dropFiltN < NAME_LIST_MAX; i++)
                if(DropMatch(dropNames[i], dropQuery)) dropFilt[dropFiltN++] = i;
        }
        dropScroll = 0;
    }
    const int n = dropQueryLen > 0 ? dropFiltN : dropCount;   // itens visiveis
    float ih = 30;
    int rows = n < DROP_ROWS_MAX ? n : DROP_ROWS_MAX;
    float h = (rows > 0 ? rows : 1) * ih + 8 + ih;   // + linha da pesquisa
    if(by + h > 700) by = 700 - h;
    Rectangle r = {bx - bw + 30, by, bw, h};
    if(r.x + r.width > 1270) r.x = 1270 - r.width;
    DrawRectangleRec(r, (Color) {15, 15, 15, 245});
    DrawRectangleLinesEx(r, 1, GREEN);
    Vector2 m = EdMouseWorld();
    const char inside = (m.x >= r.x && m.x <= r.x + r.width && m.y >= r.y && m.y <= r.y + r.height);

    // linha da pesquisa (topo)
    if(dropQueryLen > 0) {
        DrawTextEx(mainFont, dropQuery, (Vector2) {r.x + 10, r.y + 6}, 20, 1, YELLOW);
    } else {
        DrawTextEx(mainFont, "search...", (Vector2) {r.x + 10, r.y + 6}, 20, 1, GRAY);
    }
    DrawLine((int)(r.x + 4), (int)(r.y + ih + 2), (int)(r.x + r.width - 4), (int)(r.y + ih + 2), (Color) {70, 70, 70, 255});
    const float listY = r.y + ih;

    // roda: rola a lista (so com o mouse em cima dela)
    if(inside && n > rows) {
        dropScroll -= (int)GetMouseWheelMove();
        if(dropScroll > n - rows) dropScroll = n - rows;
        if(dropScroll < 0) dropScroll = 0;
    }

    if(n == 0) {
        DrawTextEx(mainFont, dropQueryLen > 0 ? "(no results)" : "(empty)", (Vector2) {r.x + 8, listY + 8}, 20, 1, YELLOW);
    }
    // o clique que abriu a lista nao pode escolher o item que ficou embaixo do mouse
    const char canPick = !dropJustOpened;
    for(int row = 0; row < rows; row++) {
        const int vi = dropScroll + row;
        const int i = dropQueryLen > 0 ? dropFilt[vi] : vi;
        Rectangle ir = {r.x + 4, listY + 4 + row * ih, r.width - 14, ih - 2};
        char hov = (m.x >= ir.x && m.x <= ir.x + ir.width && m.y >= ir.y && m.y <= ir.y + ir.height);
        if(hov)
            DrawRectangleRec(ir, (Color) {50, 90, 50, 255});
        DrawTextEx(mainFont, dropNames[i], (Vector2) {ir.x + 6, ir.y + 4}, 20, 1, WHITE);
        if(canPick && hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            strncpy(box->buf, dropNames[i], sizeof(box->buf) - 1);
            box->buf[sizeof(box->buf) - 1] = 0;
            // "default" na lista de skins = vazio (notas normais)
            if(activeDrop == DROP_SKIN && strcmp(box->buf, "default") == 0) box->buf[0] = 0;
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
        const float trackH = h - 8 - ih;
        const float thumbH = trackH * rows / n;
        const float thumbY = listY + 4 + (trackH - thumbH) * dropScroll / (float)(n - rows);
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
char importPath[512];

static void Picker_OpenSelected(void) {
    if(pickerSel == 0) {
        openBox.buf[0] = 0;
        ceDlg = DLG_OPEN;
        focusBox = &openBox;
    } else if(pickerSel >= 1 && pickerSel <= pickShownCount) {
        Editor_EnterDir(pickerSongs[pickShown[pickerSel - 1]].dir);
    }
}

static void Picker_NewSong(void) {
    EdChart_Default(&chart, "new-song");
    EdEvent_ClearAll();
    chart.dir[0] = 0;
    Picker_FreeIcons();
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
    Lists_Invalidate();
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
    EdStatusMsg("NEW SONG - SAVE AS");
}

/* Calcula a linha i; false se ela esta fora da tela. */
static bool Picker_RowGeom(int i, PickRow* r) {
    const float d = (float)i - pickerOffset;
    const float y = PICK_ROW_CY + d * PICK_ROW_STEP;
    if(y < 100.0f || y > 640.0f)
        return false;
    const float ad = fabsf(d);
    r->sel = fmaxf(0.0f, 1.0f - ad);                 /* 1 = selecionada, 0 = longe */
    const float ind = fminf(ad, 3.0f) * 22.0f;       /* recuo por distancia (como no freeplay) */
    r->song = (i == 0) ? NULL : &pickerSongs[pickShown[i - 1]];
    r->name = (i == 0) ? "open file directory..." : pickLabels[pickShown[i - 1]];
    r->size = (i == 0 ? 30.0f : 34.0f) + 10.0f * r->sel;
    const float textX = PICK_ROW_X + ind + (i == 0 ? 0.0f : PICK_ICON_SIZE + 16.0f);
    r->pos = (Vector2) {textX, y - r->size * 0.5f};
    r->box = (Rectangle) {PICK_ROW_X + ind, y - PICK_ICON_SIZE * 0.5f, PICK_ICON_SIZE, PICK_ICON_SIZE};
    r->icon = (Texture2D) {0};
    r->a = (unsigned char)(255.0f * fmaxf(0.25f, 1.0f - ad * 0.16f));
    const Color base = (i == 0) ? (Color) {150, 255, 150, 255} : (Color) {200, 196, 226, 255};
    r->fill = (Color) {
        (unsigned char)(base.r + (255 - base.r) * r->sel),
        (unsigned char)(base.g + (255 - base.g) * r->sel),
        (unsigned char)(base.b + (255 - base.b) * r->sel),
        r->a
    };
    r->out = (Color) {0, 0, 0, r->a};
    return true;
}

/* Passadas: formas (uma textura), icones (uma textura cada), textos (textura da fonte).
   Cada troca de textura ou de modo abre um draw call novo, entao agrupar por tipo reduz muito. */
void Picker_Draw(void) {
    Render_SetCamera(&camEd);
    Render_DrawGraphicObject(&bgEd);

    const int total = 1 + pickShownCount;
    const int first = (int)fmaxf(0.0f, floorf(pickerOffset - PICK_WINDOW));
    const int last = (int)fminf((float)(total - 1), ceilf(pickerOffset + PICK_WINDOW));
    PickRow rows[PICK_MAX_VISIBLE];
    int n = 0;
    for(int i = first; i <= last && n < PICK_MAX_VISIBLE; ++i)
        if(Picker_RowGeom(i, &rows[n])) ++n;

    // Icones: uma textura por musica visivel. Get so devolve o que ja foi decodificado em fundo.
    for(int k = 0; k < n; ++k) {
        if(rows[k].song != NULL)
            FreeplayIcon_Get(rows[k].song, &rows[k].icon);
    }
    // Backdrop: icone da musica selecionada, grande e esmaecido (mesmo truque do freeplay).
    if(pickerSel >= 1 && pickerSel <= pickShownCount) {
        Texture2D bgIcon = {0};
        if(FreeplayIcon_Get(&pickerSongs[pickShown[pickerSel - 1]], &bgIcon) == 1) {
            DrawTexturePro(bgIcon, Picker_SquareCrop(bgIcon), (Rectangle) {-60, -20, 1400, 1400}, (Vector2) {0, 0}, 0.0f,
                (Color) {255, 255, 255, 70});
        }
    }
    DrawRectangleGradientV(0, 0, 1280, 720, (Color) {8, 6, 18, 90}, (Color) {8, 6, 18, 210});

    // Cartao escuro (paleta do freeplay) e titulo com barra rosa
    DrawRectangleRounded((Rectangle) {130, 40, 780, 650}, 0.04f, 8, (Color) {12, 10, 26, 222});
    DrawRectangleRoundedLinesEx((Rectangle) {130, 40, 780, 650}, 0.04f, 8, 1.0f, (Color) {120, 100, 200, 150});
    DrawRectangleRounded((Rectangle) {150, 56, 6, 40}, 0.5f, 4, (Color) {255, 92, 176, 255});
    DrawTextEx(mainFont, "song selector", (Vector2) {166, 60}, 48, 4, WHITE);
    {
        // contador e pagina no cabecalho: 12 / 340 - pagina 2 de 34
        const int cur = pickerSel;
        const int pages = (total + PICK_PAGE - 1) / PICK_PAGE;
        char info[64];
        snprintf(info, sizeof(info), "%d / %d   page %d of %d", cur + 1 > total ? total : cur + 1, total,
            cur / PICK_PAGE + 1, pages < 1 ? 1 : pages);
        DrawTextEx(mainFont, info, (Vector2) {166, 112}, 20, 1, (Color) {150, 146, 178, 255});
    }

    // Passada de formas: pilula da linha selecionada (com leve fade nas vizinhas) e barra de destaque.
    for(int k = 0; k < n; ++k) {
        const PickRow* r = &rows[k];
        if(r->sel <= 0.01f) continue;
        const Rectangle pill = {r->box.x - 18.0f, r->box.y - 9.0f, 690.0f * (0.85f + 0.15f * r->sel), 64.0f};
        DrawRectangleRounded(pill, 0.5f, 12, Fade((Color) {40, 26, 72, 235}, r->sel));
        DrawRectangle((int)pill.x, (int)(pill.y + 10.0f), 5, 44, Fade((Color) {255, 92, 176, 255}, r->sel));
    }
    // Passada de icones: sem quadrado de fundo; so a imagem (e um contorno rosa na selecionada).
    for(int k = 0; k < n; ++k) {
        const PickRow* r = &rows[k];
        if(r->song == NULL || r->icon.id == 0) continue;
        DrawTexturePro(r->icon, Picker_SquareCrop(r->icon), r->box, (Vector2) {0, 0}, 0.0f,
            (Color) {255, 255, 255, r->a});
        if(r->sel > 0.5f)
            DrawRectangleRoundedLinesEx(r->box, 0.12f, 8, 2.0f, (Color) {255, 92, 176, 255});
    }
    // Passada de textos: sombra e texto (textura da fonte).
    for(int k = 0; k < n; ++k) {
        const PickRow* r = &rows[k];
        DrawTextEx(mainFont, r->name, (Vector2) {r->pos.x + 2.0f, r->pos.y + 3.0f}, r->size, 2.0f,
            (Color) {0, 0, 0, (unsigned char)(160.0f * r->sel + 40.0f)});
        DrawTextEx(mainFont, r->name, r->pos, r->size, 2.0f, r->fill);
    }

    // Barra de rolagem: tamanho proporcional a fatia visivel, posicao proporcional a selecao.
    if(total > PICK_WINDOW * 2 + 1) {
        const float trackY = 130.0f, trackH = 500.0f;
        const float thumbH = fmaxf(24.0f, trackH * (PICK_WINDOW * 2 + 1) / (float)total);
        const float thumbY = trackY + (trackH - thumbH) * (float)pickerSel / (float)(total - 1);
        DrawRectangleRounded((Rectangle) {PICK_TRACK_X, trackY, 4, trackH}, 0.5f, 4, (Color) {50, 44, 80, 255});
        DrawRectangleRounded((Rectangle) {PICK_TRACK_X, thumbY, 4, thumbH}, 0.5f, 4, (Color) {255, 92, 176, 255});
    }

    if(pickShownCount == 0 && pickerCount > 0)
        DrawTextEx(mainFont, "no song found", (Vector2) {260, 400}, 32, 2, (Color) {255, 200, 120, 255});
    DrawRectangleRounded((Rectangle) {140, 684, 760, 28}, 0.4f, 6, (Color) {20, 24, 36, 220});
    DrawTextEx(mainFont, "ENTER open   R refresh   N new   PGUP/PGDN page   ESC back",
        (Vector2) {156, 689}, 20, 1, (Color) {200, 200, 200, 255});

    // Caixas de busca e botoes por ultimo (formas e textos proprios).
    SearchBox_Draw(&pickSearch, mainFont);
    if(EdButton((Rectangle) {150, 630, 200, 40}, "OPEN", 22)) Picker_OpenSelected();
    if(EdButton((Rectangle) {360, 630, 200, 40}, "NEW", 22)) Picker_NewSong();
    Render_StopCamera();
}

/* Move a selecao com volta: passou do fim vai pro inicio e vice-versa. Ao dar a volta, a lista pula sem animar. */
static void Picker_Move(int delta, int rows) {
    const int prev = pickerSel;
    pickerSel = ((prev + delta) % rows + rows) % rows;
    const bool wrapped = delta > 0 ? pickerSel < prev : pickerSel > prev;
    if(wrapped) pickerOffset = (float)pickerSel;
}

/* Pula N linhas (pagina, roda rapida, pontas) sem dar a volta: prende nos limites. */
static void Picker_Jump(int delta, int rows) {
    pickerSel += delta;
    if(pickerSel < 0) pickerSel = 0;
    if(pickerSel > rows - 1) pickerSel = rows - 1;
}

/* Y do centro da linha i na tela (mesma conta de Picker_RowGeom). */
static float Picker_RowY(int i) {
    return PICK_ROW_CY + ((float)i - pickerOffset) * PICK_ROW_STEP;
}

void Picker_Update(void) {
    FreeplayIcon_Tick();   // resolve e decodifica em fundo; o desenho so envia para a GPU

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
                    EdStatusMsg("THIS JSON IS NOT A SONG");
                }
                break;
            }
        }
        UnloadDroppedFiles(drop);
    }

    if(ceDlg != DLG_NONE)
        return;

    // letras/Enter/Esc/Backspace pertencem ao campo enquanto ele esta focado
    const bool typing = SearchBox_Focused(&pickSearch);
    if(SearchBox_Update(&pickSearch)) Picker_ApplyFilter();

    const int rows = 1 + pickShownCount;
    if(IsKeyPressed(KEY_DOWN) || (!typing && IsKeyPressed(KEY_S)))
        Picker_Move(1, rows);
    if(IsKeyPressed(KEY_UP) || (!typing && IsKeyPressed(KEY_W)))
        Picker_Move(-1, rows);
    // paginacao: PgUp/PgDn pulam uma pagina (sem dar a volta); Home/End vao pras pontas
    if(!typing) {
        if(IsKeyPressed(KEY_PAGE_DOWN)) Picker_Jump(PICK_PAGE, rows);
        if(IsKeyPressed(KEY_PAGE_UP)) Picker_Jump(-PICK_PAGE, rows);
        if(IsKeyPressed(KEY_HOME)) Picker_Jump(-rows, rows);
        if(IsKeyPressed(KEY_END)) Picker_Jump(rows, rows);
    }
    const float wheel = GetMouseWheelMove();
    if(wheel != 0.0f) {
        const int step = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? 5 : 1;
        Picker_Jump(wheel > 0.0f ? -step : step, rows);
    }
    if(!typing && IsKeyPressed(KEY_R))
        Picker_Refresh();
    if(!typing && IsKeyPressed(KEY_N))
        Picker_NewSong();
    // vizinhos fora da tela ja comecam a resolver/decodificar quando a selecao muda
    if(pickerSel != pickWarmSel) {
        pickWarmSel = pickerSel;
        for(int i = pickerSel - PICK_WARM; i <= pickerSel + PICK_WARM; ++i)
            if(i >= 1 && i <= pickShownCount)
                FreeplayIcon_Warm(&pickerSongs[pickShown[i - 1]]);
    }
    if(!typing && (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE))) {
        RayGame_ResetMusic();
        RayGame_ToggleMusic(1);
        MenuState_SetScene();
        return;
    }
    if(!typing && (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER))) {
        Picker_OpenSelected();
        return;
    }
    // clique: 1o seleciona, 2o abre (OPEN ao lado da musica)
    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && ceDlg == DLG_NONE &&
       !CheckCollisionPointRec(EdMouseWorld(), pickSearch.bounds)) {
        Vector2 m = EdMouseWorld();
        for(int i = 0; i < rows; i++) {
            const float y = Picker_RowY(i);
            if(m.x >= PICK_ROW_X - 20.0f && m.x <= 880.0f && m.y >= y - 33.0f && m.y <= y + 33.0f) {
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
void Editor_LeaveToPicker(void) {
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
void Editor_RequestLeave(void) {
    if(chart.dirty) {
        ceDlg = DLG_LEAVE;
        focusBox = NULL;
        focusNum = NULL;
    } else {
        Editor_LeaveToPicker();
    }
}
