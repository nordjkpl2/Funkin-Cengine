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
#include <math.h>

// ---------- acoes de trecho (usadas no menu e nas abas, sem duplicar) ----------
static void Edit_DoNewPart(void) {
    EdTouchAddSec();
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
    EdStatusMsg("NEW PART AT END");
}

static void Edit_DoDelPart(void) {
    if(chart.nsec <= 1) { EdStatusMsg("CANNOT DELETE: ONLY 1 PART"); return; }
    EdTouchDelSec(secIdx);
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
    EdStatusMsg("PART COPIED");
}

static void Edit_DoPaste(void) {
    if(!copyValid) { EdStatusMsg("COPY FIRST (C)"); return; }
    EdTouchAddSec();
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
    EdStatusMsg("PASTED AT END");
}

/* Salva e abre o test play com o modo pedido (FULL = F5, RECEPTORS = F6, PLAY ONLY = botoes). */
void Edit_StartTest(PlayStateTestMode mode) {
    Audio_StopPlay();
    Editor_Save();
    PlayState_SetSongDir(chart.dir);   // zera o modo; SetTestMode vem depois
    PlayState_SetTestMode(mode);
    PlayState_SetReturnEditor(1);
    PlayState_SetScene();
}

static void Edit_DoTestPlay(void) {
    Edit_StartTest(PLAYSTATE_TEST_FULL);
}

/* Item do menu Play: com ok = 0 (arquivo ausente) o texto fica vermelho e o clique so avisa. */
/* lane >= 0: desenha a seta dessa lane (0..3) antes do texto; -1 = sem icone. */
static char PlayMenuItem(Rectangle r, const char* label, char ok, const char* missing, int lane) {
    Vector2 m = EdMouseWorld();
    char hov = (m.x >= r.x && m.x <= r.x + r.width && m.y >= r.y && m.y <= r.y + r.height);
    DrawRectangleRec(r, hov ? (Color){60,60,60,255} : (Color){18,18,18,255});
    float tx = r.x + 8;
    if(lane >= 0) {
        EdArrows_DrawIcon(lane, r.x + 16, r.y + r.height / 2, 22.0f, ok ? WHITE : (Color){255,255,255,90});
        tx = r.x + 34;
    }
    Ed_DrawText(mainFont, label, (Vector2){tx, r.y + 5}, 20, 1, ok ? WHITE : RED);
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

void Edit_DrawMenuBar(void) {
    DrawRectangle(0, 0, 1280, (int)MENU_H, (Color){16,16,16,255});
    DrawLine(0, (int)MENU_H, 1280, (int)MENU_H, (Color){60,60,60,255});
    Vector2 m = EdMouseWorld();
    const char* names[5] = {"File", "Edit", "View", "Sound", "Play"};
    for(int i = 0; i < 5; i++) {
        Rectangle r = {(float)(PANEL_X + i * 70), 0, 68, MENU_H};
        char hov = (m.x >= r.x && m.x <= r.x + r.width && m.y >= r.y && m.y <= r.y + r.height);
        char open = (menuOpen == i + 1);
        DrawRectangleRec(r, (open || hov) ? (Color){60,60,60,255} : (Color){16,16,16,255});
        Ed_DrawText(mainFont, names[i], (Vector2){r.x + 12, 5}, 20, 1, open ? GREEN : WHITE);
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
        Rectangle box = {mx, MENU_H, 230, 6 * 30 + 8};
        DrawRectangleRec(box, (Color){15,15,15,245});
        DrawRectangleLinesEx(box, 1, GREEN);
        if(EdMenuItemIcon((Rectangle){box.x + 4, box.y + 4, 222, 28}, "Save  Ctrl+S", ED_ICON_SAVE)) { menuOpen = 0; Editor_Save(); }
        else if(EdMenuItemIcon((Rectangle){box.x + 4, box.y + 32, 222, 28}, "Save As...", ED_ICON_SAVE)) { menuOpen = 0; saveAsBox.buf[0] = 0; importPath[0] = 0; ceDlg = DLG_SAVEAS; focusBox = &saveAsBox; }
        else if(EdMenuItemIcon((Rectangle){box.x + 4, box.y + 60, 222, 28}, "Save Compressed", ED_ICON_SAVE)) { menuOpen = 0; EdChart_SaveCompressed(&chart); EdStatusMsg("SAVED COMPRESSED"); }
        else if(EdMenuItemIcon((Rectangle){box.x + 4, box.y + 88, 222, 28}, "Save as binary", ED_ICON_SAVE)) { menuOpen = 0; if(EdChart_SaveBinary(&chart)) EdStatusMsg("SAVED BINARY .song"); else EdStatusMsg("ERROR SAVING BINARY"); }
        else if(EdMenuItemIcon((Rectangle){box.x + 4, box.y + 116, 222, 28}, "Save Events", ED_ICON_DOOR)) { menuOpen = 0; if(EdEvents_Save(chart.dir)) EdStatusMsg("EVENTS SAVED"); else EdStatusMsg("ERROR SAVING EVENTS"); }
        else if(EdMenuItemIcon((Rectangle){box.x + 4, box.y + 144, 222, 28}, "Back to list  ESC", ED_ICON_BACK)) { menuOpen = 0; Editor_RequestLeave(); }
        else if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if(!(m.x >= box.x && m.x <= box.x + box.width && m.y >= 0 && m.y <= box.y + box.height)) menuOpen = 0;
        }
    } else if(menuOpen == 2) {
        Rectangle box = {mx, MENU_H, 230, 5 * 30 + 8};
        DrawRectangleRec(box, (Color){15,15,15,245});
        DrawRectangleLinesEx(box, 1, GREEN);
        if(EdMenuItemIcon((Rectangle){box.x + 4, box.y + 4, 222, 28}, "Undo  Ctrl+Z", ED_ICON_RESTART)) { menuOpen = 0; Undo_Pop(); if(secIdx >= chart.nsec) secIdx = chart.nsec - 1; if(secIdx < 0) secIdx = 0; SecTimes_Invalidate(); chart.dirty = 1; }
        else if(EdMenuItemIcon((Rectangle){box.x + 4, box.y + 32, 222, 28}, "Copy part  C", ED_ICON_RESTART)) { menuOpen = 0; Edit_DoCopy(); }
        else if(EdMenuItemIcon((Rectangle){box.x + 4, box.y + 60, 222, 28}, "Paste at end  V", ED_ICON_RESTART)) { menuOpen = 0; Edit_DoPaste(); }
        else if(EdMenuItem((Rectangle){box.x + 4, box.y + 88, 222, 28}, "New part  N")) { menuOpen = 0; Edit_DoNewPart(); }
        else if(EdMenuItemIcon((Rectangle){box.x + 4, box.y + 116, 222, 28}, "Delete part  X", ED_ICON_TRASH)) { menuOpen = 0; Edit_DoDelPart(); }
        else if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if(!(m.x >= box.x && m.x <= box.x + box.width && m.y >= 0 && m.y <= box.y + box.height)) menuOpen = 0;
        }
    } else if(menuOpen == 3) {
        Rectangle box = {mx, MENU_H, 230, 2 * 30 + 8};
        DrawRectangleRec(box, (Color){15,15,15,245});
        DrawRectangleLinesEx(box, 1, GREEN);
        const char* followName = playFollow ? "Note player : ON" : "Note player : OFF";
        if(EdMenuItemIcon((Rectangle){box.x + 4, box.y + 4, 222, 28}, followName, ED_ICON_PLAYHEAD)) { menuOpen = 0; playFollow = !playFollow; }
        else if(EdMenuItem((Rectangle){box.x + 4, box.y + 32, 222, 28}, "Go to part start")) { menuOpen = 0; cursorMs = SecStartMs(secIdx); }
        else if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if(!(m.x >= box.x && m.x <= box.x + box.width && m.y >= 0 && m.y <= box.y + box.height)) menuOpen = 0;
        }
    } else if(menuOpen == 4) {
        // Sound: escuta so uma faixa dentro do editor (sem abrir a gameplay). Vermelho = arquivo ausente.
        Rectangle box = {mx, MENU_H, 260, 5 * 30 + 8};
        DrawRectangleRec(box, (Color){15,15,15,245});
        DrawRectangleLinesEx(box, 1, GREEN);
        if(PlayMenuItem((Rectangle){box.x + 4, box.y + 4, 252, 28}, "All sounds", 1, "", -1)) { menuOpen = 0; Audio_SetSolo(0); }
        else if(PlayMenuItem((Rectangle){box.x + 4, box.y + 32, 252, 28}, "Play only inst", edPlayHasInst, "NO Inst.ogg (ou mp3/wav)", 0)) { menuOpen = 0; Edit_PlaySolo(1, edPlayHasInst); }
        else if(PlayMenuItem((Rectangle){box.x + 4, box.y + 60, 252, 28}, "Play only vocals", edPlayHasVocals, "NO Voices (ou Voices-Player/Opponent)", 1)) { menuOpen = 0; Edit_PlaySolo(2, edPlayHasVocals); }
        else if(PlayMenuItem((Rectangle){box.x + 4, box.y + 88, 252, 28}, "Play only bf vocals", edPlayHasBf, "NO Voices-Player.ogg", 2)) { menuOpen = 0; Edit_PlaySolo(3, edPlayHasBf); }
        else if(PlayMenuItem((Rectangle){box.x + 4, box.y + 116, 252, 28}, "Play only opponent vocals", edPlayHasOpp, "NO Voices-Opponent.ogg", 3)) { menuOpen = 0; Edit_PlaySolo(4, edPlayHasOpp); }
        else if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if(!(m.x >= box.x && m.x <= box.x + box.width && m.y >= 0 && m.y <= box.y + box.height)) menuOpen = 0;
        }
    } else {
        // Play: testes do chart (abrem a gameplay).
        Rectangle box = {mx, MENU_H, 230, 2 * 30 + 8};
        DrawRectangleRec(box, (Color){15,15,15,245});
        DrawRectangleLinesEx(box, 1, GREEN);
        if(PlayMenuItem((Rectangle){box.x + 4, box.y + 4, 222, 28}, "Test Play  F5", 1, "", -1)) { menuOpen = 0; Edit_DoTestPlay(); }
        else if(PlayMenuItem((Rectangle){box.x + 4, box.y + 32, 222, 28}, "Only note receptor  F6", 1, "", -1)) { menuOpen = 0; Edit_StartTest(PLAYSTATE_TEST_RECEPTORS); }
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
    static TextMeasure tabM[TAB_COUNT]; // nomes das abas sao fixos: medidos uma vez
    Vector2 m = EdMouseWorld();
    for(int t = 0; t < TAB_COUNT; t++) {
        Rectangle r = {x0 + t * (tw + 4), y0, tw, 26};
        char active = (activeTab == t);
        char hov = (m.x >= r.x && m.x <= r.x + r.width && m.y >= r.y && m.y <= r.y + r.height);
        DrawRectangleRounded(r, 0.3f, 6, active ? (Color){40,80,50,255} : hov ? (Color){52,56,72,255} : (Color){26,30,42,255});
        DrawRectangleRoundedLinesEx(r, 0.3f, 6, 1.0f, active ? GREEN : (Color){90,96,120,255});
        if(active) DrawRectangleRounded((Rectangle){r.x + 14, r.y + r.height - 3, r.width - 28, 3}, 0.5f, 4, GREEN);
        Vector2 s = TextMeasure_Get(&tabM[t], mainFont, ChartTab_Name(t), 18, 1);
        Ed_DrawText(mainFont, ChartTab_Name(t), (Vector2){r.x + (r.width - s.x) / 2, r.y + 4}, 18, 1, active ? GREEN : WHITE);
        // com menu aberto, as abas ficam por baixo dele: o clique e do menu (senao troca de aba e fecha o menu)
        if(hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && ceDlg == DLG_NONE && !activeDrop && menuOpen == 0) {
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
    const char blocked = (ceDlg != DLG_NONE || activeDrop || menuOpen);
    const float boxW = SongFieldWidth();

    Ed_DrawHeader(x, y, "SONG", GREEN);
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
    if(EdButton((Rectangle){x + 200, y, 190, 40}, "SAVE AS", 20)) {
        saveAsBox.buf[0] = 0; importPath[0] = 0; ceDlg = DLG_SAVEAS; focusBox = &saveAsBox;
    }
}

static void Edit_DrawPanel_Section(float x, float y) {
    char blocked = (ceDlg != DLG_NONE || activeDrop || menuOpen);
    char buf[96];
    SecFollowCursor();
    snprintf(buf, sizeof(buf), "SECTION %d/%d", secIdx + 1, chart.nsec);
    Ed_DrawHeader(x, y, buf, GREEN);
    y += 32;
    Ed_DrawText(mainFont, "turn (TAB):", (Vector2){x, y + 4}, 22, 1, WHITE);
    if(EdButton((Rectangle){x + 150, y, 180, 32}, chart.secs[secIdx].mustHit ? "boyfriend" : "opponent", 20)) {
        EdTouchSec(secIdx); chart.secs[secIdx].mustHit = !chart.secs[secIdx].mustHit;
    }
    y += 42;
    // steps = tamanho do trecho em passos (16 = padrao, 4 tempos). Era "size:".
    Ed_DrawText(mainFont, "steps:", (Vector2){x, y + 4}, 22, 1, WHITE);
    {
        Rectangle r = {x + 150, y, 90, 32};
        if(focusNum != &nbLen) NumBox_Refresh(&nbLen, (float)chart.secs[secIdx].lenSteps, 1);
        NumBox_Update(&nbLen, r, blocked);
        NumBox_Draw(&nbLen, r);
    }
    Ed_DrawText(mainFont, "16=4 beats", (Vector2){x + 250, y + 8}, 18, 1, YELLOW);
    y += 42;
    Ed_DrawText(mainFont, "bpm:", (Vector2){x, y + 4}, 22, 1, WHITE);
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
    Ed_DrawHeader(x, y, "DATA", GREEN);
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
    Ed_DrawText(mainFont, buf, (Vector2){x, y}, 18, 1, YELLOW);
    y += 28;
    snprintf(buf, sizeof(buf), "bpm %.1f  speed %.2f%s", (double)chart.bpm, (double)chart.speed, chart.dirty ? " *" : "");
    Ed_DrawText(mainFont, buf, (Vector2){x, y}, 18, 1, YELLOW);
    y += 36;
    Ed_DrawText(mainFont, "click adds  right-click deletes", (Vector2){x, y}, 18, 1, YELLOW);
    y += 24;
    Ed_DrawText(mainFont, "SHIFT/CTRL+wheel = long note", (Vector2){x, y}, 18, 1, YELLOW);
}

static void Edit_DrawPanel_Charting(float x, float y) {
    Ed_DrawHeader(x, y, "CHARTING", GREEN);
    y += 34;
    if(EdButton((Rectangle){x, y, 190, 40}, playing ? "PAUSE" : "LISTEN", 22)) Audio_Toggle();
    y += 50;
    // arquivos de audio achados (flags ja existem, custo zero)
    Ed_DrawText(mainFont, edHasAudio ? "inst.ogg: yes" : "inst.ogg: no", (Vector2){x, y}, 18, 1,
        edHasAudio ? GREEN : RED);
    y += 24;
    const char* vocTxt = edHasSplit ? "vocals.ogg: split" : (edHasVoices ? "vocals.ogg: yes" : "vocals.ogg: no");
    Ed_DrawText(mainFont, vocTxt, (Vector2){x, y}, 18, 1, (edHasSplit || edHasVoices) ? GREEN : RED);
    y += 30;
    Ed_DrawText(mainFont, "space = play/stop", (Vector2){x, y}, 18, 1, YELLOW);
    y += 24;
    Ed_DrawText(mainFont, "drag red bar = seek", (Vector2){x, y}, 18, 1, YELLOW);
    y += 24;
    Ed_DrawText(mainFont, "W/S or wheel = scroll", (Vector2){x, y}, 18, 1, YELLOW);
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
    const char blocked = (ceDlg != DLG_NONE || activeDrop || menuOpen);
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
    DrawRectangleRounded(evPickBox, 0.05f, 6, (Color){12, 14, 24, 235});
    DrawRectangleRoundedLinesEx(evPickBox, 0.05f, 6, 2.0f, (Color){0, 200, 120, 255});

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
        DrawRectangleRounded((Rectangle){barX, y, 6, EV_PICK_LIST_H}, 0.5f, 4, (Color){40, 48, 70, 200});
        DrawRectangleRounded((Rectangle){barX, thumbY, 6, thumbH}, 0.5f, 4, GREEN);
    } else {
        evPickDrag = 0;
    }

    if(n == 0) {
        Ed_DrawText(mainFont, "no event found", (Vector2){x + 6, y + 8}, 18, 1, YELLOW);
        return;
    }

    // so desenha as linhas visiveis (linha inteira, sem corte)
    // mesmo visual do seletor de preset (Options > Note Colors): linha com hover, clique adiciona
    const Vector2 mouse = EdMouseWorld();
    const bool canClick = !(ceDlg != DLG_NONE || activeDrop || menuOpen);
    int idx = 0, row = 0;
    for(int i = 0; i < playStateEventTypeCount && row < EV_PICK_ROWS; i++) {
        const PlayStateEventType* t = &playStateEventTypes[i];
        if(!EvPickMatch(t)) continue;
        if(idx++ < evPickTop) continue;
        const Rectangle rr = {x + 4, y + (float)(row * EV_PICK_ROW_H) + 3, EV_PICK_W - 16, EV_PICK_ROW_H - 6};
        row++;
        const bool hov = canClick && CheckCollisionPointRec(mouse, rr);
        DrawRectangleRounded(rr, 0.25f, 6, hov ? (Color){52, 60, 86, 255} : (Color){28, 32, 46, 255});
        if(hov) DrawRectangleRoundedLinesEx(rr, 0.25f, 6, 1.5f, GREEN);
        EvDrawIcon((Rectangle){rr.x + 8, rr.y + (rr.height - EV_ICON_S) * 0.5f, EV_ICON_S, EV_ICON_S}, t->name);

        DrawTextEx(mainFont, t->name, (Vector2){rr.x + 50, rr.y + 5}, 20, 1, hov ? GREEN : WHITE);
        // descricao: encolhe a fonte se nao cabe na largura (nao vaza pra fora da caixa)
        const float maxW = rr.width - 60.0f;
        float fs = 15.0f;
        const float tw = MeasureTextEx(mainFont, t->description, fs, 1).x;
        if(tw > maxW) fs = fmaxf(10.0f, fs * maxW / tw);
        DrawTextEx(mainFont, t->description, (Vector2){rr.x + 50, rr.y + 28}, fs, 1, (Color){255, 214, 80, 255});

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
    TxtBox_Update(b, r, (ceDlg != DLG_NONE || activeDrop || menuOpen));
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
    Ed_DrawHeader(x, y, "EVENTS", GREEN);
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

    Ed_DrawText(mainFont, "song events:", (Vector2){x, y}, 20, 1, GREEN);
    y += 28;
    // lista com rolagem (roda do mouse): so EV_LIST_ROWS linhas por vez, com barra lateral
    const float listTop = y;
    const int maxTop = edEventCount > EV_LIST_ROWS ? edEventCount - EV_LIST_ROWS : 0;
    if(evListTop > maxTop) evListTop = maxTop;
    if(evListTop < 0) evListTop = 0;
    const int rowsDrawn = (edEventCount - evListTop < EV_LIST_ROWS) ? edEventCount - evListTop : EV_LIST_ROWS;
    evListBox = (Rectangle){x - 6, listTop - 4, 392, EV_LIST_ROWS * EV_LIST_ROW_H + 8};
    DrawRectangleRounded(evListBox, 0.08f, 6, (Color){14, 16, 24, 170});
    for(int k = 0; k < rowsDrawn; k++) {
        const int i = evListTop + k;
        const float ry = listTop + k * EV_LIST_ROW_H;
        char row[120];
        snprintf(row, sizeof(row), "%.0fms  %s", edEvents[i].time, edEvents[i].name);
        if(evSelected == i)  // linha selecionada: fundo verde suave
            DrawRectangleRounded((Rectangle){x - 2, ry - 2, 352, 30}, 0.25f, 6, (Color){60, 120, 80, 140});
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
        DrawRectangleRounded((Rectangle){x + 370, trackY, 6, trackH}, 0.5f, 4, (Color){50, 54, 70, 255});
        DrawRectangleRounded((Rectangle){x + 370, thumbY, 6, thumbH}, 0.5f, 4, (Color){120, 200, 140, 255});
    }
    y = listTop + rowsDrawn * EV_LIST_ROW_H;
    if(edEventCount == 0) Ed_DrawText(mainFont, "(nenhum)", (Vector2){x, y + 4}, 18, 1, YELLOW);
    y += 40;

    if(evSelected >= 0) {
        EdEvent* e = &edEvents[evSelected];
        snprintf(tb, sizeof(tb), "editing: %s", e->name);
        Ed_DrawText(mainFont, tb, (Vector2){x, y}, 20, 1, GREEN);
        y += 34;

        // Show Image: x/y aceitam negativo (a partir do centro) e passos de 10 px
        const PlayStateEventType* t = EvTypeOf(e->name);
        if(t != NULL && t->label1 == NULL) {
            // tipo sem valores (ex.: Hide Image): so informa
            Ed_DrawText(mainFont, "(no values)", (Vector2){x, y + 4}, 18, 1, YELLOW);
            y += 40;
            snprintf(tb, sizeof(tb), "%d event(s) in total", edEventCount);
            Ed_DrawText(mainFont, tb, (Vector2){x, y}, 18, 1, YELLOW);
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
            const char evBlocked = (ceDlg != DLG_NONE || activeDrop || menuOpen);
            Rectangle ir = {x, y, 250, 32};
            TxtBox_Update(&evImgBox, ir, evBlocked);
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
    Ed_DrawText(mainFont, tb, (Vector2){x, y}, 18, 1, YELLOW);
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

void Edit_DrawPanel(void) {
    float x = PANEL_X;
    float y = 70;
    SecFollowCursor();
    // sem bloco de fundo (pedido do dono): painel direto no bg, ajuda vai p/ baixo
    DrawLine((int)(PANEL_X - 8), 62, (int)(PANEL_X - 8), 672, (Color){60,60,60,255});
    char titleBuf[96];
    snprintf(titleBuf, sizeof(titleBuf), "%s%s", chart.name, chart.dirty ? "*" : "");
    // trunca nome longo p/ nao invadir OPEN nem a grade
    static TextMeasure titleM;
    for(int guard = 0; guard < 32; guard++) {
        Vector2 ts = TextMeasure_Get(&titleM, mainFont, titleBuf, 24, 2);
        if(ts.x <= 280 || titleBuf[0] == 0) break;
        size_t L = strlen(titleBuf);
        if(L > 4) { titleBuf[L - 4] = '.'; titleBuf[L - 3] = '.'; titleBuf[L - 2] = '.'; titleBuf[L - 1] = 0; }
        else break;
    }
    Ed_DrawText(mainFont, titleBuf, (Vector2){x, 66}, 24, 2, WHITE);
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
    // linha de ajuda enxuta: so aponta o TAB; a lista completa abre no overlay
    Ed_DrawText(mainFont, "TAB = shortcuts", (Vector2){PANEL_X, 690}, 18, 1, (Color){190, 255, 90, 255});
}

/* Overlay de atalhos: TAB abre/fecha, fundo preto cobrindo a tela. */
static char edHelpOpen = 0;
static const char* const edHelpLines[] = {
    "space play   F5 test   CTRL+S save   Q turn   T grid",
    "W/S scroll   N/X part   C/V copy/paste   ,/. bpm   ESC list",
    "click adds   right-click deletes   wheel = scroll",
    "G/H prev/next part   HOME goto part start"
};

void Edit_ToggleHelp(void) {
    edHelpOpen = !edHelpOpen;
}

void Edit_DrawHelpOverlay(void) {
    if(!edHelpOpen) return;
    DrawRectangle(0, 0, 1280, 720, (Color){0, 0, 0, 255});
    Ed_DrawHeader(120, 120, "ATALHOS", GREEN);
    for(int i = 0; i < 4; i++)
        Ed_DrawText(mainFont, edHelpLines[i], (Vector2){120, 190 + i * 44}, 26, 1, WHITE);
    Ed_DrawText(mainFont, "TAB closes", (Vector2){120, 420}, 20, 1, YELLOW);
}
