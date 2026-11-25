/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateStrums.c - receptores (strumlines) do oponente e do BF.
 */

#include "scenes/api/FontLoader.h"
#include "logic/notes/Note.h"
#include "scenes/gameplay/notes/PlayStateStrums.h"
#include "scenes/gameplay/characters/PlayStateBeat.h"
#include "GameData.h"
#include "rendering/TextMeasure.h"
#include <math.h>

#define SCREEN_W 1280.0f
#define LANE_SPACING 110.0f
#define STRUM_BG_HEIGHT 150   // faixa preta atras dos receptores (Strumline Background)
// Monitor de teclas (Keys Monitor): quadradinhos no canto superior direito.
#define MONITOR_KEY 40.0f         // lado de cada quadradinho
#define MONITOR_GAP 6.0f          // espaco entre eles
#define MONITOR_MARGIN 20.0f      // distancia da borda direita
#define MONITOR_TOP 20.0f         // distancia da borda superior
#define MONITOR_FONT 24
#define MOBILE_GLOW_RISE 12.0f    // velocidade de subida (por segundo): acende rapido
#define MOBILE_GLOW_FALL 4.0f     // velocidade de descida (por segundo): fade curto

/* Mesmas cores das notas no editor de chart: roxo, azul, verde, vermelho. */
static const Color laneColors[PLAYSTATE_LANES] = {
    {200, 80, 255, 255}, {80, 180, 255, 255}, {80, 255, 150, 255}, {255, 90, 90, 255},
};
static const char* const laneLetters[PLAYSTATE_LANES] = {"L", "D", "U", "R"};

/* gameOptions.scroll: SCROLL_UP, SCROLL_DOWN, SCROLL_MIDDLE, SCROLL_MIDDLEDOWN (ver GameData.h) */
static StrumNote dadStrumline[PLAYSTATE_LANES];
static StrumNote bfStrumline[PLAYSTATE_LANES];
static Vector2 dadStrumIdleSizes[PLAYSTATE_LANES];
static Vector2 bfStrumIdleSizes[PLAYSTATE_LANES];
static float bfMiddleX = 475.0f; // middlescroll: X do 1o receptor do BF (centralizado)
static float mobileGlow[2][PLAYSTATE_LANES]; // [jogador][coluna] 0..1, sobe suave ao apertar a coluna e desce devagar (sem pisca)

static float StrumY(void) {
    if (SCROLL_IS_DOWN(gameOptions.scroll)) return 600.0f;
    if (SCROLL_IS_MIDDLE(gameOptions.scroll)) return 20.0f;
    return 50.0f;
}

static float StrumX(char mustHit, int lane) {
    if (!SCROLL_IS_MIDDLE(gameOptions.scroll))
        return (mustHit ? 700.0f : 50.0f) + LANE_SPACING * lane;

    return mustHit ? bfMiddleX + LANE_SPACING * lane : 50.0f + 55.0f * lane;
}

StrumNote* PlayStateStrums_Dad(void) { return dadStrumline; }
StrumNote* PlayStateStrums_Bf(void) { return bfStrumline; }

void PlayStateStrums_Load(void) {
    const char middle = SCROLL_IS_MIDDLE(gameOptions.scroll);

    for (int i = 0; i < PLAYSTATE_LANES; i++) {
        StrumNote_Load(dadStrumline + i, i);
        StrumNote_Load(bfStrumline + i, i);

        dadStrumline[i].object.position.y = StrumY();
        bfStrumline[i].object.position.y = StrumY();

        // Middlescroll: oponente menor; BF na escala normal das notas.
        if (middle)
            dadStrumline[i].object.scaleX = dadStrumline[i].object.scaleY = 0.5f;
    }

    if (middle) {
        RayAnimatedObject idle = bfStrumline[0].object;
        AnimatedObject_SetAnimation(&idle, StrumNote_IdleAnimation(0));
        bfMiddleX = SCREEN_W / 2 - (LANE_SPACING * (PLAYSTATE_LANES - 1) + AnimatedObject_Sizes(&idle).x) * 0.5f;
    }

    for (int i = 0; i < PLAYSTATE_LANES; i++) {
        dadStrumline[i].object.position.x = StrumX(0, i);
        bfStrumline[i].object.position.x = StrumX(1, i);
    }

    // Cacheia o tamanho idle uma vez: nao mede/refaz a animacao a cada frame.
    for (int i = 0; i < PLAYSTATE_LANES; i++) {
        RayAnimatedObject dadIdle = dadStrumline[i].object;
        RayAnimatedObject bfIdle = bfStrumline[i].object;

        AnimatedObject_SetAnimation(&dadIdle, StrumNote_IdleAnimation(i));
        AnimatedObject_SetAnimation(&bfIdle, StrumNote_IdleAnimation(i));

        dadStrumIdleSizes[i] = AnimatedObject_Sizes(&dadIdle);
        bfStrumIdleSizes[i] = AnimatedObject_Sizes(&bfIdle);
    }
}

/*
 * Desenha o receptor mantendo o centro visual da animacao idle como ancora.
 * Animacoes maiores (confirm, por causa do brilho) fariam a seta "saltar";
 * o tamanho idle e medido uma vez no load. A posicao logica da coluna e
 * restaurada depois do desenho, entao notas e sustains usam a coordenada fixa.
 */
static void RenderStrumFixedCenter(StrumNote* strum, Vector2 idleSize) {
    Vector2 currentSize = AnimatedObject_Sizes(&strum->object);
    Vector2 originalPosition = strum->object.position;

    strum->object.position.x += (idleSize.x - currentSize.x) * 0.5f;
    strum->object.position.y += (idleSize.y - currentSize.y) * 0.5f;

    Render_DrawAnimatedObject(&strum->object);

    strum->object.position = originalPosition;
}

/* Fundo atras de tudo do HUD: faixa preta dos receptores. */
void PlayStateStrums_DrawBackdrop(void) {
    if (gameOptions.strumBgPercent > 0) {
        const unsigned char alpha = (unsigned char)(gameOptions.strumBgPercent * 255 / 100);
        DrawRectangle(0, (int)StrumY() - 10, (int)SCREEN_W, STRUM_BG_HEIGHT, (Color) {0, 0, 0, alpha});
    }
}

/* Chamar todo frame (inclusive botplay, com keyDown zerado): sobe ao apertar, desce devagar. */
void PlayStateStrums_UpdateGlow(const PlayStatePlayerKeys* keys) {
    const float dt = GetFrameTime();
    for (int s = 0; s < 2; s++) {
        for (int i = 0; i < PLAYSTATE_LANES; i++) {
            const float target = keys[s].keyDown[i] ? 1.0f : 0.0f;
            const float speed = target > mobileGlow[s][i] ? MOBILE_GLOW_RISE : MOBILE_GLOW_FALL;
            const float step = speed * dt > 1.0f ? 1.0f : speed * dt;
            mobileGlow[s][i] += (target - mobileGlow[s][i]) * step;
        }
    }
}

/* Tecla da coluna para o monitor: letra/numero quando possivel, senao a inicial da direcao. */
static void MonitorLabel(int key, int lane, char out[4]) {
    if (key >= KEY_A && key <= KEY_Z) { out[0] = (char)('A' + (key - KEY_A)); out[1] = 0; }
    else if (key >= KEY_ZERO && key <= KEY_NINE) { out[0] = (char)('0' + (key - KEY_ZERO)); out[1] = 0; }
    else { out[0] = laneLetters[lane][0]; out[1] = 0; }
}

/* Monitor de teclas estilo overlay de streamer: quadradinhos que acendem com a tecla. Desenhar por cima das notas. */
void PlayStateStrums_DrawKeyMonitor(void) {
    if (!gameOptions.strumMobileVisible) return;

    const float total = PLAYSTATE_LANES * MONITOR_KEY + (PLAYSTATE_LANES - 1) * MONITOR_GAP;
    const float x0 = SCREEN_W - MONITOR_MARGIN - total;
    static TextMeasure labelMeasure[2][PLAYSTATE_LANES]; // rotulo da coluna muda so com as teclas

    // Uma linha por jogador: so o jogador 1, ou os 2 no Co-Op (empilhadas, do topo para baixo).
    const int rows = PlayStatePlayerInput_Slots();
    for (int s = 0; s < rows; s++) {
        const float y = MONITOR_TOP + s * (MONITOR_KEY + MONITOR_GAP);
        for (int i = 0; i < PLAYSTATE_LANES; i++) {
            const Rectangle box = {x0 + i * (MONITOR_KEY + MONITOR_GAP), y, MONITOR_KEY, MONITOR_KEY};

            DrawRectangleRec(box, (Color) {0, 0, 0, 150});
            /* Cor da coluna: a cor base com o ajuste HSB da coluna (mesma conta do shader da nota). */
            Color base = laneColors[i];
            if (GameData_LaneShifted(i)) base = GameData_ShiftColor(base, GameData_LaneShift(i));
            base.a = 255;
            Color lit = base;
            lit.a = (unsigned char)(255.0f * mobileGlow[s][i]);
            DrawRectangleRec(box, lit);
            DrawRectangleLinesEx(box, 2.0f, base);

            char label[4];
            MonitorLabel(PlayStatePlayerInput_Key(s, i), i, label);
            const Vector2 ts = TextMeasure_Get(&labelMeasure[s][i], mainFont, label, MONITOR_FONT, 1.0f);
            DrawTextEx(mainFont, label, (Vector2) {box.x + (MONITOR_KEY - ts.x) * 0.5f, box.y + (MONITOR_KEY - ts.y) * 0.5f},
                       MONITOR_FONT, 1.0f, WHITE);
        }
    }
}

void PlayStateStrums_Draw(void) {
    // Hidden Notes (Alternate): so o receptor de quem esta cantando aparece. A animacao segue rodando.
    const char singerBf = PlayStateBeat_SingerIsBf();

    for (int i = 0; i < PLAYSTATE_LANES; i++) {
        AnimatedObject_UpdateFrame(&dadStrumline[i].object);
        if (GameData_HiddenShowsSide(0, singerBf))
            RenderStrumFixedCenter(dadStrumline + i, dadStrumIdleSizes[i]);

        AnimatedObject_UpdateFrame(&bfStrumline[i].object);
        if (GameData_HiddenShowsSide(1, singerBf))
            RenderStrumFixedCenter(bfStrumline + i, bfStrumIdleSizes[i]);
    }

    // Botplay: volta o BF ao idle quando a animacao de confirm termina.
    if (gameOptions.botplay) {
        for (int i = 0; i < PLAYSTATE_LANES; i++) {
            if (AnimatedObject_FinishedAnimation(&bfStrumline[i].object))
                AnimatedObject_SetAnimation(&bfStrumline[i].object, StrumNote_IdleAnimation(i));
        }
    }
}

/* Co-Op: nome de cada jogador acima dos receptores do seu lado. */
void PlayStateStrums_DrawSideNames(const char* bfName, const char* dadName) {
    const float fontSize = 30.0f;
    const float y = fmaxf(4.0f, StrumY() - 40.0f);
    for (int side = 0; side < 2; side++) {
        const RayAnimatedObject* first = side ? &bfStrumline[0].object : &dadStrumline[0].object;
        const RayAnimatedObject* last = side ? &bfStrumline[PLAYSTATE_LANES - 1].object : &dadStrumline[PLAYSTATE_LANES - 1].object;
        const char* text = side ? bfName : dadName;
        const Vector2 ts = MeasureTextEx(mainFont, text, fontSize, 2.0f);
        const float cx = (first->position.x + last->position.x) * 0.5f;
        const Vector2 pos = {cx - ts.x * 0.5f, y};
        DrawTextEx(mainFont, text, (Vector2) {pos.x + 2.0f, pos.y + 2.0f}, fontSize, 2.0f, (Color) {0, 0, 0, 200});
        DrawTextEx(mainFont, text, pos, fontSize, 2.0f, WHITE);
    }
}

void PlayStateStrums_ConfirmDad(int lane) {
    AnimatedObject_SetAnimation(&dadStrumline[lane].object, StrumNote_ConfirmAnimation(lane));
}

void PlayStateStrums_ConfirmBf(int lane) {
    AnimatedObject_SetAnimation(&bfStrumline[lane].object, StrumNote_ConfirmAnimation(lane));
}

// Animacao dos receptores de um lado (BF ou oponente, conforme quem controla) a partir das teclas deste frame.
void PlayStateStrums_ApplyInput(const PlayStatePlayerKeys* keys, char bfSide) {
    for (int i = 0; i < PLAYSTATE_LANES; i++) {
        RayAnimatedObject* strum = bfSide ? &bfStrumline[i].object : &dadStrumline[i].object;

        if (keys->pressed[i])
            AnimatedObject_SetAnimation(strum, StrumNote_ConfirmAnimation(i));
        else if (!keys->keyDown[i]) {
            const int idle = StrumNote_IdleAnimation(i);
            if (strum->currentAnim.animationIndex != idle)
                AnimatedObject_SetAnimation(strum, idle);
        }
        else if (keys->keyPressed[i])
            AnimatedObject_SetAnimation(strum, StrumNote_PressAnimation(i));
    }
}
