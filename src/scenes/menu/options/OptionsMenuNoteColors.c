/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * OptionsMenuNoteColors.c - tela de cor das notas (estilo Psych): por coluna, tres sliders de Matiz,
 * Saturacao e Brilho. Sao offsets sobre a skin (0 = original), guardados em gameOptions.laneHSB.
 * O preview e a nota real da skin, desenhada pelo mesmo shader da gameplay.
 */
#include "scenes/api/FontLoader.h"
#include "scenes/menu/options/OptionsMenuInternal.h"
#include "Game.h"
#include "GameData.h"
#include "logic/notes/Note.h"
#include "scenes/api/NoteSkinCache.h"
#include "scenes/gameplay/notes/PlayStateNoteSpawn.h"
#include "rendering/TextMeasure.h"
#include <math.h>
#include <stdio.h>

#define NC_PREV_CX 200.0f
#define NC_PREV_Y0 200.0f
#define NC_PREV_STEP 100.0f
#define NC_PREV_HALF 52.0f
#define NC_CARD_PREV (Rectangle) {40.0f, 150.0f, 320.0f, 500.0f}
#define NC_PRESET_BTN (Rectangle) {56.0f, 598.0f, 288.0f, 40.0f}
#define NC_PRESET_ARROW (Rectangle) {140.0f, 572.0f, 26.0f, 22.0f}  /* seta que abre/fecha a secao PRESET */
#define NC_CARD_SL (Rectangle) {500.0f, 150.0f, 740.0f, 420.0f}

#define NC_LANE_X 540.0f
#define NC_LANE_Y 80.0f
#define NC_LANE_STEP 170.0f
#define NC_LANE_W 150.0f
#define NC_LANE_H 46.0f

#define NC_SL_X 540.0f
#define NC_SL_W 470.0f
#define NC_SL_H 26.0f
#define NC_SL_Y0 236.0f
#define NC_SL_STEP 100.0f
#define NC_SL_COUNT 3

#define NC_BTN_Y 610.0f
#define NC_BTN_H 46.0f
#define NC_BTN_W 170.0f
#define NC_BTN_GAP 14.0f

#define NC_ROUND 0.12f

/* Roda de cor: angulo = matiz (HUE), distancia do centro = saturacao (SAT). */
#define NC_WHEEL_CX 1120.0f
#define NC_WHEEL_CY 330.0f
#define NC_WHEEL_R 110
#define NC_RAD2DEG 57.29577951308232f

bool noteColorScreen;

static int lane;      /* coluna sendo editada (0..3) */
static int slider;    /* slider com foco: 0 = matiz, 1 = saturacao, 2 = brilho */
static int drag;      /* 0 = nada; 1 + indice do slider sendo arrastado com o mouse */
static short clip[NC_SL_COUNT];
static bool hasClip;
static Texture2D wheelTex;
static bool wheelReady;
static bool wheelDrag;   /* arrastando dentro da roda */
static bool presetOpen = true; /* secao PRESET aberta (botao visivel) ou fechada */

static const char* const laneNames[4] = {"LEFT", "DOWN", "UP", "RIGHT"};
static const char* const sliderNames[NC_SL_COUNT] = {"HUE", "SATURATION", "BRIGHTNESS"};
static const short sliderMin[NC_SL_COUNT] = {-180, -100, -100};
static const short sliderMax[NC_SL_COUNT] = {180, 100, 100};
static TextMeasure laneNameMeasure[4]; /* nomes fixos: medidos uma vez */

/* Presets: valores (matiz, saturacao, brilho) por coluna LEFT, DOWN, UP, RIGHT. */
typedef struct {
    const char* name;
    short hsb[4][NC_SL_COUNT];
} NoteColorPreset;
static const NoteColorPreset presets[] = {
    {"Reimagined/Modern", {{9, 0, 40}, {11, 100, 35}, {40, 100, 28}, {9, 100, 29}}},
};

static Rectangle LaneButton(int i) { return (Rectangle) {NC_LANE_X + (float)i * NC_LANE_STEP, NC_LANE_Y, NC_LANE_W, NC_LANE_H}; }
static Rectangle SliderRect(int s) { return (Rectangle) {NC_SL_X, NC_SL_Y0 + (float)s * NC_SL_STEP, NC_SL_W, NC_SL_H}; }
static Rectangle SliderHit(int s) {
    const Rectangle r = SliderRect(s);
    return (Rectangle) {r.x - 10.0f, r.y - 18.0f, r.width + 20.0f, r.height + 36.0f};
}
static Rectangle CopyButton(void)  { return (Rectangle) {NC_SL_X, NC_BTN_Y, NC_BTN_W, NC_BTN_H}; }
static Rectangle PasteButton(void) { return (Rectangle) {NC_SL_X + NC_BTN_W + NC_BTN_GAP, NC_BTN_Y, NC_BTN_W, NC_BTN_H}; }
static Rectangle ResetButton(void) { return (Rectangle) {NC_SL_X + 2.0f * (NC_BTN_W + NC_BTN_GAP), NC_BTN_Y, NC_BTN_W + 60.0f, NC_BTN_H}; }

static short* Val(int s) { return &gameOptions.laneHSB[lane][s]; }

static void SetVal(int s, int v) {
    if (v < sliderMin[s]) v = sliderMin[s];
    if (v > sliderMax[s]) v = sliderMax[s];
    *Val(s) = (short)v;
}

static void ResetLane(void) {
    for (int s = 0; s < NC_SL_COUNT; s++) *Val(s) = 0;
}

/* Aplica o preset em todas as colunas (offsets HSB; sliders e previa leem daqui a cada frame). */
static void ApplyPreset(void) {
    const NoteColorPreset* p = &presets[0];
    for (int i = 0; i < 4; i++)
        for (int s = 0; s < NC_SL_COUNT; s++) gameOptions.laneHSB[i][s] = p->hsb[i][s];
    drag = 0;
    wheelDrag = false;
}

/* Mouse no slider: posicao na barra vira valor (arredondado para inteiro). */
static void DragSlider(int s, float mx) {
    const Rectangle r = SliderRect(s);
    float t = (mx - r.x) / r.width;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    SetVal(s, (int)lroundf(sliderMin[s] + t * (float)(sliderMax[s] - sliderMin[s])));
}

static void SelectLane(int i) {
    lane = i;
    drag = 0;
    wheelDrag = false;
}

/* Textura da roda: matiz no angulo, saturacao no raio (cheia na borda). Feita uma vez. */
static void BuildWheel(void) {
    const int size = NC_WHEEL_R * 2;
    Image img = GenImageColor(size, size, BLANK);
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            const float dx = (float)x + 0.5f - NC_WHEEL_R;
            const float dy = (float)y + 0.5f - NC_WHEEL_R;
            const float d = sqrtf(dx * dx + dy * dy);
            if (d > NC_WHEEL_R) continue;
            float hue = atan2f(dy, dx) * NC_RAD2DEG;
            if (hue < 0.0f) hue += 360.0f;
            ImageDrawPixel(&img, x, y, ColorFromHSV(hue, d / NC_WHEEL_R, 1.0f));
        }
    }
    wheelTex = LoadTextureFromImage(img);
    UnloadImage(img);
    wheelReady = true;
}

/* Mouse na roda: angulo grava HUE, distancia grava SAT (0..100). */
static void DragWheel(Vector2 m) {
    const float dx = m.x - NC_WHEEL_CX, dy = m.y - NC_WHEEL_CY;
    const float d = sqrtf(dx * dx + dy * dy);
    float sat = d / (float)NC_WHEEL_R * 100.0f;
    if (sat > 100.0f) sat = 100.0f;
    SetVal(0, (int)lroundf(atan2f(dy, dx) * NC_RAD2DEG));
    SetVal(1, (int)lroundf(sat));
    slider = 0;
}

void NoteColorScreen_Begin(void) {
    NoteColors_ReleaseFx();   /* recarrega a previa com a skin padrao */
    noteColorScreen = true;
    drag = 0;
    while (GetKeyPressed() != 0) {}   /* descarta o Enter que abriu a tela */
}

static void CloseScreen(void) {
    GameData_Flush();
    noteColorScreen = false;
}

void NoteColorScreen_Update(void) {
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) {
        CloseScreen();
        return;
    }

    // Teclado: W/S (ou setas cima/baixo) troca o slider; A/D (ou setas) ajusta; ALT = passo grande.
    const int step = IsKeyDown(KEY_LEFT_ALT) ? 10 : 1;
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) slider = (slider + NC_SL_COUNT - 1) % NC_SL_COUNT;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) slider = (slider + 1) % NC_SL_COUNT;
    if (IsKeyPressedRepeat(KEY_LEFT) || IsKeyPressedRepeat(KEY_A)) SetVal(slider, *Val(slider) - step);
    if (IsKeyPressedRepeat(KEY_RIGHT) || IsKeyPressedRepeat(KEY_D)) SetVal(slider, *Val(slider) + step);
    if (IsKeyPressed(KEY_P)) ApplyPreset();   /* mesmo efeito do botao PRESET */
    if (IsKeyPressed(KEY_Q)) SelectLane((lane + 3) % 4);
    if (IsKeyPressed(KEY_E)) SelectLane((lane + 1) % 4);
    if (IsKeyPressed(KEY_R)) {
        if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
            for (int i = 0; i < 4; i++)
                for (int s = 0; s < NC_SL_COUNT; s++) gameOptions.laneHSB[i][s] = 0;
        }
        else ResetLane();
    }

    // Mouse: arrastar nas barras; cliques nas colunas, previa, copiar/colar/resetar.
    const Vector2 m = RayGame_MouseWorld();
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        drag = 0;
        wheelDrag = false;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        for (int s = 0; s < NC_SL_COUNT; s++)
            if (CheckCollisionPointRec(m, SliderHit(s))) drag = 1 + s;
        if (CheckCollisionPointCircle(m, (Vector2) {NC_WHEEL_CX, NC_WHEEL_CY}, (float)NC_WHEEL_R)) wheelDrag = true;
        for (int i = 0; i < 4; i++) {
            if (CheckCollisionPointRec(m, LaneButton(i))) SelectLane(i);
            const Rectangle pr = {NC_PREV_CX - NC_PREV_HALF, NC_PREV_Y0 + (float)i * NC_PREV_STEP - NC_PREV_HALF,
                                  NC_PREV_HALF * 2.0f, NC_PREV_HALF * 2.0f};
            if (CheckCollisionPointRec(m, pr)) SelectLane(i);
        }
        if (CheckCollisionPointRec(m, CopyButton())) {
            for (int s = 0; s < NC_SL_COUNT; s++) clip[s] = *Val(s);
            hasClip = true;
        }
        if (CheckCollisionPointRec(m, PasteButton()) && hasClip)
            for (int s = 0; s < NC_SL_COUNT; s++) SetVal(s, clip[s]);
        if (CheckCollisionPointRec(m, ResetButton())) ResetLane();
        if (CheckCollisionPointRec(m, NC_PRESET_ARROW)) presetOpen = !presetOpen;
        if (presetOpen && CheckCollisionPointRec(m, NC_PRESET_BTN)) ApplyPreset();
    }

    if (drag > 0) {
        slider = drag - 1;
        DragSlider(slider, m.x);
    }
    if (wheelDrag) DragWheel(m);
}

/* Receptor + nota reais da skin (a nota ja vai com o ajuste da coluna), centrados em (cx, cy).
   Usado aqui e na tela de teclas Custom. */
static StrumNote laneStrum[4];
static RayAnimatedObject laneNote[4];
static bool laneFxReady;

static void EnsureLaneFx(void) {
    if (laneFxReady) return;
    Note_SetCurrentSkin(Cache_GetDefaultNoteSkinPtr());   /* so o arquivo da skin padrao, nao a da ultima musica */
    for (int i = 0; i < 4; i++) {
        StrumNote_Load(laneStrum + i, i);          /* tambem carrega as animacoes da skin */
        Render_DefaultAnimated(laneNote + i);
        laneNote[i].animationSet = *Note_GetCurrentSkin();
        AnimatedObject_SetAnimation(laneNote + i, Note_Animation(i));
    }
    laneFxReady = true;
}

void NoteColors_DrawLanePreview(int laneIdx, float cx, float cy, float scale) {
    EnsureLaneFx();
    StrumNote* s = laneStrum + laneIdx;
    RayAnimatedObject* n = laneNote + laneIdx;

    s->object.scaleX = s->object.scaleY = scale;
    n->scaleX = n->scaleY = scale;
    AnimatedObject_UpdateFrame(&s->object);
    AnimatedObject_UpdateFrame(n);

    const Vector2 ss = AnimatedObject_Sizes(&s->object);
    s->object.position = (Vector2) {cx - ss.x * 0.5f, cy - ss.y * 0.5f};
    const Vector2 ns = AnimatedObject_Sizes(n);
    n->position = (Vector2) {cx - ns.x * 0.5f, cy - ns.y * 0.5f};

    Render_DrawAnimatedObject(&s->object);
    PlayStateNoteSpawn_DrawShifted(n, GameData_LaneShift(laneIdx));
}

void NoteColors_DrawNoteIcon(int laneIdx, float cx, float cy, float scale) {
    EnsureLaneFx();
    RayAnimatedObject* n = laneNote + laneIdx;
    n->scaleX = n->scaleY = scale;
    AnimatedObject_UpdateFrame(n);
    const Vector2 ns = AnimatedObject_Sizes(n);
    n->position = (Vector2) {cx - ns.x * 0.5f, cy - ns.y * 0.5f};
    PlayStateNoteSpawn_DrawShifted(n, GameData_LaneShift(laneIdx));
}

void NoteColors_ReleaseFx(void) {
    PlayStateNoteSpawn_ReleaseTint();
    laneFxReady = false;
}

/* Barra de um slider: gradiente de fundo + marcador no valor atual + rotulo e valor. */
static void DrawSliderBar(int s) {
    const Rectangle r = SliderRect(s);
    const bool focus = slider == s;
    const int v = *Val(s);

    // Rotulo e valor acima da barra.
    char value[16];
    if (s == 0) (void)snprintf(value, sizeof(value), "%+d", v);
    else        (void)snprintf(value, sizeof(value), "%+d%%", v);
    DrawTextEx(mainFont, sliderNames[s], (Vector2) {r.x, r.y - 34.0f}, 26, 1.0f, focus ? YELLOW : GRAY);
    static TextMeasure valueMeasure[NC_SL_COUNT];   // um cache por slider: so muda quando o valor muda
    const Vector2 vs = TextMeasure_Get(&valueMeasure[s], mainFont, value, 26, 1.0f);
    DrawTextEx(mainFont, value, (Vector2) {r.x + r.width - vs.x, r.y - 34.0f}, 26, 1.0f, focus ? WHITE : LIGHTGRAY);

    // Fundo da barra: matiz = arco-iris; saturacao = cinza -> cor quente; brilho = preto -> branco.
    if (s == 0) {
        const float seg = r.width / 6.0f;
        const Color stops[7] = {
            {255, 0, 0, 255}, {255, 255, 0, 255}, {0, 255, 0, 255}, {0, 255, 255, 255},
            {0, 0, 255, 255}, {255, 0, 255, 255}, {255, 0, 0, 255}};
        for (int i = 0; i < 6; i++)
            DrawRectangleGradientH((int)(r.x + seg * (float)i), (int)r.y, (int)seg + 1, (int)r.height, stops[i], stops[i + 1]);
    }
    else if (s == 1) DrawRectangleGradientH((int)r.x, (int)r.y, (int)r.width, (int)r.height,
                                            (Color) {110, 110, 110, 255}, (Color) {255, 150, 70, 255});
    else             DrawRectangleGradientH((int)r.x, (int)r.y, (int)r.width, (int)r.height,
                                            (Color) {0, 0, 0, 255}, (Color) {255, 255, 255, 255});
    DrawRectangleLinesEx(r, focus ? 3.0f : 2.0f, focus ? YELLOW : (Color) {20, 20, 20, 255});

    // Marcador: posicao do valor dentro da faixa.
    const float t = (float)(v - sliderMin[s]) / (float)(sliderMax[s] - sliderMin[s]);
    const float kx = r.x + t * r.width;
    DrawRectangleRec((Rectangle) {kx - 3.0f, r.y - 6.0f, 6.0f, r.height + 12.0f}, BLACK);
    DrawRectangleRec((Rectangle) {kx - 2.0f, r.y - 4.0f, 4.0f, r.height + 8.0f}, WHITE);
}

static void DrawButton(Rectangle r, const char* text, bool enabled) {
    const Color fill = (Color) {46, 46, 52, 255};
    DrawRectangleRounded(r, NC_ROUND, 6, fill);
    DrawRectangleRoundedLinesEx(r, NC_ROUND, 6, 2.0f, enabled ? WHITE : GRAY);
    const Vector2 ts = MeasureTextEx(mainFont, text, 26, 1.0f);
    DrawTextEx(mainFont, text, (Vector2) {r.x + (r.width - ts.x) * 0.5f, r.y + (r.height - ts.y) * 0.5f},
               26, 1.0f, enabled ? WHITE : GRAY);
}

void NoteColorScreen_Draw(void) {
    DrawRectangle(0, 0, 1280, 720, (Color) {0, 0, 0, 225});
    DrawTextEx(mainFont, "NOTE COLORS", (Vector2) {80, 14}, 40, TEXT_SPACING, YELLOW);

    // Cartoes: previa a esquerda, controles a direita.
    DrawRectangleRounded(NC_CARD_PREV, NC_ROUND, 8, (Color) {22, 22, 28, 240});
    DrawRectangleRounded(NC_CARD_SL, NC_ROUND, 8, (Color) {22, 22, 28, 240});

    // Botoes das colunas: a coluna selecionada fica com borda branca.
    for (int i = 0; i < 4; i++) {
        const Rectangle r = LaneButton(i);
        const bool sel = i == lane;
        DrawRectangleRounded(r, NC_ROUND, 6, sel ? (Color) {70, 70, 80, 255} : (Color) {34, 34, 40, 255});
        DrawRectangleRoundedLinesEx(r, NC_ROUND, 6, sel ? 3.0f : 1.0f, sel ? WHITE : GRAY);
        const Vector2 ts = TextMeasure_Get(&laneNameMeasure[i], mainFont, laneNames[i], 26, 1.0f);
        DrawTextEx(mainFont, laneNames[i], (Vector2) {r.x + (r.width - ts.x) * 0.5f, r.y + (r.height - ts.y) * 0.5f},
                   26, 1.0f, sel ? WHITE : LIGHTGRAY);
    }

    // Previa: as 4 colunas com a skin real; a selecionada aparece maior.
    for (int i = 0; i < 4; i++)
        NoteColors_DrawLanePreview(i, NC_PREV_CX, NC_PREV_Y0 + (float)i * NC_PREV_STEP, i == lane ? 0.85f : 0.6f);

    // Presets: titulo com seta (abre/fecha) e botao embaixo das notas.
    DrawTextEx(mainFont, "PRESET", (Vector2) {NC_PRESET_BTN.x, NC_PRESET_BTN.y - 26.0f}, 22, 1.0f, GRAY);
    {
        const Rectangle a = NC_PRESET_ARROW;
        const float ax = a.x + a.width * 0.5f, ay = a.y + a.height * 0.5f;
        if (presetOpen) /* aberta: aponta para baixo */
            DrawTriangle((Vector2) {ax - 8.0f, ay - 5.0f}, (Vector2) {ax + 8.0f, ay - 5.0f}, (Vector2) {ax, ay + 6.0f}, WHITE);
        else            /* fechada: aponta para a direita */
            DrawTriangle((Vector2) {ax - 5.0f, ay - 8.0f}, (Vector2) {ax - 5.0f, ay + 8.0f}, (Vector2) {ax + 6.0f, ay}, WHITE);
    }
    if (presetOpen) {
        DrawRectangleRounded(NC_PRESET_BTN, NC_ROUND, 6, (Color) {46, 46, 52, 255});
        DrawRectangleRoundedLinesEx(NC_PRESET_BTN, NC_ROUND, 6, 2.0f, WHITE);
        const char* pname = presets[0].name;
        static TextMeasure presetMeasure;
        const Vector2 ps = TextMeasure_Get(&presetMeasure, mainFont, pname, 22, 1.0f);
        DrawTextEx(mainFont, pname, (Vector2) {NC_PRESET_BTN.x + (NC_PRESET_BTN.width - ps.x) * 0.5f,
                   NC_PRESET_BTN.y + (NC_PRESET_BTN.height - ps.y) * 0.5f}, 22, 1.0f, WHITE);
    }

    // Estado da coluna: original ou ajustada.
    const bool shifted = GameData_LaneShifted(lane);
    DrawTextEx(mainFont, shifted ? "Ajustada" : "Original", (Vector2) {NC_SL_X, NC_LANE_Y + NC_LANE_H + 14.0f},
               26, 1.0f, shifted ? YELLOW : GRAY);

    // Os tres sliders.
    for (int s = 0; s < NC_SL_COUNT; s++) DrawSliderBar(s);

    // Roda de cor: o marcador fica na posicao (matiz, saturacao) atual da coluna.
    if (!wheelReady) BuildWheel();
    const float wx = NC_WHEEL_CX - NC_WHEEL_R, wy = NC_WHEEL_CY - NC_WHEEL_R;
    DrawTexture(wheelTex, (int)wx, (int)wy, WHITE);
    DrawCircleLines((int)NC_WHEEL_CX, (int)NC_WHEEL_CY, (float)NC_WHEEL_R, (Color) {20, 20, 20, 255});
    const float t = (float)*Val(1) / 100.0f;
    const float ang = (float)*Val(0) / NC_RAD2DEG;
    const float mx = NC_WHEEL_CX + (float)NC_WHEEL_R * t * cosf(ang);
    const float my = NC_WHEEL_CY + (float)NC_WHEEL_R * t * sinf(ang);
    DrawCircleV((Vector2) {mx, my}, 9.0f, BLACK);
    DrawCircleV((Vector2) {mx, my}, 6.0f, WHITE);
    DrawTextEx(mainFont, "COLOR", (Vector2) {NC_WHEEL_CX - 36.0f, NC_WHEEL_CY - NC_WHEEL_R - 34.0f}, 26, 1.0f, WHITE);

    // Copiar / colar / resetar a coluna.
    DrawButton(CopyButton(), "COPY", true);
    DrawButton(PasteButton(), "PASTE", hasClip);
    DrawButton(ResetButton(), "RESET LANE", true);

    DrawTextEx(mainFont, "W/S: select   A/D: adjust   ALT: x10   Q/E: lane   R: reset lane   SHIFT+R: reset all",
               (Vector2) {NC_SL_X, 676}, 20, 1, GRAY);
    DrawTextEx(mainFont, "ESC/BACKSPACE: back   P: preset", (Vector2) {NC_SL_X, 696}, 20, 1, GRAY);
}

void NoteColorScreen_Destroy(void) {
    NoteColors_ReleaseFx();
    if (wheelReady) UnloadTexture(wheelTex);
    wheelReady = false;
}
