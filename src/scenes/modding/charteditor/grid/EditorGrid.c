/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: a grade (notas, linhas de tempo, cursor, mouse) e seu layout.
 *
 * Ordem de desenho pensada no lote do raylib: tudo que usa a textura branca (casas, linhas como
 * retangulos de 1 px, rastros) sai junto, e as setas (textura da skin) vem num passe so delas.
 * DrawLine usa outro modo (RL_LINES) e quebraria o lote, por isso as linhas sao retangulos. */

#include "characters/Character.h"
#include "logic/notes/Note.h"
#include "raymath.h"
#include "scenes/modding/charteditor/grid/EditorGrid.h"
#include "scenes/modding/charteditor/io/EditorDataAndIO.h"
#include "scenes/modding/charteditor/grid/EditorTimeline.h"
#include "scenes/modding/charteditor/audio/EditorAudio.h"
#include "scenes/modding/charteditor/ui/EditorWidgets.h"

Texture2D edIconO;
Texture2D edIconP;
char edIconOOk = 0;
char edIconPOk = 0;

#define GRID_X1 (GRID_X0 + GRID_W)

// barra da musica no topo (arrasta pra andar)
#define SBAR_X GRID_X0
#define SBAR_Y 42.0f
#define SBAR_W GRID_W
#define SBAR_H 8.0f

/* Notas a ate 30 px fora da grade ainda aparecem (cabeca cortada pela borda). Linhas de ocupacao
   cobrem essa faixa toda: 1 por pixel. */
#define NOTE_MARGIN 30.0f
#define OCC_TOP (GRID_Y0 - NOTE_MARGIN)
enum { OCC_ROWS = (int)(GRID_Y1 - GRID_Y0 + 2 * NOTE_MARGIN) + 1 };

static const Color LINE_STEP = {255, 255, 255, 26};
static const Color LINE_BEAT = {255, 255, 255, 70};
static const Color LINE_PART = {255, 170, 60, 230};
static const Color CURSOR_C = {255, 70, 110, 255};

static char barDrag = 0;

// ---------- setas (sprites da skin, pre-calculados) ----------

/* Frame 0 da animacao de cada cor ja medido: desenhar e so um DrawTexturePro,
   sem copiar/restaurar objeto animado por nota. */
typedef struct {
    Texture2D tex;
    Rectangle src;
    float w, h;   // tamanho na tela com escala 1 (lado maior = ARROW_PX)
    char ok;
} EdArrowSprite;

#define ARROW_PX 56.0f

static EdArrowSprite edArrows[4];
static char edArrowsReady = 0;

void EdArrows_Init(void) {
    if(edArrowsReady)
        return;
    const RayAnimationHandler* nah = Note_GetCurrentSkin();
    const char* names[4] = {"purple", "blue", "green", "red"};   // sprites claros, iguais aos do jogo
    for(int i = 0; i < 4; i++) {
        EdArrowSprite* a = edArrows + i;
        a->ok = 0;
        const int ai = (nah != NULL) ? AnimationSet_FindAnimation(nah, names[i]) : -1;
        if(ai < 0)
            continue;
        const RayAnimation* an = nah->animations + ai;
        if(an->frames == NULL || an->frameCount <= 0)
            continue;
        const Frame* fr = an->frames;
        const float big = (float)((fr->w > fr->h) ? fr->w : fr->h);
        const float s = (big > 1) ? ARROW_PX / big : 1.0f;
        a->tex = nah->texture;
        a->src = (Rectangle){(float)fr->x, (float)fr->y, (float)fr->w, (float)fr->h};
        a->w = fr->w * s;
        a->h = fr->h * s;
        a->ok = 1;
    }
    edArrowsReady = 1;
}

void EdArrows_Reset(void) {
    edArrowsReady = 0;
    EdArrows_Init();
}

// seta da lane (0..3) centrada em (cx, cy); k = escala sobre ARROW_PX
static void EdArrow_Draw(int lane, float cx, float cy, float k, Color tint) {
    const EdArrowSprite* a = edArrows + (lane & 3);
    if(!a->ok || tint.a == 0)
        return;
    const float w = a->w * k, h = a->h * k;
    DrawTexturePro(a->tex, a->src, (Rectangle){cx - w * 0.5f, cy - h * 0.5f, w, h}, (Vector2){0, 0}, 0, tint);
}

// icone de seta nos menus (~size pixels)
void EdArrows_DrawIcon(int lane, float cx, float cy, float size, Color tint) {
    if(edArrowsReady)
        EdArrow_Draw(lane, cx, cy, size / ARROW_PX, tint);
}

// ---------- geometria ----------

float EdPxPerMs(void) {
    return (GRID_Y1 - GRID_Y0) / (float)VIEW_MS;
}

static float Grid_Y(double t, float ppm) {
    return GRID_Y0 + (float)((t - viewTopMs) * ppm);
}

static float Grid_LaneCX(int dispLane) {
    return GRID_X0 + dispLane * LANE_W + LANE_W * 0.5f;
}

int EdGrid_LaneAt(Vector2 m) {
    if(m.x < GRID_X0 || m.x >= GRID_X1 || m.y < GRID_Y0 || m.y > GRID_Y1)
        return -1;
    return (int)((m.x - GRID_X0) / LANE_W);
}

double EdGrid_TimeAt(float y) {
    return viewTopMs + (y - GRID_Y0) / EdPxPerMs();
}

static Color EdLaneColor(int lane) {
    switch(lane & 3) {
        case 0: return (Color){200, 80, 255, 255};
        case 1: return (Color){80, 180, 255, 255};
        case 2: return (Color){80, 255, 150, 255};
        default: return (Color){255, 90, 90, 255};
    }
}

static float Grid_SongFrac(void) {
    const double end = fmax(SongEndMs(), 1000.0);
    return Clamp((float)(cursorMs / end), 0.0f, 1.0f);
}

// ---------- desenho ----------

// moldura, casas, lado do jogador, casa sob o mouse e icones dos cantores
static void Grid_DrawBackground(int hoverLane) {
    const float gridH = GRID_Y1 - GRID_Y0;
    const Rectangle frame = {GRID_X0 - 12, MENU_H + 6, GRID_W + 24, GRID_Y1 + 10 - (MENU_H + 6)};
    DrawRectangleRounded(frame, 0.03f, 6, ED_C_POPUP);
    DrawRectangleRoundedLinesEx(frame, 0.03f, 6, 1.0f, ED_C_EDGE);

    for(int l = 0; l < 8; l++) {
        const float x = GRID_X0 + l * LANE_W;
        DrawRectangleRec((Rectangle){x, GRID_Y0, LANE_W, gridH}, (l & 1) ? (Color){26, 23, 46, 255} : (Color){21, 19, 38, 255});
        DrawRectangleRec((Rectangle){x + 8, GRID_Y0 - 6, LANE_W - 16, 3}, EdLaneColor(l));   // cor da seta da casa
    }
    // seu lado (direita) levemente verde
    DrawRectangleRec((Rectangle){GRID_X0 + LANE_W * 4, GRID_Y0, LANE_W * 4, gridH}, (Color){120, 230, 150, 24});
    if(hoverLane >= 0)
        DrawRectangleRec((Rectangle){GRID_X0 + hoverLane * LANE_W, GRID_Y0, LANE_W, gridH}, (Color){255, 255, 255, 14});
    // divisoria entre os dois lados
    DrawRectangleRec((Rectangle){GRID_X0 + LANE_W * 4 - 1, GRID_Y0 - 30, 2, gridH + 30}, ED_C_EDGE);

    // icones dos cantores em cima (igual a barra de vida do jogo)
    if(edIconOOk)
        DrawTexturePro(edIconO, Character_IconFrame(edIconO),
            (Rectangle){GRID_X0 + LANE_W * 2 - 22, GRID_Y0 - 52, 44, 44}, (Vector2){0, 0}, 0, WHITE);
    if(edIconPOk)
        DrawTexturePro(edIconP, Character_IconFrame(edIconP),
            (Rectangle){GRID_X0 + LANE_W * 6 - 22, GRID_Y0 - 52, 44, 44}, (Vector2){0, 0}, 0, WHITE);
}

// barra da musica: trilho, parte tocada e botao
static void Grid_DrawSongBar(void) {
    const float frac = Grid_SongFrac();
    DrawRectangleRounded((Rectangle){SBAR_X, SBAR_Y, SBAR_W, SBAR_H}, 1.0f, 6, (Color){255, 255, 255, 28});
    DrawRectangleRounded((Rectangle){SBAR_X, SBAR_Y, fmaxf(frac * SBAR_W, SBAR_H), SBAR_H}, 1.0f, 6, ED_C_ACCENT);
    const Vector2 knob = {SBAR_X + frac * SBAR_W, SBAR_Y + SBAR_H * 0.5f};
    DrawCircleV(knob, barDrag ? 9.0f : 7.0f, WHITE);
    DrawCircleV(knob, 3.5f, ED_C_ACCENT);
}

// linhas de tempo por trecho (so os trechos e passos que aparecem na tela)
static void Grid_DrawTimeLines(float ppm, double viewBottom) {
    int first, last;
    SecRangeInView(viewTopMs, viewBottom, &first, &last);

    for(int i = first; i <= last; i++) {
        const double start = SecStartMs(i);
        const double end = (i + 1 < chart.nsec) ? SecStartMs(i + 1) : SongEndMs() + 4000;
        if(end < viewTopMs || start > viewBottom)
            continue;
        if(i == secIdx) {   // trecho atual: fundo claro + marca rosa na lateral
            const float ya = Grid_Y(fmax(start, viewTopMs), ppm);
            const float yb = Grid_Y(fmin(end, viewBottom), ppm);
            DrawRectangleRec((Rectangle){GRID_X0, ya, GRID_W, yb - ya}, (Color){255, 255, 255, 10});
            DrawRectangleRec((Rectangle){GRID_X0 - 7, ya, 3, yb - ya}, ED_C_ACCENT);
        }
        const double stepMs = 60000.0 / chart.secs[i].bpm / snapDiv;
        if(stepMs > 0) {
            // do 1o passo visivel ate o fim do trecho; o passo 0 e a linha do trecho (desenhada abaixo)
            long n = (viewTopMs > start) ? (long)ceil((viewTopMs - start) / stepMs) : 1;
            if(n < 1) n = 1;
            for(;; n++) {
                const double t = start + n * stepMs;
                if(t >= end - 0.5 || t > viewBottom)
                    break;
                const char isBeat = (n % snapDiv) == 0;
                DrawRectangleRec((Rectangle){GRID_X0, floorf(Grid_Y(t, ppm)), GRID_W, 1}, isBeat ? LINE_BEAT : LINE_STEP);
            }
        }
        if(start >= viewTopMs && start <= viewBottom)
            DrawRectangleRec((Rectangle){GRID_X0 - 8, floorf(Grid_Y(start, ppm)) - 1, GRID_W + 16, 2}, LINE_PART);
    }
}

// Note player ON: passado apaga, na agulha acende (so visual, sem tocar no chart)
typedef enum { LOOK_NORMAL, LOOK_PAST, LOOK_HIT } NoteLook;

static NoteLook Note_Look(const EdNote* n) {
    if(!playFollow)
        return LOOK_NORMAL;
    const float dt = n->time - (float)cursorMs;
    if(playing && fabsf(dt) <= 200.0f)
        return LOOK_HIT;
    return (dt < 0) ? LOOK_PAST : LOOK_NORMAL;
}

/* Trecho pode ter nota entre top e bot? (limites tmin/tmax: nao depende de a nota estar
   dentro do tempo nominal do trecho, entao charts baguncados carregam) */
static char Sec_MayShow(const EdSec* s, double top, double bot) {
    return s->count > 0 && s->tmax >= top && s->tmin <= bot;
}

/* Passe 1: rastros (textura branca). Um rastro por casa e pixel de topo: se ja ha um igual ou mais
   longo ali, o novo ficaria escondido atras (mesma cor; o alfa somado so escureceria).
   Comeca lenMax antes do topo, senao a cauda some quando a cabeca sai da tela. */
static void Grid_DrawSustains(float ppm, double top, double bot) {
    static unsigned short susEnd[8][OCC_ROWS];
    memset(susEnd, 0, sizeof(susEnd));

    for(int i = 0; i < chart.nsec; i++) {
        const EdSec* s = chart.secs + i;
        if(s->lenMax <= 0 || !Sec_MayShow(s, top, bot))
            continue;
        for(int k = EdSec_LowerBound(s, (float)(top - s->lenMax)); k < s->count; k++) {
            const EdNote* n = s->notes + k;
            if(n->time > bot)
                break;
            if(n->len <= 0 || n->lane < 0 || n->lane >= 8)
                continue;
            const float y = Grid_Y(n->time, ppm);
            const float y0 = fmaxf(y, GRID_Y0);
            const float y1 = fminf(y + fmaxf(n->len * ppm, 6.0f), GRID_Y1);
            if(y1 <= y0)
                continue;
            const int dl = EdDispLane(s, n->lane);
            const NoteLook look = Note_Look(n);
            if(!playFollow) {   // com Note player o alfa muda por nota: nada se esconde
                const int r0 = (int)(y0 - OCC_TOP);
                const int r1 = (int)(y1 - OCC_TOP) + 1;
                if(r0 >= 0 && r0 < OCC_ROWS && r1 <= OCC_ROWS) {
                    if(susEnd[dl][r0] >= r1)
                        continue;
                    susEnd[dl][r0] = (unsigned short)r1;
                }
            }
            Color c = EdLaneColor(n->lane);
            c.a = (look == LOOK_PAST) ? 50 : (look == LOOK_HIT) ? 255 : 160;
            DrawRectangleRec((Rectangle){Grid_LaneCX(dl) - 7, y0, 14, y1 - y0}, c);
        }
    }
}

/* Passe 2: setas (textura da skin). Notas curtas empilhadas no mesmo pixel e casa saem uma vez so:
   o custo fica em ~8 casas x altura da grade, por maior que seja a densidade do chart. */
static void Grid_DrawHeads(float ppm, double top, double bot) {
    static unsigned char occ[8][OCC_ROWS];
    memset(occ, 0, sizeof(occ));

    for(int i = 0; i < chart.nsec; i++) {
        const EdSec* s = chart.secs + i;
        if(!Sec_MayShow(s, top, bot))
            continue;
        for(int k = EdSec_LowerBound(s, (float)top); k < s->count; k++) {
            const EdNote* n = s->notes + k;
            if(n->time > bot)
                break;
            if(n->lane < 0 || n->lane >= 8)
                continue;
            const int dl = EdDispLane(s, n->lane);
            const float y = Grid_Y(n->time, ppm);
            if(n->len <= 0) {
                const int row = (int)(y - OCC_TOP);
                if(row >= 0 && row < OCC_ROWS) {
                    if(occ[dl][row])
                        continue;
                    occ[dl][row] = 1;
                }
            }
            const NoteLook look = Note_Look(n);
            const float cx = Grid_LaneCX(dl);
            if(look == LOOK_HIT) {
                DrawCircleV((Vector2){cx, y + 1}, 34.0f, (Color){255, 255, 255, 60});
                DrawCircleV((Vector2){cx, y + 1}, 26.0f, (Color){255, 255, 200, 90});
            }
            EdArrow_Draw(n->lane, cx, y + 1, (look == LOOK_HIT) ? 1.25f : 1.0f,
                (look == LOOK_PAST) ? (Color){255, 255, 255, 90} : WHITE);
        }
    }
}

// cursor: faixa com brilho + agulha a esquerda (as notas sobem ate ele, igual ao jogo)
static void Grid_DrawCursor(float ppm) {
    const float y = floorf(Grid_Y(cursorMs, ppm));
    if(y < GRID_Y0 - NOTE_MARGIN || y > GRID_Y1 + NOTE_MARGIN)
        return;
    const float x0 = GRID_X0 - 8;
    const float w = GRID_W + 16;
    DrawRectangleRec((Rectangle){x0, y - 5, w, 10}, (Color){255, 70, 110, 50});
    DrawRectangleRec((Rectangle){x0, y - 1, w, 3}, CURSOR_C);
    DrawRectangleRec((Rectangle){x0, y, w, 1}, WHITE);
    DrawTriangle((Vector2){x0 - 18, y - 10}, (Vector2){x0 - 18, y + 11}, (Vector2){x0 + 1, y + 0.5f}, CURSOR_C);
}

// ---------- mouse ----------

// barra da musica: clica e arrasta pra andar na musica
static void Grid_BarMouse(Vector2 m) {
    const Rectangle hit = {SBAR_X, SBAR_Y - 8, SBAR_W, SBAR_H + 16};
    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(m, hit))
        barDrag = 1;
    if(!barDrag)
        return;
    if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        const double end = fmax(SongEndMs(), 1000.0);
        cursorMs = Clamp((m.x - SBAR_X) / SBAR_W, 0.0f, 1.0f) * end;
        viewTopMs = fmax(cursorMs - VIEW_MS * 0.5, -500.0);
    } else {
        if(playing)
            Audio_SeekAll(cursorMs);
        barDrag = 0;
    }
}

// mouse na grade: fantasma onde a nota vai cair (X vermelho = ja tem).
// Esquerdo SO poe, direito SO apaga. O encaixe conta do comeco do trecho (bate com as linhas).
static void Grid_NoteMouse(Vector2 m, float ppm, int lane) {
    if(lane < 0 || barDrag)
        return;
    const double t = fmax(EdGrid_TimeAt(m.y), 0.0);
    const int si = SecAtTime(t);
    EdSec* s = chart.secs + si;
    const int raw = EdDispLane(s, lane);   // casa da tela -> casa do arquivo
    const double beatMs = 60000.0 / s->bpm;
    const double stepMs = beatMs / snapDiv;
    const double start = SecStartMs(si);
    const float snapped = (float)fmax(start + round((t - start) / stepMs) * stepMs, 0.0);
    const float gy = Grid_Y(snapped, ppm) + 1;
    const float cx = Grid_LaneCX(lane);
    const int exist = EdSec_FindNear(s, snapped, raw, fmaxf((float)stepMs / 2, 15.0f));

    if(gy >= GRID_Y0 - 26 && gy <= GRID_Y1 + 26) {
        if(exist >= 0) {
            DrawLineEx((Vector2){cx - 13, gy - 13}, (Vector2){cx + 13, gy + 13}, 3.0f, ED_C_BAD);
            DrawLineEx((Vector2){cx - 13, gy + 13}, (Vector2){cx + 13, gy - 13}, 3.0f, ED_C_BAD);
        } else {
            EdArrow_Draw(raw, cx, gy, 1.0f, (Color){255, 255, 255, 150});
        }
    }

    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && exist < 0) {
        EdTouchSec(si);
        EdSec_Add(s, snapped, raw, 0);
        secIdx = si;
        cursorMs = snapped;
    }
    if(IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        const int idx = EdSec_FindNear(s, (float)t, raw, (float)beatMs);
        if(idx >= 0) {
            EdTouchSec(si);   /* antes de remover: o undo precisa do estado com a nota */
            EdSec_RemoveAt(s, idx);
            secIdx = si;
        }
    }
}

void Edit_DrawGrid(void) {
    const float ppm = EdPxPerMs();
    const double viewBottom = viewTopMs + VIEW_MS;
    const double top = viewTopMs - NOTE_MARGIN / ppm;
    const double bot = viewBottom + NOTE_MARGIN / ppm;
    // mouse travado com janela, lista, menu ou ajuda abertos (o clique que fecha nao poe nota)
    const char mouseOn = !EdUiBlocked() && !EdHelpOpen();
    const Vector2 m = EdMouseWorld();
    const int lane = mouseOn ? EdGrid_LaneAt(m) : -1;

    Grid_DrawBackground(lane);
    Grid_DrawSongBar();
    Grid_DrawTimeLines(ppm, viewBottom);
    Grid_DrawSustains(ppm, top, bot);
    Grid_DrawHeads(ppm, top, bot);
    Grid_DrawCursor(ppm);

    if(mouseOn) {
        Grid_BarMouse(m);
        Grid_NoteMouse(m, ppm, lane);
    }
}
