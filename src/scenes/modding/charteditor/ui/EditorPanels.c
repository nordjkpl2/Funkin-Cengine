/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: barra de menu, abas e paineis laterais (Song, Section, Note...). */
#include "scenes/api/FontLoader.h"
#include "scenes/gameplay/PlayState.h"
#include"data/song/SongList.h"
#include "core/ModPath.h"
#include "scenes/modding/charteditor/io/EditorDataAndIO.h"
#include "scenes/modding/charteditor/grid/EditorTimeline.h"
#include "scenes/modding/charteditor/audio/EditorAudio.h"
#include "scenes/modding/charteditor/ui/EditorWidgets.h"
#include "scenes/modding/charteditor/grid/EditorGrid.h"
#include "scenes/modding/charteditor/ui/EditorPanels.h"
#include "rendering/TextMeasure.h"
#include "scenes/modding/charteditor/lists/EditorCharactersAndLists.h"
#include "scenes/modding/charteditor/model/EditorStagePos.h"
#include "scenes/modding/charteditor/ui/EditorPicker.h"
#include "scenes/gameplay/events/PlayStateEventsInGameplay.h"
#include "scenes/modding/charteditor/EditorActions.h"
#include <math.h>

/* Abre a janela "Save As" vazia (menu File e aba Song). */
static void Edit_OpenSaveAs(void) {
    saveAsBox.buf[0] = 0;
    importPath[0] = 0;
    ceDlg = DLG_SAVEAS;
    focusBox = &saveAsBox;
}

/* Salva e abre o test play com o modo pedido (FULL = F5, RECEPTORS = F6). */
void Edit_StartTest(PlayStateTestMode mode) {
    Audio_StopPlay();
    Editor_Save();
    PlayState_SetSongDir(chart.dir);   // zera o modo; SetTestMode vem depois
    PlayState_SetTestMode(mode);
    PlayState_SetReturnEditor(1);
    PlayState_SetScene();
}

// ---------- menus suspensos ----------
#define MENU_ITEM_H 28.0f
#define MENU_TITLE_W 70.0f

/* Caixa (com sombra) de um menu com n itens, logo abaixo do titulo. */
static Rectangle Menu_Box(float x, float w, int n) {
    const Rectangle box = {x, MENU_H + 2, w, n * MENU_ITEM_H + 8};
    DrawRectangleRounded((Rectangle){box.x + 4, box.y + 5, box.width, box.height}, 0.06f, 6, (Color){0, 0, 0, 110});
    DrawRectangleRounded(box, 0.06f, 6, ED_C_POPUP);
    DrawRectangleRoundedLinesEx(box, 0.06f, 6, 1.0f, ED_C_EDGE);
    return box;
}

static Rectangle Menu_Item(Rectangle box, int i) {
    return (Rectangle){box.x + 4, box.y + 4 + i * MENU_ITEM_H, box.width - 8, MENU_ITEM_H};
}

/* Item do menu Sound/Play: com ok = 0 (arquivo ausente) o texto fica vermelho e o clique so avisa.
   lane >= 0: desenha a seta dessa lane (0..3) antes do texto; -1 = sem icone. */
static char PlayMenuItem(Rectangle r, const char* label, char ok, const char* missing, int lane) {
    const char hov = EdHover(r);
    if(hov) {
        DrawRectangleRounded(r, 0.25f, 6, ED_C_BTN_HOV);
        DrawRectangleRec((Rectangle){r.x, r.y + 5, 3, r.height - 10}, ED_C_ACCENT);
    }
    float tx = r.x + 10;
    if(lane >= 0) {
        EdArrows_DrawIcon(lane, r.x + 18, r.y + r.height / 2, 22.0f, ok ? WHITE : (Color){255, 255, 255, 90});
        tx = r.x + 34;
    }
    Ed_DrawText(mainFont, label, (Vector2){tx, r.y + 5}, 20, 1, !ok ? ED_C_BAD : hov ? ED_C_ACCENT : WHITE);
    if(!(hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))) return 0;
    if(!ok) EdStatusMsg(missing);
    return 1;
}

/* Sound > Play only X: deixa so essa faixa audivel e toca no editor (nao abre a gameplay). */
static void Edit_PlaySolo(int mix, char ok) {
    if(!ok) return;
    Audio_SetSolo(mix);
    if(!playing) Audio_Toggle();
}

/* Itens do menu aberto; retorna a caixa (para o "clicou fora fecha"). Item clicado fecha o menu. */
static Rectangle Menu_DrawOpen(float mx) {
    Rectangle box;
    switch(menuOpen) {
    case 1: // File
        box = Menu_Box(mx, 230, 6);
        if(EdMenuItemIcon(Menu_Item(box, 0), "Save  Ctrl+S", ED_ICON_SAVE)) { menuOpen = 0; Editor_Save(); }
        else if(EdMenuItemIcon(Menu_Item(box, 1), "Save As...", ED_ICON_SAVE)) { menuOpen = 0; Edit_OpenSaveAs(); }
        else if(EdMenuItemIcon(Menu_Item(box, 2), "Save Compressed", ED_ICON_SAVE)) { menuOpen = 0; EdChart_SaveCompressed(&chart); EdStatusMsg("SAVED COMPRESSED"); }
        else if(EdMenuItemIcon(Menu_Item(box, 3), "Save as binary", ED_ICON_SAVE)) { menuOpen = 0; EdStatusMsg(EdChart_SaveBinary(&chart) ? "SAVED BINARY .song" : "ERROR SAVING BINARY"); }
        else if(EdMenuItemIcon(Menu_Item(box, 4), "Save Events", ED_ICON_DOOR)) { menuOpen = 0; EdStatusMsg(EdEvents_Save(chart.dir) ? "EVENTS SAVED" : "ERROR SAVING EVENTS"); }
        else if(EdMenuItemIcon(Menu_Item(box, 5), "Back to list  ESC", ED_ICON_BACK)) { menuOpen = 0; Editor_RequestLeave(); }
        break;
    case 2: // Edit
        box = Menu_Box(mx, 230, 5);
        if(EdMenuItemIcon(Menu_Item(box, 0), "Undo  Ctrl+Z", ED_ICON_RESTART)) { menuOpen = 0; EdAct_Undo(); }
        else if(EdMenuItem(Menu_Item(box, 1), "Copy part  C")) { menuOpen = 0; EdAct_CopyPart(); }
        else if(EdMenuItem(Menu_Item(box, 2), "Paste at end  V")) { menuOpen = 0; EdAct_PastePart(); }
        else if(EdMenuItem(Menu_Item(box, 3), "New part  N")) { menuOpen = 0; EdAct_NewPart(); }
        else if(EdMenuItemIcon(Menu_Item(box, 4), "Delete part  X", ED_ICON_TRASH)) { menuOpen = 0; EdAct_DeletePart(); }
        break;
    case 3: // View
        box = Menu_Box(mx, 230, 2);
        if(EdMenuItemIcon(Menu_Item(box, 0), playFollow ? "Note player : ON" : "Note player : OFF", ED_ICON_PLAYHEAD)) { menuOpen = 0; playFollow = !playFollow; }
        else if(EdMenuItem(Menu_Item(box, 1), "Go to part start  HOME")) { menuOpen = 0; cursorMs = SecStartMs(secIdx); }
        break;
    case 4: // Sound: escuta so uma faixa dentro do editor. Vermelho = arquivo ausente.
        box = Menu_Box(mx, 270, 5);
        if(PlayMenuItem(Menu_Item(box, 0), "All sounds", 1, "", -1)) { menuOpen = 0; Audio_SetSolo(0); }
        else if(PlayMenuItem(Menu_Item(box, 1), "Play only inst", edPlayHasInst, "NO Inst.ogg (ou mp3/wav)", 0)) { menuOpen = 0; Edit_PlaySolo(1, edPlayHasInst); }
        else if(PlayMenuItem(Menu_Item(box, 2), "Play only vocals", edPlayHasVocals, "NO Voices (ou Voices-Player/Opponent)", 1)) { menuOpen = 0; Edit_PlaySolo(2, edPlayHasVocals); }
        else if(PlayMenuItem(Menu_Item(box, 3), "Play only bf vocals", edPlayHasBf, "NO Voices-Player.ogg", 2)) { menuOpen = 0; Edit_PlaySolo(3, edPlayHasBf); }
        else if(PlayMenuItem(Menu_Item(box, 4), "Play only opponent vocals", edPlayHasOpp, "NO Voices-Opponent.ogg", 3)) { menuOpen = 0; Edit_PlaySolo(4, edPlayHasOpp); }
        break;
    default: // Play: testes do chart (abrem a gameplay)
        box = Menu_Box(mx, 240, 2);
        if(PlayMenuItem(Menu_Item(box, 0), "Test Play  F5", 1, "", -1)) { menuOpen = 0; Edit_StartTest(PLAYSTATE_TEST_FULL); }
        else if(PlayMenuItem(Menu_Item(box, 1), "Only note receptor  F6", 1, "", -1)) { menuOpen = 0; Edit_StartTest(PLAYSTATE_TEST_RECEPTORS); }
        break;
    }
    return box;
}

void Edit_DrawMenuBar(void) {
    DrawRectangle(0, 0, 1280, (int)MENU_H, ED_C_BAR);
    DrawRectangle(0, (int)MENU_H, 1280, 1, ED_C_EDGE);
    DrawRectangleRounded((Rectangle){14, 7, 4, 14}, 0.5f, 4, ED_C_ACCENT);
    DrawTextEx(mainFont, "CHART EDITOR", (Vector2){26, 5}, 18, 1, ED_C_DIM);

    static const char* const names[5] = {"File", "Edit", "View", "Sound", "Play"};
    const char canClick = ceDlg == DLG_NONE && !EdHelpOpen();
    for(int i = 0; i < 5; i++) {
        const Rectangle r = {PANEL_X + i * MENU_TITLE_W, 0, MENU_TITLE_W - 2, MENU_H};
        const char hov = canClick && EdHover(r);
        const char open = (menuOpen == i + 1);
        if(open || hov)
            DrawRectangleRec(r, ED_C_BTN_HOV);
        if(open)
            DrawRectangleRec((Rectangle){r.x + 10, MENU_H - 3, r.width - 20, 3}, ED_C_ACCENT);
        Ed_DrawText(mainFont, names[i], (Vector2){r.x + 12, 5}, 20, 1, open ? ED_C_ACCENT : WHITE);
        if(hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            menuOpen = open ? 0 : (i + 1);
            activeDrop = 0;
            focusBox = NULL;
            focusNum = NULL;
        }
    }
    if(menuOpen == 0)
        return;
    const Rectangle box = Menu_DrawOpen(PANEL_X + (menuOpen - 1) * MENU_TITLE_W);
    // clicou fora (e fora da barra de titulos, que troca de menu sozinha): fecha
    if(menuOpen != 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !EdHover(box) && EdMouseWorld().y > MENU_H)
        menuOpen = 0;
}

// abas laterais (uma ativa por vez)
void Edit_DrawTabs(void) {
    const float tw = 102;
    static TextMeasure tabM[TAB_COUNT]; // nomes das abas sao fixos: medidos uma vez
    // com menu aberto, as abas ficam por baixo dele: o clique e do menu (senao troca de aba e fecha o menu)
    const char canClick = !EdUiBlocked();
    for(int t = 0; t < TAB_COUNT; t++) {
        const Rectangle r = {PANEL_X + t * (tw + 4), 34, tw, 26};
        const char active = (activeTab == t);
        const char hov = canClick && EdHover(r);
        Ed_DrawBox(r, active ? (Color){48, 30, 80, 255} : hov ? ED_C_BTN_HOV : ED_C_BTN, active ? ED_C_ACCENT : ED_C_EDGE);
        if(active) DrawRectangleRounded((Rectangle){r.x + 18, r.y + r.height - 4, r.width - 36, 3}, 1.0f, 4, ED_C_ACCENT);
        const Vector2 s = TextMeasure_Get(&tabM[t], mainFont, ChartTab_Name(t), 18, 1);
        Ed_DrawText(mainFont, ChartTab_Name(t), (Vector2){r.x + (r.width - s.x) / 2, r.y + 3}, 18, 1, (active || hov) ? WHITE : ED_C_DIM);
        if(hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
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
                          int dropId, char blocked) {
    Ed_DrawText(mainFont, label, (Vector2){x, y + 4}, 22, 1, WHITE);

    // a seta fica dentro da propria caixa (sem quadrado separado); a caixa de texto ocupa o resto
    const Rectangle field = {x + SONG_FIELD_X, y, boxW, 32};
    const Rectangle text = {field.x, y, boxW - 28, 32};
    TxtBox_Update(box, text, blocked);
    TxtBox_Draw(box, text, hint);

    const Rectangle button = {field.x + boxW - 28, y, 28, 32};
    if(EdDropBtn(button, activeDrop == dropId)) {
        if(activeDrop == dropId) {
            activeDrop = DROP_NONE;
        } else {
            activeDrop = dropId;
            dropBox = box;
            dropCount = Lists_Get(dropId, &dropNames);   // scan so agora, na 1a abertura
            dropX = field.x;
            dropY = field.y + field.height + 4;
            dropW = boxW;
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
    static TextMeasure fieldM[5]; // cada caixa tem seu cache: so remede quando o texto muda
    float width = 140;
    for(int i = 0; i < 5; i++) {
        const float w = TextMeasure_Get(&fieldM[i], mainFont, texts[i], 20, 1).x + 24;
        if(w > width) width = w;
    }
    return width > 240 ? 240 : width;
}

static void Edit_DrawPanel_Song(float x, float y) {
    const char blocked = EdUiBlocked();
    const float boxW = SongFieldWidth();

    Ed_DrawHeader(x, y, "SONG", ED_C_ACCENT);
    y += 34;
    DrawDropField(x, y, boxW, "Boyfriend:", &p1Box, "bf", DROP_BOYFRIEND, blocked);
    y += 38;
    DrawDropField(x, y, boxW, "Opponent:", &p2Box, "dad", DROP_OPPONENT, blocked);
    y += 38;
    DrawDropField(x, y, boxW, "Girlfriend:", &gfBox, "gf", DROP_GIRLFRIEND, blocked);
    y += 38;
    DrawDropField(x, y, boxW, "Stage:", &stageBox, "stage", DROP_STAGE, blocked);
    y += 38;
    DrawDropField(x, y, boxW, "Note Skin:", &skinBox, "default", DROP_SKIN, blocked);
    y += 42;
    y += StagePos_Draw(x, y, stageBox.buf, chart.dir, blocked) + 8;

    Ed_DrawText(mainFont, "bpm:", (Vector2){x, y + 4}, 22, 1, WHITE);
    {
        Rectangle r = {x + SONG_FIELD_X, y, 80, 32};
        if(focusNum != &nbTopBpm) NumBox_Refresh(&nbTopBpm, chart.bpm, 0);
        NumBox_Update(&nbTopBpm, r, blocked);
        NumBox_Draw(&nbTopBpm, r);
    }
    Ed_DrawText(mainFont, "speed:", (Vector2){x + 230, y + 4}, 22, 1, WHITE);
    {
        Rectangle r = {x + 310, y, 70, 32};
        if(focusNum != &nbSpeed) NumBox_Refresh(&nbSpeed, chart.speed, 0);
        NumBox_Update(&nbSpeed, r, blocked);
        NumBox_Draw(&nbSpeed, r);
    }
    y += 44;

    if(EdButton((Rectangle){x, y, 190, 40}, "SAVE", 22)) Editor_Save();
    if(EdButton((Rectangle){x + 200, y, 190, 40}, "SAVE AS", 20)) Edit_OpenSaveAs();
}

static void Edit_DrawPanel_Section(float x, float y) {
    const char blocked = EdUiBlocked();
    char buf[96];
    snprintf(buf, sizeof(buf), "SECTION %d/%d", secIdx + 1, chart.nsec);
    Ed_DrawHeader(x, y, buf, ED_C_ACCENT);
    y += 32;
    Ed_DrawText(mainFont, "turn (Q):", (Vector2){x, y + 4}, 22, 1, WHITE);
    if(EdButton((Rectangle){x + 150, y, 180, 32}, chart.secs[secIdx].mustHit ? "boyfriend" : "opponent", 20))
        EdAct_ToggleTurn();
    y += 42;
    // steps = tamanho do trecho em passos (16 = padrao, 4 tempos). Era "size:".
    Ed_DrawText(mainFont, "steps:", (Vector2){x, y + 4}, 22, 1, WHITE);
    {
        Rectangle r = {x + 150, y, 90, 32};
        if(focusNum != &nbLen) NumBox_Refresh(&nbLen, (float)chart.secs[secIdx].lenSteps, 1);
        NumBox_Update(&nbLen, r, blocked);
        NumBox_Draw(&nbLen, r);
    }
    Ed_DrawText(mainFont, "16 = 4 beats", (Vector2){x + 250, y + 8}, 18, 1, ED_C_HINT);
    y += 42;
    Ed_DrawText(mainFont, "bpm:", (Vector2){x, y + 4}, 22, 1, WHITE);
    {
        Rectangle r = {x + 150, y, 90, 32};
        if(focusNum != &nbSecBpm) NumBox_Refresh(&nbSecBpm, chart.secs[secIdx].bpm, 0);
        NumBox_Update(&nbSecBpm, r, blocked);
        NumBox_Draw(&nbSecBpm, r);
    }
    y += 46;
    if(EdButton((Rectangle){x, y, 120, 36}, "+ part", 20)) EdAct_NewPart();
    if(EdButton((Rectangle){x + 130, y, 120, 36}, "X part", 20)) EdAct_DeletePart();
    y += 44;
    if(EdButton((Rectangle){x, y, 120, 36}, "copy", 20)) EdAct_CopyPart();
    if(EdButton((Rectangle){x + 130, y, 120, 36}, "paste", 20)) EdAct_PastePart();
    y += 44;
}

// Data: resumo do chart e do cursor (antes eram 2 abas com a contagem de notas repetida)
static void Edit_DrawPanel_Data(float x, float y) {
    char buf[160];
    char tbuf[32];
    Ed_DrawHeader(x, y, "DATA", ED_C_ACCENT);
    y += 32;
    EdTimeFmt(tbuf, sizeof(tbuf), cursorMs);
    snprintf(buf, sizeof(buf), "cursor: %s", tbuf);
    Ed_DrawText(mainFont, buf, (Vector2){x, y}, 20, 1, WHITE);
    y += 30;
    snprintf(buf, sizeof(buf), "in part: %d notes", chart.secs[secIdx].count);
    Ed_DrawText(mainFont, buf, (Vector2){x, y}, 20, 1, WHITE);
    y += 30;
    int total = 0;
    for(int i = 0; i < chart.nsec; i++) total += chart.secs[i].count;
    snprintf(buf, sizeof(buf), "total: %d notes in %d parts", total, chart.nsec);
    Ed_DrawText(mainFont, buf, (Vector2){x, y}, 20, 1, WHITE);
    y += 30;
    EdTimeFmt(tbuf, sizeof(tbuf), SongEndMs());
    snprintf(buf, sizeof(buf), "length: %s", tbuf);
    Ed_DrawText(mainFont, buf, (Vector2){x, y}, 20, 1, WHITE);
    y += 30;
    snprintf(buf, sizeof(buf), "dir: %s", chart.dir[0] ? chart.dir : "(unsaved)");
    Ed_DrawText(mainFont, buf, (Vector2){x, y}, 18, 1, ED_C_DIM);
    y += 28;
    snprintf(buf, sizeof(buf), "bpm %.1f  speed %.2f%s", (double)chart.bpm, (double)chart.speed, chart.dirty ? " *" : "");
    Ed_DrawText(mainFont, buf, (Vector2){x, y}, 18, 1, ED_C_DIM);
    y += 36;
    Ed_DrawText(mainFont, "click adds  right-click deletes", (Vector2){x, y}, 18, 1, ED_C_HINT);
    y += 24;
    Ed_DrawText(mainFont, "SHIFT/CTRL+wheel = long note", (Vector2){x, y}, 18, 1, ED_C_HINT);
}

static void Edit_DrawPanel_Charting(float x, float y) {
    Ed_DrawHeader(x, y, "CHARTING", ED_C_ACCENT);
    y += 34;
    if(EdButton((Rectangle){x, y, 190, 40}, playing ? "PAUSE" : "LISTEN", 22)) Audio_Toggle();
    y += 50;
    // arquivos de audio achados (flags ja existem, custo zero)
    Ed_DrawText(mainFont, edHasAudio ? "inst.ogg: yes" : "inst.ogg: no", (Vector2){x, y}, 18, 1,
        edHasAudio ? ED_C_OK : ED_C_BAD);
    y += 24;
    const char* vocTxt = edHasSplit ? "vocals.ogg: split" : (edHasVoices ? "vocals.ogg: yes" : "vocals.ogg: no");
    Ed_DrawText(mainFont, vocTxt, (Vector2){x, y}, 18, 1, (edHasSplit || edHasVoices) ? ED_C_OK : ED_C_BAD);
    y += 30;
    Ed_DrawText(mainFont, "space = play/stop", (Vector2){x, y}, 18, 1, ED_C_HINT);
    y += 24;
    Ed_DrawText(mainFont, "drag the song bar = seek", (Vector2){x, y}, 18, 1, ED_C_HINT);
    y += 24;
    Ed_DrawText(mainFont, "W/S or wheel = scroll", (Vector2){x, y}, 18, 1, ED_C_HINT);
}

// ---------- eventos: picker com busca + edicao do selecionado ----------
static char evPickOpen;
static char evPickShaders;  // picker abre so com os shaders de tela
static TxtBox evSearchBox;
static TxtBox evImgBox;     // image name do Show Image selecionado
static TxtBox evNumBox[3];  // valores 1..3 do evento selecionado (campo de texto numerico)
static int evSelected = -1; // indice em edEvents (-1 = nenhum)

// busca sem diferenciar maiusculas (needle vazio casa tudo)
static char EvContainsCI(const char* hay, const char* needle) {
    const size_t n = strlen(needle);
    if(n == 0) return 1;
    for(size_t i = 0; hay[i] != 0; i++) {
        size_t k = 0;
        while(k < n && hay[i + k] != 0) {
            char a = hay[i + k], b = needle[k];
            if(a >= 'A' && a <= 'Z') a = (char)(a + 32);
            if(b >= 'A' && b <= 'Z') b = (char)(b + 32);
            if(a != b) break;
            k++;
        }
        if(k == n) return 1;
    }
    return 0;
}

static void EvClosePicker(void) {
    evPickOpen = 0;
    evPickShaders = 0;
    if(focusBox == &evSearchBox) focusBox = NULL;
}

static void EvAddFromType(const PlayStateEventType* t) {
    char v1[16], v2[16], v3[16];
    snprintf(v1, sizeof(v1), "%d", t->defaultDurMs);
    snprintf(v2, sizeof(v2), "%d", t->defaultAlphaPct);
    snprintf(v3, sizeof(v3), "%d", t->defaultScalePct);
    evSelected = EdEvent_Add((float)cursorMs, t->name, v1, v2, v3, "");
    chart.dirty = 1;
    EdStatusMsg("EVENT ADDED (CTRL+S saves events.json)");
}

// seletor: lista com rolagem (linhas inteiras, igual ao menu de faixas)
#define EV_PICK_ROW_H 52
#define EV_PICK_ROWS 6
#define EV_PICK_LIST_H (EV_PICK_ROW_H * EV_PICK_ROWS)
#define EV_PICK_W 470
#define EV_ICON_S 30

// pseudo icone do evento: quadrado arredondado com a sigla (iniciais) e cor fixa pelo nome
static void EvDrawIcon(Rectangle r, const char* name) {
    unsigned h = 2166136261u;
    for(const char* p = name; *p; p++) h = (h ^ (unsigned char)*p) * 16777619u;
    DrawRectangleRounded(r, 0.3f, 6, ColorFromHSV((float)(h % 360u), 0.5f, 0.9f));

    char ab[3] = {0, 0, 0};
    int k = 0;
    char word = 1;
    for(const char* p = name; *p && k < 2; p++) {
        const char c = *p;
        const char isLetter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        if(isLetter && word) ab[k++] = (char)((c >= 'a' && c <= 'z') ? c - 32 : c);
        word = !isLetter;
    }
    const float fs = 15.0f;
    const Vector2 s = MeasureTextEx(mainFont, ab, fs, 1);
    DrawTextEx(mainFont, ab, (Vector2){r.x + (r.width - s.x) * 0.5f, r.y + (r.height - s.y) * 0.5f}, fs, 1, (Color){20, 20, 30, 255});
}
static int evPickTop;                       // primeira linha visivel
static char evPickDrag;                     // arrastando a barra
static char evPickLastQ[sizeof(evSearchBox.buf)];  // busca anterior (muda -> volta ao topo)
static Rectangle evPickBox;                 // area da lista (ultimo desenho), p/ roda do mouse
#define EV_LIST_ROWS 5                      // linhas visiveis da lista de eventos da musica
#define EV_LIST_ROW_H 32
static int evListTop;                       // primeira linha visivel da lista da musica
static Rectangle evListBox;                 // area da lista da musica (ultimo desenho)

static char EvPickMatch(const PlayStateEventType* t) {
    if(evPickShaders && !t->usesShader) return 0;
    return EvContainsCI(t->name, evSearchBox.buf) || EvContainsCI(t->description, evSearchBox.buf);
}

static int EvPickCount(void) {
    int n = 0;
    for(int i = 0; i < playStateEventTypeCount; i++)
        if(EvPickMatch(&playStateEventTypes[i])) n++;
    return n;
}

static int EvPickMaxTop(int n) {
    return n > EV_PICK_ROWS ? n - EV_PICK_ROWS : 0;
}

char Edit_EventPickerWheel(float wheel) {
    if(activeTab != TAB_EVENTS || !evPickOpen || wheel == 0) return 0;
    if(!CheckCollisionPointRec(EdMouseWorld(), evPickBox)) return 0;
    evPickTop += wheel > 0 ? -1 : 1;
    int maxTop = EvPickMaxTop(EvPickCount());
    if(evPickTop < 0) evPickTop = 0;
    if(evPickTop > maxTop) evPickTop = maxTop;
    return 1;
}

/* Roda do mouse sobre a lista de song events: rola as linhas e retorna 1. */
char Edit_EventListWheel(float wheel) {
    if(activeTab != TAB_EVENTS || evPickOpen || wheel == 0) return 0;
    if(!CheckCollisionPointRec(EdMouseWorld(), evListBox)) return 0;
    evListTop += wheel > 0 ? -1 : 1;
    int maxTop = edEventCount > EV_LIST_ROWS ? edEventCount - EV_LIST_ROWS : 0;
    if(evListTop < 0) evListTop = 0;
    if(evListTop > maxTop) evListTop = maxTop;
    return 1;
}

// lista filtrada pela busca; clicar adiciona no cursor
static void Edit_DrawEventPicker(float x, float y) {
    const char blocked = EdUiBlocked();
    Ed_DrawText(mainFont, evPickShaders ? "choose shader:" : "search event:", (Vector2){x, y + 4}, 20, 1, WHITE);
    y += 28;
    Rectangle sr = {x, y, 250, 32};
    TxtBox_Update(&evSearchBox, sr, blocked);
    TxtBox_Draw(&evSearchBox, sr, evPickShaders ? "tv / glitch / off" : "fade in / out");
    if(EdButton((Rectangle){x + 260, y, 90, 32}, "fechar", 18)) EvClosePicker();
    y += 44;

    // busca mudou: volta ao topo da lista
    if(strcmp(evPickLastQ, evSearchBox.buf) != 0) {
        snprintf(evPickLastQ, sizeof(evPickLastQ), "%s", evSearchBox.buf);
        evPickTop = 0;
    }
    const int n = EvPickCount();
    const int maxTop = EvPickMaxTop(n);
    if(evPickTop > maxTop) evPickTop = maxTop;
    if(evPickTop < 0) evPickTop = 0;

    evPickBox = (Rectangle){x, y, EV_PICK_W, EV_PICK_LIST_H};
    DrawRectangleRounded(evPickBox, 0.03f, 6, ED_C_POPUP);
    DrawRectangleRoundedLinesEx(evPickBox, 0.03f, 6, 1.5f, ED_C_EDGE);

    // barra de rolagem: mesmo estilo do menu de faixas (trilho cinza, polegar verde)
    const float barX = x + EV_PICK_W - 8;
    const Rectangle barHit = {x + EV_PICK_W - 14, y, 16, EV_PICK_LIST_H};
    if(maxTop > 0) {
        const Vector2 m = EdMouseWorld();
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(m, barHit)) evPickDrag = 1;
        if(!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) evPickDrag = 0;
        const float thumbH = fmaxf(24.0f, EV_PICK_LIST_H * (float)EV_PICK_ROWS / (float)n);
        if(evPickDrag) {
            const float span = EV_PICK_LIST_H - thumbH;
            float t = span > 0.0f ? (m.y - y - thumbH * 0.5f) / span : 0.0f;
            t = fminf(fmaxf(t, 0.0f), 1.0f);
            evPickTop = (int)lroundf(t * (float)maxTop);
        }
        const float thumbY = y + (EV_PICK_LIST_H - thumbH) * (float)evPickTop / (float)maxTop;
        DrawRectangleRounded((Rectangle){barX, y, 6, EV_PICK_LIST_H}, 0.5f, 4, (Color){50, 44, 80, 255});
        DrawRectangleRounded((Rectangle){barX, thumbY, 6, thumbH}, 0.5f, 4, ED_C_ACCENT);
    } else {
        evPickDrag = 0;
    }

    if(n == 0) {
        Ed_DrawText(mainFont, "no event found", (Vector2){x + 6, y + 8}, 18, 1, ED_C_HINT);
        return;
    }

    // so desenha as linhas visiveis (linha inteira, sem corte)
    // mesmo visual do seletor de preset (Options > Note Colors): linha com hover, clique adiciona
    const Vector2 mouse = EdMouseWorld();
    const bool canClick = !EdUiBlocked();
    int idx = 0, row = 0;
    for(int i = 0; i < playStateEventTypeCount && row < EV_PICK_ROWS; i++) {
        const PlayStateEventType* t = &playStateEventTypes[i];
        if(!EvPickMatch(t)) continue;
        if(idx++ < evPickTop) continue;
        const Rectangle rr = {x + 4, y + (float)(row * EV_PICK_ROW_H) + 3, EV_PICK_W - 16, EV_PICK_ROW_H - 6};
        row++;
        const bool hov = canClick && CheckCollisionPointRec(mouse, rr);
        Ed_DrawBox(rr, hov ? ED_C_BTN_HOV : ED_C_BTN, hov ? ED_C_ACCENT : (Color){0, 0, 0, 0});
        EvDrawIcon((Rectangle){rr.x + 8, rr.y + (rr.height - EV_ICON_S) * 0.5f, EV_ICON_S, EV_ICON_S}, t->name);

        DrawTextEx(mainFont, t->name, (Vector2){rr.x + 50, rr.y + 5}, 20, 1, hov ? ED_C_ACCENT : WHITE);
        // descricao: encolhe a fonte se nao cabe na largura (nao vaza pra fora da caixa)
        const float maxW = rr.width - 60.0f;
        float fs = 15.0f;
        const float tw = MeasureTextEx(mainFont, t->description, fs, 1).x;
        if(tw > maxW) fs = fmaxf(10.0f, fs * maxW / tw);
        DrawTextEx(mainFont, t->description, (Vector2){rr.x + 50, rr.y + 28}, fs, 1, ED_C_HINT);

        if(hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            EvAddFromType(t);
            EvClosePicker();
            return;
        }
    }
}

// tipo do evento pelo nome (NULL = tipo desconhecido)
static const PlayStateEventType* EvTypeOf(const char* name) {
    for(int i = 0; i < playStateEventTypeCount; i++)
        if(strcmp(playStateEventTypes[i].name, name) == 0) return &playStateEventTypes[i];
    return NULL;
}

// valor inteiro do evento (v1/v2/v3 sao texto no events.json)
static void EvStepInt(char* field, size_t n, int delta, int lo, int hi) {
    int v = atoi(field) + delta;
    if(v < lo) v = lo;
    if(v > hi) v = hi;
    char nv[16];
    snprintf(nv, sizeof(nv), "%d", v);
    if(strcmp(nv, field) != 0) {
        snprintf(field, n, "%s", nv);
        chart.dirty = 1;
    }
}

/* Campo numerico editavel: digitando, so digitos (e '-' no inicio) vao para o evento; fora do campo, limita a faixa. */
static void EvNumField(int slot, Rectangle r, char* field, size_t n, int lo, int hi) {
    TxtBox* b = &evNumBox[slot];
    TxtBox_Update(b, r, EdUiBlocked());
    TxtBox_Draw(b, r, "");
    if(focusBox == b) {
        char dig[16];
        size_t k = 0;
        for(const char* p = b->buf; *p != 0 && k < sizeof(dig) - 1; p++)
            if((*p >= '0' && *p <= '9') || (*p == '-' && k == 0)) dig[k++] = *p;
        dig[k] = 0;
        if(strcmp(dig, b->buf) != 0) snprintf(b->buf, sizeof(b->buf), "%s", dig);
        if(k > 0 && strcmp(dig, field) != 0) {
            snprintf(field, n, "%s", dig);
            chart.dirty = 1;
        }
    } else {
        EvStepInt(field, n, 0, lo, hi);
        snprintf(b->buf, sizeof(b->buf), "%s", field);
    }
}

static void Edit_DrawPanel_Events(float x, float y) {
    char tb[96];
    Ed_DrawHeader(x, y, "EVENTS", ED_C_ACCENT);
    y += 34;
    EdTimeFmt(tb, sizeof(tb), cursorMs);
    char curBuf[96];
    snprintf(curBuf, sizeof(curBuf), "cursor: %s", tb);
    Ed_DrawText(mainFont, curBuf, (Vector2){x, y + 4}, 20, 1, WHITE);
    y += 34;

    // lista trocada (outra musica / novo chart): o indice guardado nao vale mais
    static int evSelGen = -1;
    if(evSelGen != edEventsGen) {
        evSelGen = edEventsGen;
        evSelected = -1;
    }
    if(evSelected >= edEventCount) evSelected = -1;

    // grupos recolhiveis: ">" fecha/abre; o botao do grupo aberto mostra "v"
    if(EdButton((Rectangle){x, y, 350, 36}, (evPickOpen && !evPickShaders) ? "v Add event" : "> Add event", 20)) {
        if(evPickOpen && !evPickShaders) EvClosePicker();
        else {
            evPickOpen = 1;
            evPickShaders = 0;
            evSearchBox.buf[0] = 0;
            evPickTop = 0;
            focusBox = &evSearchBox;
        }
    }
    if(EdButton((Rectangle){x + 360, y, 220, 36}, (evPickOpen && evPickShaders) ? "v Add shader" : "> Add shader", 20)) {
        if(evPickOpen && evPickShaders) EvClosePicker();
        else {
            evPickOpen = 1;
            evPickShaders = 1;
            evSearchBox.buf[0] = 0;
            evPickTop = 0;
            focusBox = &evSearchBox;
        }
    }
    y += 50;
    if(evPickOpen) {
        Edit_DrawEventPicker(x, y);
        return;
    }

    Ed_DrawText(mainFont, "song events:", (Vector2){x, y}, 20, 1, ED_C_DIM);
    y += 28;
    // lista com rolagem (roda do mouse): so EV_LIST_ROWS linhas por vez, com barra lateral
    const float listTop = y;
    const int maxTop = edEventCount > EV_LIST_ROWS ? edEventCount - EV_LIST_ROWS : 0;
    if(evListTop > maxTop) evListTop = maxTop;
    if(evListTop < 0) evListTop = 0;
    const int rowsDrawn = (edEventCount - evListTop < EV_LIST_ROWS) ? edEventCount - evListTop : EV_LIST_ROWS;
    evListBox = (Rectangle){x - 6, listTop - 4, 392, EV_LIST_ROWS * EV_LIST_ROW_H + 8};
    DrawRectangleRounded(evListBox, 0.04f, 6, (Color){12, 10, 26, 170});
    for(int k = 0; k < rowsDrawn; k++) {
        const int i = evListTop + k;
        const float ry = listTop + k * EV_LIST_ROW_H;
        char row[120];
        snprintf(row, sizeof(row), "%.0fms  %s", edEvents[i].time, edEvents[i].name);
        if(evSelected == i)  // linha selecionada: fundo verde suave
            DrawRectangleRounded((Rectangle){x - 2, ry - 2, 352, 32}, 0.25f, 6, (Color){255, 92, 176, 70});
        EvDrawIcon((Rectangle){x, ry, 28, 28}, edEvents[i].name);
        if(EdButton((Rectangle){x + 34, ry, 266, 28}, row, 18)) evSelected = (evSelected == i) ? -1 : i;
        if(EdButton((Rectangle){x + 310, ry, 36, 28}, "X", 18)) {
            EdEvent_RemoveAt(i);
            if(evSelected == i) evSelected = -1;
            else if(evSelected > i) evSelected--;
            chart.dirty = 1;
            return;
        }
    }
    if(edEventCount > EV_LIST_ROWS) {  // barra de rolagem: trilho + alca proporcional
        const float trackY = listTop, trackH = EV_LIST_ROWS * EV_LIST_ROW_H - 4;
        const float thumbH = trackH * EV_LIST_ROWS / edEventCount;
        const float thumbY = trackY + (trackH - thumbH) * evListTop / maxTop;
        DrawRectangleRounded((Rectangle){x + 370, trackY, 6, trackH}, 0.5f, 4, (Color){50, 44, 80, 255});
        DrawRectangleRounded((Rectangle){x + 370, thumbY, 6, thumbH}, 0.5f, 4, ED_C_ACCENT);
    }
    y = listTop + rowsDrawn * EV_LIST_ROW_H;
    if(edEventCount == 0) Ed_DrawText(mainFont, "(none yet)", (Vector2){x, y + 4}, 18, 1, ED_C_HINT);
    y += 40;

    if(evSelected >= 0) {
        EdEvent* e = &edEvents[evSelected];
        snprintf(tb, sizeof(tb), "editing: %s", e->name);
        Ed_DrawText(mainFont, tb, (Vector2){x, y}, 20, 1, ED_C_ACCENT);
        y += 34;

        // Show Image: x/y aceitam negativo (a partir do centro) e passos de 10 px
        const PlayStateEventType* t = EvTypeOf(e->name);
        if(t != NULL && t->label1 == NULL) {
            // tipo sem valores (ex.: Hide Image): so informa
            Ed_DrawText(mainFont, "(no values)", (Vector2){x, y + 4}, 18, 1, ED_C_HINT);
            y += 40;
            snprintf(tb, sizeof(tb), "%d event(s) in total", edEventCount);
            Ed_DrawText(mainFont, tb, (Vector2){x, y}, 18, 1, ED_C_DIM);
            return;
        }
        const char img = (t != NULL && t->usesImage);
        const char chr = (t != NULL && t->usesChar);  // troca de personagem: alvo 0..2 + nome (s1)
        const char txt = (t != NULL && t->usesText);  // so texto livre em s1 (ex.: criador da musica)
        const char* lab1 = (t != NULL) ? t->label1 : "value 1:";
        const char* lab2 = (t != NULL) ? t->label2 : "value 2:";
        // shader e parallax: intensidade e velocidade 0..100 (passos de 10)
        const char fx = (t != NULL && (t->usesShader || strcmp(t->name, "Camera Parallax") == 0));
        const char cam = (t != NULL && strcmp(t->name, "Camera Move") == 0);  // zoom % + x/y com sinal
        const int lo1 = img ? -4000 : cam ? 50 : 0, hi1 = img ? 4000 : chr ? 2 : cam ? 200 : (fx ? 100 : 60000);
        const int st1 = (img || fx || cam) ? 10 : chr ? 1 : 50;
        const int lo2 = (img || cam) ? -4000 : 0, hi2 = (img || cam) ? 4000 : 100, st2 = 10;

        Ed_DrawText(mainFont, lab1, (Vector2){x, y + 4}, 20, 1, WHITE);
        if(EdButton((Rectangle){x + 110, y, 36, 32}, "-", 22)) EvStepInt(e->v1, sizeof(e->v1), -st1, lo1, hi1);
        EvNumField(0, (Rectangle){x + 150, y, 86, 32}, e->v1, sizeof(e->v1), lo1, hi1);
        if(EdButton((Rectangle){x + 250, y, 36, 32}, "+", 22)) EvStepInt(e->v1, sizeof(e->v1), st1, lo1, hi1);
        // Music Credit: tempo em ms; "++" soma 100
        const char msCredit = (t != NULL && strcmp(t->name, "Music Credit") == 0);
        if(msCredit && EdButton((Rectangle){x + 294, y, 46, 32}, "++", 20)) EvStepInt(e->v1, sizeof(e->v1), 100, lo1, hi1);
        y += 40;

        if(!chr && !txt) {
            Ed_DrawText(mainFont, lab2, (Vector2){x, y + 4}, 20, 1, WHITE);
            if(EdButton((Rectangle){x + 110, y, 36, 32}, "-", 22)) EvStepInt(e->v2, sizeof(e->v2), -st2, lo2, hi2);
            EvNumField(1, (Rectangle){x + 150, y, 86, 32}, e->v2, sizeof(e->v2), lo2, hi2);
            if(EdButton((Rectangle){x + 250, y, 36, 32}, "+", 22)) EvStepInt(e->v2, sizeof(e->v2), st2, lo2, hi2);
            y += 40;
        }

        if(t != NULL && t->label3 != NULL) {
            Ed_DrawText(mainFont, t->label3, (Vector2){x, y + 4}, 20, 1, WHITE);
            const int lo3 = cam ? -4000 : 10;  // Camera Move: y aceita negativo
            if(EdButton((Rectangle){x + 110, y, 36, 32}, "-", 22)) EvStepInt(e->v3, sizeof(e->v3), -10, lo3, 1000);
            EvNumField(2, (Rectangle){x + 150, y, 86, 32}, e->v3, sizeof(e->v3), lo3, 1000);
            if(EdButton((Rectangle){x + 250, y, 36, 32}, "+", 22)) EvStepInt(e->v3, sizeof(e->v3), 10, lo3, 1000);
            y += 40;
        }

        if(img || chr || txt) {
            // caixa de texto com o image name (ou do personagem): sincroniza com o evento selecionado
            static int evImgSel = -2;
            static int evImgGen = -1;
            if(evImgSel != evSelected || evImgGen != edEventsGen) {
                evImgSel = evSelected;
                evImgGen = edEventsGen;
                snprintf(evImgBox.buf, sizeof(evImgBox.buf), "%s", e->s1);
            }
            Ed_DrawText(mainFont, chr ? "character (folder/xml name):" : txt ? "creator (text):" : "image (images/<name>.png):", (Vector2){x, y}, 18, 1, WHITE);
            y += 26;
            Rectangle ir = {x, y, 250, 32};
            TxtBox_Update(&evImgBox, ir, EdUiBlocked());
            TxtBox_Draw(&evImgBox, ir, chr ? "e.g.: bf-car, gf, dad" : txt ? "creator name" : "image name");
            if(strcmp(evImgBox.buf, e->s1) != 0) {
                snprintf(e->s1, sizeof(e->s1), "%s", evImgBox.buf);
                snprintf(evImgBox.buf, sizeof(evImgBox.buf), "%s", e->s1); /* ja truncado */
                chart.dirty = 1;
            }
            y += 44;
        }
        y += 4;
    }

    snprintf(tb, sizeof(tb), "%d event(s) in total", edEventCount);
    Ed_DrawText(mainFont, tb, (Vector2){x, y}, 18, 1, ED_C_DIM);
}

// preview do shader do evento selecionado (so na aba Events); fora disso, sem efeito
void Edit_ShaderPreview(void) {
    // o picker de musicas nao usa shader de tela
    if(ceMode == CE_PICKER) {
        ShaderFx_Reset();
        return;
    }
    if(activeTab == TAB_EVENTS && evSelected >= 0 && evSelected < edEventCount) {
        const EdEvent* e = &edEvents[evSelected];
        const PlayStateEventType* t = EvTypeOf(e->name);
        if(t != NULL && t->usesShader) {
            PlayStateEvents_ApplyShader(e->name, atof(e->v1), atof(e->v2));
            return;
        }
    }
    ShaderFx_Reset();
}

/* Nome da musica (+ "*" se mudou) cortado com "..." pra caber em maxW.
   So remede quando o texto muda (antes media varias vezes por frame com nome longo). */
static const char* Panel_Title(float maxW) {
    static char key[sizeof(chart.name) + 2] = {1};   // valor impossivel: forca o 1o calculo
    static char out[sizeof(key)];
    char want[sizeof(key)];
    snprintf(want, sizeof(want), "%s%s", chart.name, chart.dirty ? "*" : "");
    if(strcmp(want, key) == 0)
        return out;
    memcpy(key, want, sizeof(key));
    memcpy(out, want, sizeof(out));
    for(size_t L = strlen(out); L > 4 && MeasureTextEx(mainFont, out, 24, 2).x > maxW; L--) {
        out[L - 4] = '.';
        out[L - 3] = '.';
        out[L - 2] = '.';
        out[L - 1] = 0;
    }
    return out;
}

void Edit_DrawPanel(void) {
    const float x = PANEL_X;
    const float y = 100;
    SecFollowCursor();
    // sem bloco de fundo (pedido do dono): painel direto no bg
    DrawRectangleRec((Rectangle){PANEL_X - 9, 62, 1, 620}, ED_C_EDGE);
    Ed_DrawText(mainFont, Panel_Title(280), (Vector2){x, 66}, 24, 2, WHITE);
    // OPEN ao lado da musica: volta p/ lista p/ abrir outra (mesmo que ESC)
    if(EdButton((Rectangle){x + 300, 62, 110, 32}, "OPEN", 20)) {
        Editor_RequestLeave();
        return;
    }
    switch(activeTab) {
        case TAB_SONG: Edit_DrawPanel_Song(x, y); break;
        case TAB_SECTION: Edit_DrawPanel_Section(x, y); break;
        case TAB_DATA: Edit_DrawPanel_Data(x, y); break;
        case TAB_CHARTING: Edit_DrawPanel_Charting(x, y); break;
        default: Edit_DrawPanel_Events(x, y); break;
    }
}

/* Overlay de atalhos (TAB abre/fecha): cartao com tecla + o que faz, em duas colunas. */
typedef struct {
    const char* key;
    const char* what;
} HelpRow;

static const HelpRow edHelp[] = {
    {"Space", "play / pause"},
    {"F5 / F6", "test play / receptors only"},
    {"Ctrl+S", "save"},
    {"Ctrl+Z", "undo"},
    {"Click", "add note"},
    {"Right-click", "delete note"},
    {"Shift+Wheel", "long note length"},
    {"W / S, Wheel", "scroll"},
    {"G / H", "previous / next part"},
    {"Home", "go to part start"},
    {"N / X", "new / delete part"},
    {"C / V", "copy / paste part"},
    {"Q", "switch turn"},
    {"T", "grid snap"},
    {", / .", "part bpm (Shift = 10)"},
    {"Esc", "back to song list"},
};

void Edit_DrawHelpOverlay(void) {
    if(!EdHelpOpen()) return;
    enum { ROWS = (int)(sizeof(edHelp) / sizeof(edHelp[0])), PER_COL = (ROWS + 1) / 2 };
    DrawRectangle(0, 0, 1280, 720, (Color){0, 0, 0, 215});
    const Rectangle card = {170, 110, 940, 110 + PER_COL * 44};
    DrawRectangleRounded(card, 0.04f, 8, ED_C_POPUP);
    DrawRectangleRoundedLinesEx(card, 0.04f, 8, 1.5f, ED_C_EDGE);
    Ed_DrawHeader(card.x + 30, card.y + 26, "SHORTCUTS", ED_C_ACCENT);
    Ed_DrawText(mainFont, "TAB or ESC closes", (Vector2){card.x + card.width - 210, card.y + 30}, 18, 1, ED_C_DIM);
    for(int i = 0; i < ROWS; i++) {
        const float cx = card.x + 30 + (i / PER_COL) * 460;
        const float cy = card.y + 76 + (i % PER_COL) * 44;
        const Rectangle pill = {cx, cy, 150, 32};
        Ed_DrawBox(pill, ED_C_BTN, ED_C_EDGE);
        DrawTextEx(mainFont, edHelp[i].key, (Vector2){pill.x + 10, pill.y + 7}, 18, 1, WHITE);
        DrawTextEx(mainFont, edHelp[i].what, (Vector2){pill.x + 166, pill.y + 6}, 20, 1, ED_C_DIM);
    }
}
