/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: a grade (notas, linhas de tempo, cursor, mouse) e seu layout. */
#include"scenes/AllScenes.h"
#include"scenes/Song.h"
#include"scenes/SongList.h"
#include "scenes/ModPath.h"
#include "scenes/Character.h"
#include"scenes/Stage.h"
#include"scenes/Note.h"
#include "Game.h"
#include "GameData.h"
#include "raymath.h"
#include "EditorGrid.h"
#include "EditorDataAndIO.h"
#include "EditorTimeline.h"
#include "EditorAudio.h"
#include "EditorWidgets.h"

/* // * Código do Chart Editor */
// Chart Editor v1: palavras simples, sem jargao.
// PICKER (escolher musica / abrir json) -> EDIT (grade) -> F5 testa no jogo.
// ^ 1. Chart

static Color EdLaneColor(int lane);

Texture2D edIconO;

Texture2D edIconP;

char edIconOOk = 0;

char edIconPOk = 0;

// setas do proprio jogo (NOTE_assets) pras cabecas das notas
static RayAnimatedObject edArrows[4];

static Vector2 edArrowAdj[4];

static char edArrowsReady = 0;

void EdArrows_Init(void) {
    if(edArrowsReady)
        return;
    const RayAnimationHandler* nah = Note_GetCurrentSkin();
    // sprites claros das notas (iguais aos que descem no jogo), sem tinta
    const char* names[4] = {"purple", "blue", "green", "red"};
    for(int i = 0; i < 4; i++) {
        Render_DefaultAnimated(edArrows + i);
        edArrows[i].animationSet = *nah;
        int ai = AnimationSet_FindAnimation(nah, names[i]);
        if(ai >= 0) {
            AnimatedObject_SetAnimation(edArrows + i, ai);
            Frame* fr = nah->animations[ai].frames;
            float big = (float)((fr->w > fr->h) ? fr->w : fr->h);
            float s = (big > 1) ? 56.0f / big : 1.0f;
            edArrows[i].scaleX = edArrows[i].scaleY = s;
            edArrows[i].color = WHITE;
            RayAnimation* an = nah->animations + ai;
            edArrowAdj[i].x = (an->animationOffset.x + fr->fx) * s - fr->w * s / 2;
            edArrowAdj[i].y = (an->animationOffset.y + fr->fy) * s - fr->h * s / 2;
        }
    }
    edArrowsReady = 1;
}

float EdPxPerMs(void) {
    return (GRID_Y1 - GRID_Y0) / (float)VIEW_MS;
}

static Color EdLaneColor(int lane) {
    switch(lane % 4) {
        case 0: return (Color) {200, 80, 255, 255};
        case 1: return (Color) {80, 180, 255, 255};
        case 2: return (Color) {80, 255, 150, 255};
        default: return (Color) {255, 90, 90, 255};
    }
}

// barra da musica no topo (arrasta pra andar; substitui os botoes de trecho)
#define SBAR_X GRID_X0

#define SBAR_Y 36.0f

#define SBAR_W (LANE_W * 8)

#define SBAR_H 16.0f

static char barDrag = 0;

// ---------- desenho da grade (cada parte numa funcao) ----------

// barra da musica (preta e branca, arrasta pra andar)
static void Grid_DrawSongBar(void) {
    double end = SongEndMs();
    if(end < 1000) end = 1000;
    float frac = Clamp((float)(cursorMs / end), 0.0f, 1.0f);
    float kx = SBAR_X + frac * SBAR_W;
    DrawRectangle((int)SBAR_X, (int)SBAR_Y, (int)SBAR_W, (int)SBAR_H, BLACK);
    DrawRectangle((int)SBAR_X, (int)SBAR_Y, (int)(kx - SBAR_X), (int)SBAR_H, WHITE);
    DrawRectangle((int)(kx - 6), (int)SBAR_Y - 2, 12, (int)SBAR_H + 4, RED);
}

// fundo, casas, lado do jogador e icones dos cantores
static void Grid_DrawBackground(void) {
    const int gridW = (int)(LANE_W * 8);
    const int gridH = (int)(GRID_Y1 - GRID_Y0);

    DrawRectangle((int)GRID_X0 - 10, (int)GRID_Y0 - 34, gridW + 20, gridH + 44, (Color) {12, 12, 12, 255});

    // casas brancas (da pra ver onde clica)
    for(int l = 0; l < 8; l++) {
        int x = (int)(GRID_X0 + l * LANE_W);
        DrawRectangle(x + 1, (int)GRID_Y0, (int)LANE_W - 2, gridH, (Color) {250, 250, 250, 255});
    }
    // divisoria entre os dois lados
    DrawRectangle((int)(GRID_X0 + 4 * LANE_W) - 1, (int)GRID_Y0 - 30, 2, gridH + 34, (Color) {60, 60, 60, 255});
    // seu lado (direita) com verde claro
    DrawRectangle((int)(GRID_X0 + LANE_W * 4), (int)GRID_Y0, (int)(LANE_W * 4), gridH, (Color) {140, 230, 170, 70});

    // icones dos cantores em cima (igual a barra de vida do jogo)
    if(edIconOOk)
        DrawTexturePro(edIconO, Character_IconFrame(edIconO),
            (Rectangle) {GRID_X0 + LANE_W * 2 - 22, GRID_Y0 - 52, 44, 44}, (Vector2) {0, 0}, 0, WHITE);
    if(edIconPOk)
        DrawTexturePro(edIconP, Character_IconFrame(edIconP),
            (Rectangle) {GRID_X0 + LANE_W * 6 - 22, GRID_Y0 - 52, 44, 44}, (Vector2) {0, 0}, 0, WHITE);
}

// linhas de tempo por trecho (so os trechos e passos que aparecem na tela)
static void Grid_DrawTimeLines(float ppm, double viewBottom) {
    const int x0 = (int)GRID_X0;
    const int x1 = (int)(GRID_X0 + LANE_W * 8);
    int first, last;
    SecRangeInView(viewTopMs, viewBottom, &first, &last);

    for(int i = first; i <= last; i++) {
        double start = SecStartMs(i);
        double end = (i + 1 < chart.nsec) ? SecStartMs(i + 1) : SongEndMs() + 4000;
        if(end < viewTopMs || start > viewBottom)
            continue;
        if(i == secIdx) {
            double a = fmax(start, viewTopMs);
            double b = fmin(end, viewBottom);
            DrawRectangle(x0, (int)(GRID_Y0 + (a - viewTopMs) * ppm),
                (int)(LANE_W * 8), (int)((b - a) * ppm), (Color) {255, 255, 255, 12});
        }
        double beatMs = 60000.0 / chart.secs[i].bpm;
        double stepMs = beatMs / snapDiv;
        if(stepMs > 0) {
            // comeca no primeiro passo visivel (nao no comeco do trecho)
            long n = (viewTopMs > start) ? (long)floor((viewTopMs - start) / stepMs) : 0;
            for(;; n++) {
                double t = start + n * stepMs;
                if(t > end + 0.5 || t > viewBottom)
                    break;
                if(t < viewTopMs)
                    continue;
                int y = (int)(GRID_Y0 + (t - viewTopMs) * ppm);
                char isBeat = (fabs(fmod(t - start, beatMs)) < stepMs / 2);
                DrawLine(x0, y, x1, y, isBeat ? (Color) {90, 90, 90, 160} : (Color) {160, 160, 160, 130});
            }
        }
        // comeco do trecho (so a linha, sem texto em cima do chart)
        if(start >= viewTopMs && start <= viewBottom) {
            int y = (int)(GRID_Y0 + (start - viewTopMs) * ppm);
            DrawLine(x0 - 8, y, x1 + 8, y, (Color) {210, 130, 0, 255});
        }
    }
}

// uma nota (casa virada pra tela; cor/seta pela casa real do arquivo)
// Note player ON: passado apaga, na agulha acende (so visual, sem tocar no chart)
static void Grid_DrawNote(const EdSec* s, const EdNote* n, float y, float ppm) {
    float x = GRID_X0 + EdDispLane(s, n->lane) * LANE_W;
    char isPast = 0;
    char isHit = 0;
    if(playFollow) {
        float dt = n->time - (float)cursorMs;
        if(playing && fabsf(dt) <= 200.0f)
            isHit = 1;
        else if(dt < 0)
            isPast = 1;
    }

    // nota longa (rastro)
    if(n->len > 0) {
        float h = fmaxf(n->len * ppm, 6.0f);
        float top = fmaxf(y, GRID_Y0);
        float bot = fminf(y + h, GRID_Y1);
        if(bot > top) {
            Color bc = EdLaneColor(n->lane);
            bc.a = isPast ? 50 : (isHit ? 255 : 160);
            DrawRectangle((int)(x + LANE_W / 2 - 7), (int)top, 14, (int)(bot - top), bc);
        }
    }

    // seta do jogo (bem mais visivel que quadrado)
    int d = n->lane % 4;
    float cx = x + LANE_W / 2;
    if(isHit) {
        DrawCircle((int)cx, (int)(y + 1), 34.0f, (Color) {255, 255, 255, 60});
        DrawCircle((int)cx, (int)(y + 1), 26.0f, (Color) {255, 255, 200, 90});
    }
    Color keepC = edArrows[d].color;
    float keepSx = edArrows[d].scaleX;
    float keepSy = edArrows[d].scaleY;
    if(isPast)
        edArrows[d].color = (Color) {255, 255, 255, 90};
    else if(isHit) {
        edArrows[d].color = WHITE;
        edArrows[d].scaleX = keepSx * 1.25f;
        edArrows[d].scaleY = keepSy * 1.25f;
    }
    edArrows[d].position.x = cx + edArrowAdj[d].x;
    edArrows[d].position.y = y + 1 + edArrowAdj[d].y;
    Render_DrawAnimatedObject(edArrows + d);
    edArrows[d].color = keepC;
    edArrows[d].scaleX = keepSx;
    edArrows[d].scaleY = keepSy;
}

// notas: pula trechos cujas notas estao todas fora da tela (limites tmin/tmax do trecho;
// nao depende de a nota estar "dentro" do tempo nominal do trecho, entao charts bagunçados carregam)
static void Grid_DrawNotes(float ppm, double viewBottom) {
    double top = viewTopMs - 30.0 / ppm, bot = viewBottom + 30.0 / ppm;
    for(int i = 0; i < chart.nsec; i++) {
        const EdSec* s = chart.secs + i;
        if(s->count <= 0 || s->tmax < top || s->tmin > bot)
            continue;
        for(int k = 0; k < s->count; k++) {
            const EdNote* n = s->notes + k;
            float y = GRID_Y0 + (float)(n->time - viewTopMs) * ppm;
            if(y < GRID_Y0 - 30 || y > GRID_Y1 + 30)
                continue;
            Grid_DrawNote(s, n, y, ppm);
        }
    }
}

// cursor (play grosso + agulha de vinil; as notas sobem ate ele, igual ao jogo)
static void Grid_DrawCursor(float ppm) {
    float y = GRID_Y0 + (float)(cursorMs - viewTopMs) * ppm;
    if(y < GRID_Y0 - 30 || y > GRID_Y1 + 30)
        return;
    int x0 = (int)GRID_X0 - 8;
    int x1 = (int)(GRID_X0 + LANE_W * 8) + 8;
    int yi = (int)y;
    // linha grossa: sombra preta + vermelho + fio branco no meio
    DrawRectangle(x0 - 2, yi - 4, (x1 - x0) + 4, 8, BLACK);
    DrawRectangle(x0, yi - 3, x1 - x0, 6, RED);
    DrawRectangle(x0, yi - 1, x1 - x0, 2, WHITE);
    // agulha: triangulo apontando pro play (lado esquerdo, estilo vinil)
    DrawTriangle((Vector2) {(float)x0 - 22, (float)yi - 13},
                 (Vector2) {(float)x0 - 22, (float)yi + 13},
                 (Vector2) {(float)x0 + 2, (float)yi}, WHITE);
    DrawTriangle((Vector2) {(float)x0 - 19, (float)yi - 10},
                 (Vector2) {(float)x0 - 19, (float)yi + 10},
                 (Vector2) {(float)x0 - 1, (float)yi}, RED);
}

// barra da musica: clica e arrasta pra andar na musica
static void Grid_BarMouse(Vector2 m) {
    double end = SongEndMs();
    if(end < 1000) end = 1000;
    Rectangle bar = {SBAR_X, SBAR_Y, SBAR_W, SBAR_H};
    char inside = (m.x >= bar.x && m.x <= bar.x + bar.width && m.y >= bar.y - 8 && m.y <= bar.y + bar.height + 8);
    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && inside)
        barDrag = 1;
    if(!barDrag)
        return;
    if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        double frac = Clamp((float)((m.x - bar.x) / bar.width), 0.0f, 1.0f);
        cursorMs = frac * end;
        viewTopMs = cursorMs - VIEW_MS * 0.5;
        if(viewTopMs < -500) viewTopMs = -500;
    } else {
        if(playing)
            Audio_SeekAll(cursorMs);
        barDrag = 0;
    }
}

// mouse na grade: fantasma onde a nota vai cair (X vermelho = ja tem).
// Esquerdo SO poe, direito SO apaga.
static void Grid_NoteMouse(Vector2 m, float ppm) {
    if(m.x < GRID_X0 || m.x > GRID_X0 + LANE_W * 8 || m.y < GRID_Y0 || m.y > GRID_Y1)
        return;
    int lane = (int)((m.x - GRID_X0) / LANE_W);
    if(lane < 0 || lane >= 8)
        return;

    double t = viewTopMs + (m.y - GRID_Y0) / ppm;
    int si = SecAtTime(fmax(t, 0));
    EdSec* s = chart.secs + si;
    int raw = EdDispLane(s, lane); // casa da tela -> casa do arquivo
    float beatMs = 60000.0f / s->bpm;
    float stepMs = beatMs / snapDiv;
    float snapped = fmaxf(roundf((float)(t / stepMs)) * stepMs, 0.0f);
    float gy = GRID_Y0 + (float)(snapped - viewTopMs) * ppm;
    float cx = GRID_X0 + lane * LANE_W + LANE_W / 2;
    int exist = EdSec_FindNear(s, snapped, raw, fmaxf(stepMs / 2, 15.0f));

    if(gy >= GRID_Y0 - 26 && gy <= GRID_Y1 + 26) {
        if(exist >= 0) {
            DrawLine((int)(cx - 14), (int)(gy - 14), (int)(cx + 14), (int)(gy + 14), RED);
            DrawLine((int)(cx - 14), (int)(gy + 14), (int)(cx + 14), (int)(gy - 14), RED);
        } else {
            int d = raw % 4;
            Color keep = edArrows[d].color;
            edArrows[d].color = (Color) {255, 255, 255, 150};
            edArrows[d].position.x = cx + edArrowAdj[d].x;
            edArrows[d].position.y = gy + 1 + edArrowAdj[d].y;
            Render_DrawAnimatedObject(edArrows + d);
            edArrows[d].color = keep;
        }
    }

    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && exist < 0) {
        EdTouch();
        EdSec_Add(s, snapped, raw, 0);
        secIdx = si;
        cursorMs = snapped;
    }
    if(IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        int idx = EdSec_FindNear(s, (float)t, raw, beatMs);
        if(idx >= 0 && EdSec_RemoveAt(s, idx)) {
            EdTouch();
            secIdx = si;
        }
    }
}

void Edit_DrawGrid(void) {
    float ppm = EdPxPerMs();
    double viewBottom = viewTopMs + VIEW_MS;

    Grid_DrawSongBar();
    Grid_DrawBackground();
    Grid_DrawTimeLines(ppm, viewBottom);
    Grid_DrawNotes(ppm, viewBottom);
    Grid_DrawCursor(ppm);

    // mouse (travado com janela/drop abertos)
    if(ceDlg == DLG_NONE && !activeDrop) {
        Vector2 m = EdMouseWorld();
        Grid_BarMouse(m);
        Grid_NoteMouse(m, ppm);
    }
}
