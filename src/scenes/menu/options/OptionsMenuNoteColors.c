/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * OptionsMenuNoteColors.c - tela de cor das notas: roda de matiz/saturacao, barra de brilho, paleta,
 * RGB/HEX e copiar/colar. Cada coluna (esq, baixo, cima, dir) guarda um RGB em gameOptions.laneTint.
 */
#include "scenes/api/FontLoader.h"
#include "scenes/menu/options/OptionsMenuInternal.h"
#include "Game.h"
#include "GameData.h"
#include "logic/notes/Note.h"
#include "scenes/api/NoteSkinCache.h"
#include "scenes/gameplay/PlayStateNoteSpawn.h"
#include "rendering/TextMeasure.h"
#include <math.h>
#include <stdio.h>

#define NC_WHEEL_CX 880.0f
#define NC_WHEEL_CY 300.0f
#define NC_WHEEL_R 150
#define NC_BAR_X 1080.0f
#define NC_BAR_Y 150.0f
#define NC_BAR_W 44.0f
#define NC_BAR_H 300.0f
#define NC_SW_COLS 12
#define NC_SW_ROWS 3
#define NC_SW_W 34.0f
#define NC_SW_H 28.0f
#define NC_SW_STEP 40.0f
#define NC_SW_ROW_STEP 34.0f
#define NC_SW_X 80.0f
#define NC_SW_Y 540.0f
#define NC_DEG2RAD 0.01745329252f
// Coluna de preview (receptor + nota reais da skin), lado esquerdo, de cima para baixo: LEFT, DOWN, UP, RIGHT.
#define NC_PREV_CX 200.0f
#define NC_PREV_Y0 150.0f
#define NC_PREV_STEP 100.0f
#define NC_PREV_HALF 52.0f

bool noteColorScreen;

static int lane;                 // coluna sendo editada (0..3)
static float hue, sat, val;      // HSV da cor atual (roda + barra)
static int drag;                 // 0 = nada, 1 = roda, 2 = barra
static Texture2D wheelTex;
static bool wheelReady;
static Color clipboard = {255, 255, 255, 255};
static bool hasClip;
static Color swatches[NC_SW_ROWS * NC_SW_COLS];
static bool swatchesReady;

static const char* const laneNames[4] = {"LEFT", "DOWN", "UP", "RIGHT"};
static TextMeasure laneNameMeasure[4]; // nomes fixos: medidos uma vez

/* Botoes das colunas em cima da roda (canto superior direito). */
static Rectangle LaneButton(int i) { return (Rectangle) {700.0f + (float)i * 108.0f, 72.0f, 96.0f, 44.0f}; }
static Rectangle CopyButton(void) { return (Rectangle) {340.0f, 300.0f, 110.0f, 44.0f}; }
static Rectangle PasteButton(void) { return (Rectangle) {462.0f, 300.0f, 110.0f, 44.0f}; }
static Rectangle ResetButton(void) { return (Rectangle) {584.0f, 300.0f, 110.0f, 44.0f}; }
static Rectangle BarRect(void) { return (Rectangle) {NC_BAR_X, NC_BAR_Y, NC_BAR_W, NC_BAR_H}; }

static Rectangle SwatchRect(int r, int c) {
    return (Rectangle) {NC_SW_X + (float)c * NC_SW_STEP, NC_SW_Y + (float)r * NC_SW_ROW_STEP, NC_SW_W, NC_SW_H};
    /* (paleta compacta: ver NC_SW_* acima) */
}

static void BuildWheel(void) {
    const int size = NC_WHEEL_R * 2;
    Image img = GenImageColor(size, size, BLANK);
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            const float dx = (float)x + 0.5f - NC_WHEEL_R;
            const float dy = (float)y + 0.5f - NC_WHEEL_R;
            const float d = sqrtf(dx * dx + dy * dy);
            if (d > NC_WHEEL_R) continue;
            float h = atan2f(dy, dx) / NC_DEG2RAD;
            if (h < 0.0f) h += 360.0f;
            ImageDrawPixel(&img, x, y, ColorFromHSV(h, d / NC_WHEEL_R, 1.0f));
        }
    }
    wheelTex = LoadTextureFromImage(img);
    UnloadImage(img);
    wheelReady = true;
}

static void BuildSwatches(void) {
    for (int c = 0; c < NC_SW_COLS; c++) {
        const float h = (float)c * 360.0f / NC_SW_COLS;
        swatches[0 * NC_SW_COLS + c] = ColorFromHSV(h, 1.0f, 1.0f);
        swatches[1 * NC_SW_COLS + c] = ColorFromHSV(h, 0.5f, 1.0f);
        swatches[2 * NC_SW_COLS + c] = ColorFromHSV(0.0f, 0.0f, 1.0f - (float)c / (NC_SW_COLS - 1));
    }
    swatchesReady = true;
}

/* Recarrega o HSV a partir do RGB guardado da coluna. */
static void LoadLaneHSV(void) {
    const Vector3 h = ColorToHSV(GameData_LaneTint(lane));
    hue = h.x;
    sat = h.y;
    val = h.z;
}

/* Grava o HSV atual como RGB da coluna. */
static void ApplyHSV(void) {
    const Color c = ColorFromHSV(hue, sat, val);
    gameOptions.laneTint[lane][0] = c.r;
    gameOptions.laneTint[lane][1] = c.g;
    gameOptions.laneTint[lane][2] = c.b;
}

static void SetLaneRGB(Color c) {
    gameOptions.laneTint[lane][0] = c.r;
    gameOptions.laneTint[lane][1] = c.g;
    gameOptions.laneTint[lane][2] = c.b;
    LoadLaneHSV();
}

static void SelectLane(int i) {
    lane = i;
    drag = 0;
    LoadLaneHSV();
}

void NoteColorScreen_Begin(void) {
    NoteColors_ReleaseFx();   /* recarrega a previa com a skin padrao */
    if (!wheelReady) BuildWheel();
    if (!swatchesReady) BuildSwatches();
    noteColorScreen = true;
    drag = 0;
    LoadLaneHSV();
    while (GetKeyPressed() != 0) {}   // descarta o Enter que abriu a tela
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
    if (IsKeyPressed(KEY_LEFT)) SelectLane((lane + 3) % 4);
    if (IsKeyPressed(KEY_RIGHT)) SelectLane((lane + 1) % 4);
    if (IsKeyPressed(KEY_R)) {
        if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
            for (int i = 0; i < 4; i++) {
                gameOptions.laneTint[i][0] = gameOptions.laneTint[i][1] = gameOptions.laneTint[i][2] = 255;
            }
            LoadLaneHSV();
        }
        else SetLaneRGB((Color) {255, 255, 255, 255});
    }

    const Vector2 m = RayGame_MouseWorld();
    const Vector2 wheelC = {NC_WHEEL_CX, NC_WHEEL_CY};

    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) drag = 0;

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (CheckCollisionPointCircle(m, wheelC, (float)NC_WHEEL_R)) drag = 1;
        else if (CheckCollisionPointRec(m, BarRect())) drag = 2;
        else {
            for (int i = 0; i < 4; i++)
                if (CheckCollisionPointRec(m, LaneButton(i))) SelectLane(i);
            for (int i = 0; i < 4; i++) {
                const Rectangle pr = {NC_PREV_CX - NC_PREV_HALF, NC_PREV_Y0 + (float)i * NC_PREV_STEP - NC_PREV_HALF,
                                      NC_PREV_HALF * 2.0f, NC_PREV_HALF * 2.0f};
                if (CheckCollisionPointRec(m, pr)) SelectLane(i);
            }
            for (int r = 0; r < NC_SW_ROWS; r++)
                for (int c = 0; c < NC_SW_COLS; c++)
                    if (CheckCollisionPointRec(m, SwatchRect(r, c))) {
                        SetLaneRGB(swatches[r * NC_SW_COLS + c]);
                    }
            if (CheckCollisionPointRec(m, CopyButton())) {
                clipboard = GameData_LaneTint(lane);
                hasClip = true;
            }
            if (CheckCollisionPointRec(m, PasteButton()) && hasClip) SetLaneRGB(clipboard);
            if (CheckCollisionPointRec(m, ResetButton())) SetLaneRGB((Color) {255, 255, 255, 255});
        }
    }

    if (drag == 1) {
        const float dx = m.x - NC_WHEEL_CX, dy = m.y - NC_WHEEL_CY;
        float d = sqrtf(dx * dx + dy * dy) / NC_WHEEL_R;
        if (d > 1.0f) d = 1.0f;
        float h = atan2f(dy, dx) / NC_DEG2RAD;
        if (h < 0.0f) h += 360.0f;
        hue = h;
        sat = d;
        ApplyHSV();
    }
    else if (drag == 2) {
        float v = 1.0f - (m.y - NC_BAR_Y) / NC_BAR_H;
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        val = v;
        ApplyHSV();
    }
}

/* Receptor + nota reais da skin (a nota ja vai com a cor da coluna), centrados em (cx, cy).
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

void NoteColors_DrawLanePreview(int lane, float cx, float cy, float scale) {
    EnsureLaneFx();
    StrumNote* s = laneStrum + lane;
    RayAnimatedObject* n = laneNote + lane;

    s->object.scaleX = s->object.scaleY = scale;
    n->scaleX = n->scaleY = scale;
    AnimatedObject_UpdateFrame(&s->object);
    AnimatedObject_UpdateFrame(n);

    const Vector2 ss = AnimatedObject_Sizes(&s->object);
    s->object.position = (Vector2) {cx - ss.x * 0.5f, cy - ss.y * 0.5f};
    const Vector2 ns = AnimatedObject_Sizes(n);
    n->position = (Vector2) {cx - ns.x * 0.5f, cy - ns.y * 0.5f};

    Render_DrawAnimatedObject(&s->object);
    PlayStateNoteSpawn_DrawTinted(n, GameData_LaneTint(lane));
}

void NoteColors_DrawNoteIcon(int lane, float cx, float cy, float scale) {
    EnsureLaneFx();
    RayAnimatedObject* n = laneNote + lane;
    n->scaleX = n->scaleY = scale;
    AnimatedObject_UpdateFrame(n);
    const Vector2 ns = AnimatedObject_Sizes(n);
    n->position = (Vector2) {cx - ns.x * 0.5f, cy - ns.y * 0.5f};
    PlayStateNoteSpawn_DrawTinted(n, GameData_LaneTint(lane));
}

void NoteColors_ReleaseFx(void) {
    PlayStateNoteSpawn_ReleaseTint();
    laneFxReady = false;
}

void NoteColorScreen_Draw(void) {
    DrawRectangle(0, 0, 1280, 720, (Color) {0, 0, 0, 225});
    DrawTextEx(mainFont, "NOTE COLORS", (Vector2) {80, 14}, 40, TEXT_SPACING, YELLOW);

    const Color cur = ColorFromHSV(hue, sat, val);
    const Color tint = GameData_LaneTint(lane);

    // Botoes das colunas (cor da coluna no fundo).
    for (int i = 0; i < 4; i++) {
        const Rectangle r = LaneButton(i);
        const Color c = GameData_LaneTint(i);
        DrawRectangleRec(r, (Color) {c.r / 3, c.g / 3, c.b / 3, 255});
        DrawRectangleLinesEx(r, i == lane ? 4.0f : 2.0f, i == lane ? WHITE : GRAY);
        const Vector2 ts = TextMeasure_Get(&laneNameMeasure[i], mainFont, laneNames[i], 26, 1.0f);
        DrawTextEx(mainFont, laneNames[i], (Vector2) {r.x + (r.width - ts.x) * 0.5f, r.y + (r.height - ts.y) * 0.5f},
                   26, 1.0f, WHITE);
    }

    // Pre-visualizacao: as 4 colunas com a skin real; a selecionada aparece maior.
    for (int i = 0; i < 4; i++)
        NoteColors_DrawLanePreview(i, NC_PREV_CX, NC_PREV_Y0 + (float)i * NC_PREV_STEP, i == lane ? 0.85f : 0.6f);

    // Leitura HEX e RGB.
    char line[64];
    if (tint.r == 255 && tint.g == 255 && tint.b == 255) {
        DrawTextEx(mainFont, "Standart (padrão)", (Vector2) {340.0f, 176.0f}, 44, 2, WHITE);
    } else {
        (void)snprintf(line, sizeof(line), "%02X%02X%02X", tint.r, tint.g, tint.b);
        DrawTextEx(mainFont, line, (Vector2) {340.0f, 150.0f}, 64, TEXT_SPACING, tint);
    }
    (void)snprintf(line, sizeof(line), "R %3d   G %3d   B %3d", tint.r, tint.g, tint.b);
    DrawTextEx(mainFont, line, (Vector2) {340.0f, 240.0f}, 26, 2, WHITE);

    // Copiar / colar / resetar.
    const Rectangle bc = CopyButton(), bp = PasteButton(), br = ResetButton();
    DrawRectangleRec(bc, (Color) {60, 60, 60, 255});
    DrawRectangleLinesEx(bc, 2.0f, WHITE);
    DrawTextEx(mainFont, "COPY", (Vector2) {bc.x + 24.0f, bc.y + 6.0f}, 28, 2, WHITE);
    DrawRectangleRec(bp, (Color) {60, 60, 60, 255});
    DrawRectangleLinesEx(bp, 2.0f, hasClip ? WHITE : GRAY);
    DrawTextEx(mainFont, "PASTE", (Vector2) {bp.x + 18.0f, bp.y + 6.0f}, 28, 2, hasClip ? WHITE : GRAY);
    DrawRectangleRec(br, (Color) {60, 60, 60, 255});
    DrawRectangleLinesEx(br, 2.0f, WHITE);
    DrawTextEx(mainFont, "RESET", (Vector2) {br.x + 18.0f, br.y + 6.0f}, 28, 2, WHITE);

    // Roda de matiz (eixo angular) e saturacao (raio).
    if (wheelReady) DrawTexture(wheelTex, (int)(NC_WHEEL_CX - NC_WHEEL_R), (int)(NC_WHEEL_CY - NC_WHEEL_R), WHITE);
    const float mx = NC_WHEEL_CX + (float)NC_WHEEL_R * sat * cosf(hue * NC_DEG2RAD);
    const float my = NC_WHEEL_CY + (float)NC_WHEEL_R * sat * sinf(hue * NC_DEG2RAD);
    DrawCircleV((Vector2) {mx, my}, 11.0f, BLACK);
    DrawCircleV((Vector2) {mx, my}, 8.0f, cur);

    // Barra de brilho (valor).
    const Rectangle bar = BarRect();
    DrawRectangleGradientV((int)bar.x, (int)bar.y, (int)bar.width, (int)bar.height,
                           ColorFromHSV(hue, sat, 1.0f), BLACK);
    DrawRectangleLinesEx(bar, 2.0f, WHITE);
    const float ky = bar.y + (1.0f - val) * bar.height;
    DrawRectangleLinesEx((Rectangle) {bar.x - 6.0f, ky - 5.0f, bar.width + 12.0f, 10.0f}, 2.0f, BLACK);
    DrawRectangleLinesEx((Rectangle) {bar.x - 4.0f, ky - 3.0f, bar.width + 8.0f, 6.0f}, 2.0f, WHITE);

    // Paleta rapida.
    for (int r = 0; r < NC_SW_ROWS; r++) {
        for (int c = 0; c < NC_SW_COLS; c++) {
            const Rectangle sr = SwatchRect(r, c);
            DrawRectangleRec(sr, swatches[r * NC_SW_COLS + c]);
            DrawRectangleLinesEx(sr, 2.0f, (Color) {20, 20, 20, 255});
        }
    }

    DrawTextEx(mainFont, "Wheel: hue/saturation   Bar: brightness   Swatch: quick pick",
               (Vector2) {80, 662}, 22, 1, GRAY);
    DrawTextEx(mainFont, "LEFT/RIGHT: column   R: reset column   SHIFT+R: reset all   ESC/BACKSPACE: back",
               (Vector2) {80, 688}, 22, 1, GRAY);
}

void NoteColorScreen_Destroy(void) {
    if (wheelReady) UnloadTexture(wheelTex);
    wheelReady = false;
    NoteColors_ReleaseFx();
}
