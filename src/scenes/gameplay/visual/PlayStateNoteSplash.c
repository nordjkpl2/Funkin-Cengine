/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateNoteSplash.c - splash de acerto nos receptores do BF.
 *
 * Custo: 1 textura + 1 animacao (4 frames) carregadas uma vez; pool fixo por coluna;
 * cada acerto gera um splash novo (sem cooldown). Se o pool da coluna encher, recicla o mais antigo.
 */

#include "scenes/gameplay/visual/PlayStateNoteSplash.h"
#include "scenes/gameplay/notes/PlayStateStrums.h"
#include "GameData.h"

#define SPLASH_TEXTURE "assets/images/noteSplashes-sparkles.png"
#define SPLASH_ANIMSET "assets/images/noteSplashes-sparkles.xml"
/* O parser do Sparrow tira os digitos finais do nome do frame, sobrando o espaco: "splash sparkles red 10000" -> "splash sparkles red " */
#define SPLASH_ANIM_NAME "splash sparkles red " /* as 4 cores do XML tem os mesmos frames */
#define SPLASH_FPS 24
#define SPLASH_SCALE 0.8f         /* ajuste visual inicial, tunar com o receptor */
#define SPLASH_ALPHA 153          /* ~0.6, como a Psych */

/* Vida = 4 frames / 24fps ~= 0.167s. Em spam de 32avos (~0.04s por nota) ficam ~5 vivos por coluna. */
#define SPLASH_LANE_MAX_ALIVE 8

typedef struct {
    RayAnimatedObject object;
    float life;
    char active;
} Splash;

static RayAnimationHandler splashSet;
static int splashAnim = -1;
static float splashLife;
static Vector2 splashSize;          /* tamanho sem escala, medido uma vez no load */
static char splashLoaded;

static Splash pool[PLAYSTATE_LANES][SPLASH_LANE_MAX_ALIVE];

void PlayStateNoteSplash_Load(void) {
    if (splashLoaded)
        return;
    splashLoaded = 1;

    splashSet = AnimationSet_LoadAnimations(SPLASH_ANIMSET, Render_LoadTexture(SPLASH_TEXTURE));

    int anim = -1;
    AnimationSet_SetData(anim, splashSet, SPLASH_ANIM_NAME, SPLASH_FPS, 0, 0, 0);
    splashAnim = anim;
    if (splashAnim < 0)
        return;

    splashLife = AnimationSet_AnimationLength(&splashSet, splashAnim);

    RayAnimatedObject probe;
    Render_DefaultAnimated(&probe);
    probe.animationSet = splashSet;
    AnimatedObject_SetAnimation(&probe, splashAnim);
    splashSize = AnimatedObject_Sizes(&probe);
}

void PlayStateNoteSplash_Reset(void) {
    for (int lane = 0; lane < PLAYSTATE_LANES; lane++)
        for (int i = 0; i < SPLASH_LANE_MAX_ALIVE; i++)
            pool[lane][i].active = 0;
}

void PlayStateNoteSplash_Spawn(int lane) {
    if (!gameOptions.noteSplashes || splashAnim < 0 || (unsigned int)lane >= PLAYSTATE_LANES)
        return;

    Splash* row = pool[lane];
    Splash* slot = NULL;
    for (int i = 0; i < SPLASH_LANE_MAX_ALIVE; i++) {
        if (!row[i].active) {
            slot = &row[i];
            break;
        }
    }
    if (slot == NULL) {
        // Pool cheio (spam): recicla o splash com menos vida, que e o mais antigo.
        slot = &row[0];
        for (int i = 1; i < SPLASH_LANE_MAX_ALIVE; i++)
            if (row[i].life < slot->life)
                slot = &row[i];
    }

    // Centro do receptor do BF da coluna; o splash fica centralizado nele.
    const RayAnimatedObject* strum = &PlayStateStrums_Bf()[lane].object;
    const Vector2 strumSize = AnimatedObject_Sizes(strum);
    const float halfW = splashSize.x * SPLASH_SCALE * 0.5f;
    const float halfH = splashSize.y * SPLASH_SCALE * 0.5f;

    RayAnimatedObject* o = &slot->object;
    Render_DefaultAnimated(o);
    o->animationSet = splashSet;
    o->scaleX = SPLASH_SCALE;
    o->scaleY = SPLASH_SCALE;
    o->color = (Color){255, 255, 255, SPLASH_ALPHA};
    o->position.x = strum->position.x + strumSize.x * 0.5f - halfW;
    o->position.y = strum->position.y + strumSize.y * 0.5f - halfH;
    AnimatedObject_SetAnimation(o, splashAnim);

    slot->life = splashLife;
    slot->active = 1;
}

void PlayStateNoteSplash_Draw(float deltaTime) {
    for (int lane = 0; lane < PLAYSTATE_LANES; lane++) {
        for (int i = 0; i < SPLASH_LANE_MAX_ALIVE; i++) {
            Splash* s = &pool[lane][i];
            if (!s->active)
                continue;

            s->life -= deltaTime;
            if (s->life <= 0.0f) {
                s->active = 0;
                continue;
            }

            AnimatedObject_UpdateFrame(&s->object);
            Render_DrawAnimatedObject(&s->object);
        }
    }
}
