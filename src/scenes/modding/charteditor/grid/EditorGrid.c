/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: a grade (notas, linhas de tempo, cursor, mouse) e seu layout.
 *
 * Ordem de desenho pensada no lote do raylib: tudo que usa a textura branca (casas, linhas como
 * retangulos de 1 px, rastros) sai junto, e as setas (textura da skin) vem num passe so delas.
 * DrawLine usa outro modo (RL_LINES) e quebraria o lote, por isso as linhas sao retangulos. */

#include "characters/Character.h"
#include "logic/notes/Note.h"
#include "GameData.h"
#include "scenes/gameplay/notes/PlayStateNoteAtlas.h"
#include "scenes/gameplay/notes/PlayStateNoteSpawn.h"
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

/* Frame 0 de cada parte da nota ja medido: desenhar e so um DrawTexturePro.
   Cor das colunas igual a gameplay: atlas recolorido (NoteAtlas, 1 textura pras 4 cores = 1 lote);
   se o atlas nao couber, cai no shader de matiz, 1 troca por coluna (no maximo 4 por frame). */
typedef struct {
    Rectangle src;
    float w, h;   // tamanho na tela (cabeca: lado maior = ARROW_PX; rastro e ponta na mesma escala)
    char ok;
} EdSprite;

typedef struct {
    Texture2D tex;
    EdSprite head, trail, end;
} EdArrowSprite;

#define ARROW_PX 56.0f

static EdArrowSprite edArrows[4];
static Vector3 edShift[4];      // offset HSB da coluna (GameData_LaneShift)
static char edUseShader = 0;    // 1 = sem atlas e alguma coluna colorida: desenha por coluna com shader
static char edArrowsReady = 0;

static EdSprite EdSprite_From(const RayAnimationHandler* set, const char* name) {
    EdSprite sp = {0};
    const int ai = AnimationSet_FindAnimation(set, name);
    if(ai < 0)
        return sp;
    const RayAnimation* an = set->animations + ai;
    if(an->frames == NULL || an->frameCount <= 0)
        return sp;
    const Frame* fr = an->frames;
    sp.src = (Rectangle){(float)fr->x, (float)fr->y, (float)fr->w, (float)fr->h};
    sp.w = (float)fr->w;
    sp.h = (float)fr->h;
    sp.ok = fr->w > 0 && fr->h > 0;
    return sp;
}

void EdArrows_Init(void) {
    if(edArrowsReady)
        return;
    static const char* const heads[4] = {"purple", "blue", "green", "red"};   // mesmos nomes do jogo (Note.c)
    static const char* const trails[4] = {"purple hold piece", "blue hold piece", "green hold piece", "red hold piece"};
    static const char* const ends[4] = {"purple hold end", "blue hold end", "green hold end", "red hold end"};
    const RayAnimationHandler* base = Note_GetCurrentSkin();

    char anyShift = 0;
    for(int i = 0; i < 4; i++) {
        edShift[i] = GameData_LaneShift(i);
        anyShift |= GameData_LaneShifted(i);
    }
    NoteAtlas_Free();
    const char atlas = (base != NULL && anyShift) ? (char)NoteAtlas_Build(base, edShift) : 0;
    edUseShader = anyShift && !atlas;

    for(int i = 0; i < 4; i++) {
        EdArrowSprite* a = edArrows + i;
        memset(a, 0, sizeof(*a));
        const RayAnimationHandler* set = atlas ? NoteAtlas_Lane(i) : base;
        if(set == NULL || set->animations == NULL)
            continue;
        a->head = EdSprite_From(set, heads[i]);
        if(!a->head.ok)
            continue;
        // escala da cabeca vale pro rastro e a ponta: mesma proporcao da gameplay
        const float big = fmaxf(a->head.w, a->head.h);
        const float k = (big > 1) ? ARROW_PX / big : 1.0f;
        a->trail = EdSprite_From(set, trails[i]);
        a->end = EdSprite_From(set, ends[i]);
        EdSprite* parts[3] = {&a->head, &a->trail, &a->end};
        for(int p = 0; p < 3; p++) {
            parts[p]->w *= k;
            parts[p]->h *= k;
        }
        a->tex = set->texture;
    }
    edArrowsReady = 1;
}

void EdArrows_Reset(void) {
    edArrowsReady = 0;
    EdArrows_Init();
}

void EdArrows_Free(void) {
    NoteAtlas_Free();
    PlayStateNoteSpawn_ReleaseTint();
    edArrowsReady = 0;
}

static void EdSprite_Draw(const EdArrowSprite* a, const EdSprite* sp, Rectangle dst, Color tint) {
    DrawTexturePro(a->tex, sp->src, dst, (Vector2){0, 0}, 0, tint);
}

// seta da lane (0..3) centrada em (cx, cy); k = escala sobre ARROW_PX
static void EdArrow_Draw(int lane, float cx, float cy, float k, Color tint) {
    const EdArrowSprite* a = edArrows + (lane & 3);
    if(!a->head.ok || tint.a == 0)
        return;
    const float w = a->head.w * k, h = a->head.h * k;
    EdSprite_Draw(a, &a->head, (Rectangle){cx - w * 0.5f, cy - h * 0.5f, w, h}, tint);
}

// cor da coluna sem atlas (shader); com atlas ou sem cor nao faz nada
static void EdLane_Begin(int lane) {
    if(edUseShader)
        PlayStateNoteSpawn_BeginShift(edShift[lane & 3]);
}

static void EdLane_End(void) {
    if(edUseShader)
        PlayStateNoteSpawn_EndShift();
}

// icone de seta nos menus (~size pixels)
void EdArrows_DrawIcon(int lane, float cx, float cy, float size, Color tint) {
    if(!edArrowsReady)
        return;
    EdLane_Begin(lane);
    EdArrow_Draw(lane, cx, cy, size / ARROW_PX, tint);
    EdLane_End();
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

static NoteLook Note_Look(float time) {
    if(!playFollow)
        return LOOK_NORMAL;
    const float dt = time - (float)cursorMs;
    if(playing && fabsf(dt) <= 200.0f)
        return LOOK_HIT;
    return (dt < 0) ? LOOK_PAST : LOOK_NORMAL;
}

/* Trecho pode ter nota (ou rastro: tmax inclui o len) entre top e bot? Nao depende de a nota
   estar dentro do tempo nominal do trecho, entao charts baguncados carregam. */
static char Sec_MayShow(const EdSec* s, double top, double bot) {
    return s->count > 0 && s->tmax >= top && s->tmin <= bot;
}

/* ---------- notas: coleta + desenho em lote (mesma ideia da gameplay) ----------
   1) cabeca no mesmo pixel e casa de outra ja coletada nao sai de novo; rastro igual ou mais curto
      no mesmo topo tambem nao (ficaria escondido atras). Custo ~8 casas x altura da grade.
   2) teto de desenho por frame: as notas vem em ordem de tempo, o corte so pega as de baixo.
   3) teto de notas lidas por frame: spam de milhares no mesmo pixel nao trava a CPU.
   Setas e rastros usam 1 textura (atlas ou skin): 1 lote. O brilho do Note player (textura branca)
   sai antes, num passe so dele, pra nao alternar textura por nota. */
#define ED_DRAW_BUDGET 2048    // cabecas + rastros (cada rastro conta 2: corpo e ponta)
#define ED_SCAN_BUDGET 32768   // notas lidas por frame

typedef struct {
    float cx, y0, y1;   // cabeca: y0 = centro; rastro: de y0 ate y1
    unsigned char lane, look;
} EdDrawItem;

static EdDrawItem edHeads[ED_DRAW_BUDGET];
static EdDrawItem edTrails[ED_DRAW_BUDGET / 2];
static int edHeadN, edTrailN, edBudget, edScan;

// marcadas com a geracao do frame: sem memset por frame
static unsigned short occHead[8][OCC_ROWS];
static unsigned short occTrailGen[8][OCC_ROWS];
static unsigned short occTrailEnd[8][OCC_ROWS];   // fim (linha + 1) do maior rastro com esse topo

static unsigned short occGen = 0;

static void Notes_NextGen(void) {
    if(++occGen == 0) {   // wraparound: limpa uma vez a cada 65535 frames
        memset(occHead, 0, sizeof(occHead));
        memset(occTrailGen, 0, sizeof(occTrailGen));
        occGen = 1;
    }
}

static void Notes_AddHead(const EdNote* n, int dl, float y) {
    const float rf = y - OCC_TOP;
    if(!(rf >= 0 && rf < OCC_ROWS))
        return;   // fora da faixa (ou NaN)
    const int row = (int)rf;
    if(occHead[dl][row] == occGen)
        return;
    occHead[dl][row] = occGen;
    if(edBudget <= 0)
        return;
    edBudget--;
    edHeads[edHeadN++] = (EdDrawItem){Grid_LaneCX(dl), y + 1, 0, (unsigned char)(n->lane & 3), (unsigned char)Note_Look(n->time)};
}

static void Notes_AddTrail(const EdNote* n, int dl, float y, float ppm) {
    const float y0 = fmaxf(y, GRID_Y0);
    const float y1 = fminf(y + fmaxf(n->len * ppm, 6.0f), GRID_Y1);
    if(!(y1 > y0))
        return;
    const int r0 = (int)(y0 - OCC_TOP);
    const int r1 = (int)(y1 - OCC_TOP) + 1;
    if(r0 < 0 || r0 >= OCC_ROWS || r1 > OCC_ROWS)
        return;
    // mesmo topo = mesmo tempo = mesma cor e mesmo look: o mais curto ficaria escondido
    if(occTrailGen[dl][r0] == occGen && occTrailEnd[dl][r0] >= r1)
        return;
    occTrailGen[dl][r0] = occGen;
    occTrailEnd[dl][r0] = (unsigned short)r1;
    if(edBudget < 2 || edTrailN >= ED_DRAW_BUDGET / 2)
        return;
    edBudget -= 2;
    edTrails[edTrailN++] = (EdDrawItem){Grid_LaneCX(dl), y0, y1, (unsigned char)(n->lane & 3), (unsigned char)Note_Look(n->time)};
}

/* Le so as notas que aparecem: top..bot (cabecas e rastros que comecam na tela) e, a parte,
   top-lenMax..top (so rastros vindos de cima, senao a cauda some quando a cabeca sai). */
static void Notes_Collect(float ppm, double top, double bot) {
    edHeadN = edTrailN = 0;
    edBudget = ED_DRAW_BUDGET;
    edScan = ED_SCAN_BUDGET;
    Notes_NextGen();

    for(int i = 0; i < chart.nsec && edScan > 0 && edBudget > 0; i++) {
        const EdSec* s = chart.secs + i;
        if(!Sec_MayShow(s, top, bot))
            continue;
        const int kTop = EdSec_LowerBound(s, (float)top);
        for(int k = kTop; k < s->count && edScan > 0; k++, edScan--) {
            const EdNote* n = s->notes + k;
            if(n->time > bot)
                break;
            if(n->lane < 0 || n->lane >= 8)
                continue;
            const int dl = EdDispLane(s, n->lane);
            const float y = Grid_Y(n->time, ppm);
            if(n->len > 0)
                Notes_AddTrail(n, dl, y, ppm);
            Notes_AddHead(n, dl, y);
        }
        if(s->lenMax <= 0)
            continue;
        for(int k = EdSec_LowerBound(s, (float)(top - s->lenMax)); k < kTop && edScan > 0; k++, edScan--) {
            const EdNote* n = s->notes + k;
            if(n->len <= 0 || n->lane < 0 || n->lane >= 8 || n->time + n->len < top)
                continue;
            Notes_AddTrail(n, EdDispLane(s, n->lane), Grid_Y(n->time, ppm), ppm);
        }
    }
}

static Color Look_Tint(unsigned char look, unsigned char normal, unsigned char past) {
    const unsigned char a = (look == LOOK_PAST) ? past : (look == LOOK_HIT) ? 255 : normal;
    return (Color){255, 255, 255, a};
}

// rastro: corpo esticado (hold piece) + ponta (hold end), igual a gameplay; skin sem eles = faixa da cor
static void Notes_DrawTrail(const EdDrawItem* it) {
    const EdArrowSprite* a = edArrows + it->lane;
    const Color tint = Look_Tint(it->look, 200, 60);
    const float h = it->y1 - it->y0;
    if(!a->trail.ok) {
        Color c = GameData_ShiftColor(EdLaneColor(it->lane), edShift[it->lane]);
        c.a = tint.a;
        DrawRectangleRec((Rectangle){it->cx - 7, it->y0, 14, h}, c);
        return;
    }
    const float endH = a->end.ok ? fminf(a->end.h, h) : 0.0f;
    const float bodyH = h - endH;
    if(bodyH > 0)
        EdSprite_Draw(a, &a->trail, (Rectangle){it->cx - a->trail.w * 0.5f, it->y0, a->trail.w, bodyH}, tint);
    if(endH > 0)
        EdSprite_Draw(a, &a->end, (Rectangle){it->cx - a->end.w * 0.5f, it->y0 + bodyH, a->end.w, endH}, tint);
}

static void Notes_DrawHead(const EdDrawItem* it) {
    EdArrow_Draw(it->lane, it->cx, it->y0, (it->look == LOOK_HIT) ? 1.25f : 1.0f, Look_Tint(it->look, 255, 90));
}

// uma coluna (lane < 0 = todas): rastros embaixo, cabecas por cima
static void Notes_DrawPass(int lane) {
    for(int i = 0; i < edTrailN; i++)
        if(lane < 0 || edTrails[i].lane == lane)
            Notes_DrawTrail(edTrails + i);
    for(int i = 0; i < edHeadN; i++)
        if(lane < 0 || edHeads[i].lane == lane)
            Notes_DrawHead(edHeads + i);
}

static void Grid_DrawNotes(float ppm, double top, double bot) {
    Notes_Collect(ppm, top, bot);

    /* Brilho do Note player: 1 so pra todas as notas na agulha (caixa que cobre todas),
       nao 1 por nota. 2 retangulos da textura branca, antes das setas: o lote delas segue unico. */
    float gx0 = 1e9f, gx1 = -1e9f, gy0 = 1e9f, gy1 = -1e9f;
    for(int i = 0; i < edHeadN; i++) {
        if(edHeads[i].look != LOOK_HIT)
            continue;
        gx0 = fminf(gx0, edHeads[i].cx);
        gx1 = fmaxf(gx1, edHeads[i].cx);
        gy0 = fminf(gy0, edHeads[i].y0);
        gy1 = fmaxf(gy1, edHeads[i].y0);
    }
    if(gx1 >= gx0) {
        DrawRectangleRec((Rectangle){gx0 - 34, gy0 - 34, gx1 - gx0 + 68, gy1 - gy0 + 68}, (Color){255, 255, 255, 40});
        DrawRectangleRec((Rectangle){gx0 - 26, gy0 - 26, gx1 - gx0 + 52, gy1 - gy0 + 52}, (Color){255, 255, 200, 60});
    }

    if(!edUseShader) {
        Notes_DrawPass(-1);   // atlas ou skin sem cor: 1 textura, 1 lote
        return;
    }
    for(int l = 0; l < 4; l++) {   // sem atlas: no maximo 4 trocas de shader, nunca por nota
        EdLane_Begin(l);
        Notes_DrawPass(l);
        EdLane_End();
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
            EdLane_Begin(raw);
            EdArrow_Draw(raw, cx, gy, 1.0f, (Color){255, 255, 255, 150});
            EdLane_End();
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
    Grid_DrawNotes(ppm, top, bot);
    Grid_DrawCursor(ppm);

    if(mouseOn) {
        Grid_BarMouse(m);
        Grid_NoteMouse(m, ppm, lane);
    }
}
