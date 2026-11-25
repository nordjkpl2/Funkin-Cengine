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

// ---- busca + icones da lista (so linhas perto da selecao ficam na GPU)
#define PICK_ICON_SIZE 56.0f
#define PICK_ICON_KEEP 10
#define PICK_WINDOW 8      /* linhas para cada lado da selecao; alem disso ja saiu da tela */
#define PICK_MAX_VISIBLE 32

/* Geometria de uma linha da lista, calculada uma vez por frame e usada nas passadas de desenho. */
typedef struct {
    char name[128];
    Vector2 pos;
    Rectangle fr, box;     /* quadradinho e icone (so i > 0) */
    Texture2D icon;
    float size;
    Color fill, out;
    unsigned char a;
    bool hasBox;
} PickRow;

static SearchBox pickSearch;
static bool pickSearchInit;
static int* pickShown;        // indices de pickerSongs que passam no filtro
static int pickShownCount;
static Texture2D* pickIcons;  // um por musica (id 0 = ainda nao carregou / sem icone)
static unsigned char* pickTried;
static int pickWinLo = 0, pickWinHi = -1; // janela (posicoes em pickShown) com icones tratados

static void Picker_FreeIcons(void) {
    if(pickIcons) {
        for(int i = 0; i < pickerCount; i++)
            if(pickIcons[i].id != 0) UnloadTexture(pickIcons[i]);
    }
    free(pickIcons);
    free(pickTried);
    pickIcons = NULL;
    pickTried = NULL;
    pickWinLo = 0;
    pickWinHi = -1;
}

// Solta todos os icones carregados (usado quando o filtro muda as posicoes)
static void Picker_ResetIcons(void) {
    if(pickIcons) {
        for(int i = 0; i < pickerCount; i++) {
            if(pickIcons[i].id != 0) UnloadTexture(pickIcons[i]);
            pickIcons[i] = (Texture2D) {0};
            pickTried[i] = 0;
        }
    }
    pickWinLo = 0;
    pickWinHi = -1;
}

void Picker_Free(void) {
    StageList_Invalidate();
    Picker_FreeIcons();
    free(pickShown);
    pickShown = NULL;
    pickShownCount = 0;
}

static void Picker_ApplyFilter(void) {
    Picker_ResetIcons();
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
        pickIcons = calloc((size_t)pickerCount, sizeof(Texture2D));
        pickTried = calloc((size_t)pickerCount, 1);
        if(!pickShown || !pickIcons || !pickTried) {
            Picker_Free();
            SongList_Free(pickerSongs);
            pickerSongs = NULL;
            pickerCount = 0;
        }
    }
    Picker_ApplyFilter();
}

// carrega no maximo 1 icone por frame (le disco) e solta os que ficaram longe
static void Picker_StreamIcons(void) {
    if(!pickIcons || pickShownCount <= 0) return;
    const int center = pickerSel - 1; // posicao na lista filtrada
    int lo = center - PICK_ICON_KEEP;
    int hi = center + PICK_ICON_KEEP;
    if(lo < 0) lo = 0;
    if(hi > pickShownCount - 1) hi = pickShownCount - 1;
    // solta so o que saiu da janela desde a ultima vez (custo proporcional a janela, nao a lista)
    for(int k = pickWinLo; k <= pickWinHi; k++) {
        if(k >= lo && k <= hi) continue;
        const int idx = pickShown[k];
        if(pickIcons[idx].id != 0) {
            UnloadTexture(pickIcons[idx]);
            pickIcons[idx] = (Texture2D) {0};
        }
        pickTried[idx] = 0;
    }
    pickWinLo = lo;
    pickWinHi = hi;
    // carrega no maximo 1 icone por frame, o primeiro pendente da janela
    for(int k = lo; k <= hi; k++) {
        const int idx = pickShown[k];
        if(!pickTried[idx]) {
            pickTried[idx] = 1;
            pickIcons[idx] = FreeplayIcon_Resolve(&pickerSongs[idx]);
            break;
        }
    }
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
        EdStatusMsg("NAO ABRIU ESSA MUSICA");
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
        EdStatusMsg("SALVO!");
    } else {
        EdStatusMsg("ERRO AO SALVAR");
    }
}

// importa json solto -> vira pasta de mod -> abre (1 = ok)
char ImportJSON(const char* jsonPath, const char* modName) {
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
char Editor_SaveAs(const char* modName) {
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
        DrawTextEx(mainFont, dropQueryLen > 0 ? "(no results)" : "(vazio)", (Vector2) {r.x + 8, listY + 8}, 20, 1, YELLOW);
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
    EdStatusMsg("NEW SONG - SALVAR COMO");
}

/* Calcula a linha i; false se ela esta fora da tela. */
static bool Picker_RowGeom(int i, PickRow* r) {
    const int fontSize = (i == 0) ? 40 : 56;
    const float y = 720 / 2 + ((float)i - pickerOffset) * (10 + fontSize);
    if(y < -60 || y > 780)
        return false;
    if(i == 0) {
        strncpy(r->name, "open file directory...", sizeof(r->name) - 1);
        r->name[sizeof(r->name) - 1] = 0;
    } else {
        const SongEntry* se = &pickerSongs[pickShown[i - 1]];
        strncpy(r->name, se->name, sizeof(r->name) - 1);
        r->name[sizeof(r->name) - 1] = 0;
        if(strstr(se->dir, "mods") != NULL)
            strncat(r->name, "  [mod]", sizeof(r->name) - strlen(r->name) - 1);
    }
    const float d = fabsf((float)i - pickerOffset);
    const float iconX = 150.0f + d * -30.0f;
    r->size = (float)fontSize;
    r->pos = (Vector2) {(float)(5 + 150 + d * -30) + (i == 0 ? 0.0f : PICK_ICON_SIZE + 20.0f), y + 5};
    r->fill = WHITE;
    r->out = BLACK;
    r->fill.a = (pickerSel == i) ? 255 : 150;
    r->out.a = (pickerSel == i) ? 255 : 100;
    if(i == 0) r->fill = (pickerSel == 0) ? GREEN : (Color) {150, 255, 150, 150};
    r->hasBox = i > 0 && pickerSel == i;   // so a selecionada mostra o icone; as outras ficam so com o nome
    if(r->hasBox) {
        r->a = (pickerSel == i) ? 255 : 150;
        r->fr = (Rectangle) {iconX - 6.0f, y - 4.0f, PICK_ICON_SIZE + 12.0f, PICK_ICON_SIZE + 12.0f};
        r->box = (Rectangle) {iconX, y + 2.0f, PICK_ICON_SIZE, PICK_ICON_SIZE};
        r->icon = pickIcons ? pickIcons[pickShown[i - 1]] : (Texture2D) {0};
    }
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

    // Passada 1: quadradinhos de fundo e borda.
    for(int k = 0; k < n; ++k) {
        if(!rows[k].hasBox) continue;
        DrawRectangleRec(rows[k].fr, (Color) {20, 20, 30, (unsigned char)(rows[k].a * 200 / 255)});
        DrawRectangleLinesEx(rows[k].fr, 2.0f, (Color) {255, 255, 255, rows[k].a});
    }
    // Passada 2: icones do oponente (uma textura por musica).
    for(int k = 0; k < n; ++k) {
        if(!rows[k].hasBox || rows[k].icon.id == 0) continue;
        DrawTexturePro(rows[k].icon, Picker_SquareCrop(rows[k].icon), rows[k].box, (Vector2) {0, 0}, 0.0f,
            (Color) {255, 255, 255, rows[k].a});
    }
    // Passada 3: textos (titulo, "?", nomes, mensagens e dica), todos com a textura da fonte.
    // cartao escuro atras da lista + titulo com barra de destaque
    DrawRectangleRounded((Rectangle) {130, 40, 780, 650}, 0.04f, 8, (Color) {8, 10, 18, 170});
    DrawRectangleRounded((Rectangle) {150, 56, 6, 40}, 0.5f, 4, GREEN);
    DrawTextEx(mainFont, "song selector", (Vector2) {166, 60}, 48, 4, WHITE);
    for(int k = 0; k < n; ++k) {
        const PickRow* r = &rows[k];
        if(r->hasBox && r->icon.id == 0) {
            const Vector2 q = MeasureTextEx(mainFont, "?", 32, 2);
            DrawTextEx(mainFont, "?", (Vector2) {r->box.x + (r->box.width - q.x) * 0.5f, r->box.y + (r->box.height - q.y) * 0.5f},
                32, 2, (Color) {200, 200, 200, r->a});
        }
        DrawTextEx(mainFont, r->name, r->pos, r->size, 4, r->out);
        const Vector2 fillPos = {r->pos.x - 5, r->pos.y - 5};
        DrawTextEx(mainFont, r->name, fillPos, r->size, 4, r->fill);
    }
    if(pickShownCount == 0 && pickerCount > 0)
        DrawTextEx(mainFont, "nenhuma musica encontrada", (Vector2) {260, 400}, 32, 2, (Color) {255, 200, 120, 255});
    DrawRectangleRounded((Rectangle) {140, 684, 760, 28}, 0.4f, 6, (Color) {20, 24, 36, 220});
    DrawTextEx(mainFont, "ENTER open   R update   N new   arraste um .json pra ca   ESC return",
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

void Picker_Update(void) {
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

    // letras/Enter/Esc/Backspace pertencem ao campo enquanto ele esta focado
    const bool typing = SearchBox_Focused(&pickSearch);
    if(SearchBox_Update(&pickSearch)) Picker_ApplyFilter();

    const int rows = 1 + pickShownCount;
    if(IsKeyPressed(KEY_DOWN) || (!typing && IsKeyPressed(KEY_S)))
        Picker_Move(1, rows);
    if(IsKeyPressed(KEY_UP) || (!typing && IsKeyPressed(KEY_W)))
        Picker_Move(-1, rows);
    const float wheel = GetMouseWheelMove();
    if(wheel != 0.0f)
        Picker_Move(wheel > 0.0f ? -1 : 1, rows);
    if(!typing && IsKeyPressed(KEY_R))
        Picker_Refresh();
    if(!typing && IsKeyPressed(KEY_N))
        Picker_NewSong();
    Picker_StreamIcons();
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
            int fs = (i == 0) ? 40 : 56;
            float y = 720 / 2 + ((float)i - pickerOffset) * (10 + fs);
            if(m.x >= 60 && m.x <= 1100 && m.y >= y - 5 && m.y <= y + fs) {
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
