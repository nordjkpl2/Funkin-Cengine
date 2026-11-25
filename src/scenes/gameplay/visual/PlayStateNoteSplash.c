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
#include "data/config/EffectsConfig.h"
#include "data/config/NotesplashConfig.h"

#include <string.h>

#define SPLASH_DEFAULT_SCALE 0.8f /* padrao do Effects Editor (scalePct 80) */

/* Vida = 4 frames / 24fps ~= 0.167s. Em spam de 32avos (~0.04s por nota) ficam ~5 vivos por coluna. */
#define SPLASH_LANE_MAX_ALIVE 8

typedef struct {
    RayAnimatedObject object;
    float life;
    char active;
} Splash;

static RayAnimationHandler splashSet;
static char splashAnimset[NOTESPLASH_PATH_MAX]; /* atlas carregado agora; vazio = nenhum */
static int splashAnim = -1;
static float splashLife;
static Vector2 splashSize;          /* tamanho sem escala, medido uma vez no load */

static Splash pool[PLAYSTATE_LANES][SPLASH_LANE_MAX_ALIVE];

void PlayStateNoteSplash_Load(void) {
    const EffectsSplash* cfg = &EffectsConfig_Get()->splash;
    const NotesplashEntry* entry = NotesplashConfig_Active();

    // Atlas so e trocado se a entrada ativa mudou; fps vem do Effects Editor e e reaplicado a cada entrada no PlayState.
    if (strcmp(splashAnimset, entry->animset) != 0) {
        char texture[NOTESPLASH_PATH_MAX];
        AnimationSet_FreeAll(&splashSet);
        splashAnimset[0] = '\0';
        if (NotesplashConfig_TexturePath(entry->animset, texture, sizeof(texture))) {
            splashSet = AnimationSet_LoadAnimations(entry->animset, Render_LoadTexture(texture));
            strcpy(splashAnimset, entry->animset);
        }
    }

    int anim = -1;
    if (splashAnimset[0] != '\0')
        AnimationSet_SetData(anim, splashSet, entry->prefix, cfg->fps, 0, 0, 0);
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

void PlayStateNoteSplash_Spawn(int lane, char bfSide) {
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

    // Centro do receptor do lado que acertou (BF ou oponente); o splash fica centralizado nele.
    const EffectsSplash* cfg = &EffectsConfig_Get()->splash;
    const float scale = (float)cfg->scalePct / 100.0f;
    const RayAnimatedObject* strum = &(bfSide ? PlayStateStrums_Bf() : PlayStateStrums_Dad())[lane].object;
    const Vector2 strumSize = AnimatedObject_Sizes(strum);
    const float halfW = splashSize.x * scale * 0.5f;
    const float halfH = splashSize.y * scale * 0.5f;

    RayAnimatedObject* o = &slot->object;
    Render_DefaultAnimated(o);
    o->animationSet = splashSet;
    o->scaleX = scale;
    o->scaleY = scale;
    const Color tint = EffectsConfig_TintColor(cfg->tint);
    o->color = (Color){tint.r, tint.g, tint.b, (unsigned char)(cfg->alphaPct * 255 / 100)};
    o->position.x = strum->position.x + strumSize.x * 0.5f - halfW + (float)cfg->offsetX;
    o->position.y = strum->position.y + strumSize.y * 0.5f - halfH + (float)cfg->offsetY;
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
