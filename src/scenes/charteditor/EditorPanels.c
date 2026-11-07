/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: barra de menu, abas e paineis laterais (Song, Section, Note...). */
#include"scenes/AllScenes.h"
#include"data/Song.h"
#include"data/SongList.h"
#include "core/ModPath.h"
#include "logic/Character.h"
#include"logic/Stage.h"
#include"logic/Note.h"
#include "Game.h"
#include "GameData.h"
#include "raymath.h"
#include "EditorDataAndIO.h"
#include "EditorTimeline.h"
#include "EditorAudio.h"
#include "EditorWidgets.h"
#include "EditorGrid.h"
#include "EditorPanels.h"
#include "EditorCharactersAndLists.h"
#include "EditorPicker.h"

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

void Edit_DrawMenuBar(void) {
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
void Edit_DrawTabs(void) {
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

// Data: resumo do chart e do cursor (antes eram 2 abas com a contagem de notas repetida)
static void Edit_DrawPanel_Data(float x, float y) {
    char buf[160];
    char tbuf[32];
    SecFollowCursor();
    DrawTextEx(mainFont, "DATA", (Vector2){x, y}, 22, 1, GREEN);
    y += 32;
    EdTimeFmt(tbuf, sizeof(tbuf), cursorMs);
    snprintf(buf, sizeof(buf), "cursor: %s", tbuf);
    DrawTextEx(mainFont, buf, (Vector2){x, y}, 20, 1, WHITE);
    y += 30;
    snprintf(buf, sizeof(buf), "in part: %d notes", chart.secs[secIdx].count);
    DrawTextEx(mainFont, buf, (Vector2){x, y}, 20, 1, WHITE);
    y += 30;
    int total = 0;
    for(int i = 0; i < chart.nsec; i++) total += chart.secs[i].count;
    snprintf(buf, sizeof(buf), "total: %d notes in %d parts", total, chart.nsec);
    DrawTextEx(mainFont, buf, (Vector2){x, y}, 20, 1, WHITE);
    y += 30;
    EdTimeFmt(tbuf, sizeof(tbuf), SongEndMs());
    snprintf(buf, sizeof(buf), "length: %s", tbuf);
    DrawTextEx(mainFont, buf, (Vector2){x, y}, 20, 1, WHITE);
    y += 30;
    snprintf(buf, sizeof(buf), "dir: %s", chart.dir[0] ? chart.dir : "(unsaved)");
    DrawTextEx(mainFont, buf, (Vector2){x, y}, 18, 1, YELLOW);
    y += 28;
    snprintf(buf, sizeof(buf), "bpm %.1f  speed %.2f%s", (double)chart.bpm, (double)chart.speed, chart.dirty ? " *" : "");
    DrawTextEx(mainFont, buf, (Vector2){x, y}, 18, 1, YELLOW);
    y += 36;
    DrawTextEx(mainFont, "click adds  right-click deletes", (Vector2){x, y}, 18, 1, YELLOW);
    y += 24;
    DrawTextEx(mainFont, "SHIFT/CTRL+wheel = long note", (Vector2){x, y}, 18, 1, YELLOW);
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

void Edit_DrawPanel(void) {
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
