/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
#include "scenes/api/FontLoader.h"
#include "scenes/gameplay/PlayState.h"
#include "scenes/LoadingState.h"
#include "scenes/Transition.h"
#include "Game.h"
#include <math.h>
/* // * Tela de Loading pra não ter que esperar carregar charts de teste, outd 18 of10 2026 */

// --- estado da tela (static, sem globais) ---
static char pendingDir[256];
static bool presented;
static bool leaving;        // veu de saida iniciado; a gameplay entra quando ele terminar

#define LEAVE_FADE_SECS 0.25f   // saida suave da loading
#define ENTER_FADE_SECS 0.35f   // entrada suave da gameplay
static Texture2D bgTex;
static float fadeAlpha;     // 1 = opaco (sem fade)
static float progress;      // 0..1, valor mostrado na barra (suavizado)

static const char* MSG = "LOADING";
static const int FONT_SIZE = 40;
static const int SPACING = 4;

// Layout de referência (canvas 1280x720)
#define REF_W 1280.0f
#define REF_H 720.0f
#define BAR_W 520.0f
#define BAR_H 10.0f
#define BAR_Y 640.0f

void LoadingState_SetSongDir(const char* dir) {
    snprintf(pendingDir, sizeof(pendingDir), "%s", dir);
}

static void LoadingState_Create(RayScene* scene) {
    (void)scene;
    presented = false;
    leaving = false;
    fadeAlpha = 1.0f;   // sem fade: a tela aparece direto, sem tempo extra
    progress = 0.0f;
    bgTex = Render_LoadTexture("assets/images/loading/loadingBG.jpg");
}

static void LoadingState_Update(RayScene* scene) {
    (void)scene;

    // Barra sobe visualmente até ~80% (não é progresso real: o carregamento é bloqueante)
    progress += (0.8f - progress) * 0.35f;

    if (!presented) {
        // deixa a Draw apresentar 1 frame, sem tempo extra
        presented = true;
        return;
    }
    progress = 1.0f;
    if (!leaving) {
        leaving = true;
        Transition_FadeOut(LEAVE_FADE_SECS);
        return;
    }
    if (!Transition_Done()) return;

    PlayState_SetSongDirKeep(pendingDir);
    PlayState_SetScene();
    Transition_FadeIn(ENTER_FADE_SECS);   // a gameplay abre do preto
}

// Tudo é desenhado no canvas fixo (1280x720); o Game estica para a janela.
static void DrawCoverTexture(Texture2D tex, float alpha) {
    if (tex.id == 0) return;
    float sw = REF_W, sh = REF_H;
    float scale = fmaxf(sw / (float)tex.width, sh / (float)tex.height);
    float dw = tex.width * scale, dh = tex.height * scale;
    Rectangle src = { 0, 0, (float)tex.width, (float)tex.height };
    Rectangle dst = { (sw - dw) * 0.5f, (sh - dh) * 0.5f, dw, dh };
    DrawTexturePro(tex, src, dst, (Vector2){ 0, 0 }, 0.0f, Fade(WHITE, alpha));
}

static void LoadingState_Draw(RayScene* scene) {
    (void)scene;
    ClearBackground(BLACK);

    const float sw = REF_W, sh = REF_H, k = 1.0f;

    DrawCoverTexture(bgTex, fadeAlpha);
    // Escurece o fundo para o texto e a barra ficarem legíveis
    DrawRectangle(0, 0, (int)sw, (int)sh, Fade(BLACK, 0.35f * fadeAlpha));

    // Barra de progresso centralizada
    float barW = BAR_W * k, barH = BAR_H * k;
    float barX = sw * 0.5f - barW * 0.5f;
    float barY = BAR_Y * k;
    Rectangle track = { barX, barY, barW, barH };
    DrawRectangleRounded(track, 1.0f, 8, Fade(BLACK, 0.55f * fadeAlpha));
    DrawRectangleRoundedLinesEx(track, 1.0f, 8, 2.0f * k, Fade(WHITE, 0.25f * fadeAlpha));
    Rectangle fill = { barX, barY, barW * progress, barH };
    if (fill.width > 0.0f) {
        DrawRectangleRounded(fill, 1.0f, 8, Fade(WHITE, fadeAlpha));
    }

    // Texto e porcentagem acima da barra
    int pct = (int)(progress * 100.0f + 0.5f);
    char pctText[16];
    snprintf(pctText, sizeof(pctText), "%d%%", pct);

    float fs = FONT_SIZE * k, sp = SPACING * k;
    Vector2 msgSize = MeasureTextEx(mainFont, MSG, fs, sp);
    DrawTextEx(mainFont, MSG, (Vector2){ sw * 0.5f - msgSize.x * 0.5f, barY - msgSize.y - 14.0f * k },
               fs, sp, Fade(WHITE, fadeAlpha));

    Vector2 pctSize = MeasureTextEx(mainFont, pctText, fs * 0.6f, sp);
    DrawTextEx(mainFont, pctText, (Vector2){ sw * 0.5f - pctSize.x * 0.5f, barY + barH + 10.0f * k },
               fs * 0.6f, sp, Fade(WHITE, 0.8f * fadeAlpha));

    if (leaving) Transition_Draw();
}

static void LoadingState_Destroy(RayScene* scene) {
    (void)scene;
    if (bgTex.id != 0) {
        UnloadTexture(bgTex);
        bgTex = (Texture2D){ 0 };
    }
}

static RayScene loadingScene;
Scene_MakeSceneCode(
    loadingScene,
    LoadingState_SetScene,
    LoadingState_Create,
    LoadingState_Update,
    LoadingState_Draw,
    LoadingState_Destroy
)
