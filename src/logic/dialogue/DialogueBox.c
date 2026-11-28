/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * DialogueBox.c - porta do DialogueBoxPsych (Psych Engine 0.7).
 * Constantes, posicoes e tempos iguais aos da Psych; o texto usa a fonte principal
 * (sem Alphabet) com quebra automatica de linha.
 */

#include "logic/dialogue/DialogueBox.h"
#include "logic/animations/AnimSetSparrow.h"
#include "scenes/api/FontLoader.h"
#include "Render.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define SCREEN_W 1280.0f
#define SCREEN_H 720.0f

#define BOX_X 70.0f
#define BOX_Y 370.0f
#define BOX_SCALE 0.9f
#define BOX_FPS 24.0f
#define FALLBACK_OPEN_SECS 0.18f   /* abertura da bolha desenhada (sem speech_bubble) */

#define LEFT_CHAR_X (-60.0f)
#define RIGHT_CHAR_X (-100.0f)
#define DEFAULT_CHAR_Y 60.0f
#define OFFSET_POS (-600.0f)
#define CHAR_SCALE 0.7f
#define SCROLL_SPEED 4000.0f
#define PORTRAIT_MAX 16

#define TEXT_X 175.0f
#define TEXT_Y 460.0f
#define LONG_TEXT_ADD 24.0f
#define TEXT_SIZE 40.0f
#define TEXT_SPACING 1.0f
#define TEXT_LINE_H 54.0f
#define TEXT_WRAP 930.0f
#define TEXT_ROWS_MAX 12

#define SFX_MAX 12
#define SFX_ALIASES 3   /* letras rapidas sobrepoem o som (como a Psych) */

/* Bolha: indice = CENTER | OPEN | ANGRY (mesma ordem dos nomes da Psych). */
enum { BOX_ANGRY = 1, BOX_OPEN = 2, BOX_CENTER = 4, BOX_SLOTS = 8 };
static const char *const kBoxPrefix[BOX_SLOTS] = {
    "speech bubble normal", "AHH speech bubble",
    "Speech Bubble Normal Open", "speech bubble loud open",
    "speech bubble middle", "AHH Speech Bubble middle",
    "Speech Bubble Middle Open", "speech bubble Middle loud open"
};

typedef struct {
    int anim;
    int frame;
    float time;
    float fps;
    bool loop;
    bool reverse;
    bool finished;
} Player;

typedef struct {
    char name[DIALOGUE_NAME_MAX];
    DialogueCharDef def;
    bool found;    /* json com este nome */
    bool hasDef;   /* algum json (o proprio ou o padrao) */
    RayAnimationHandler set;
    int loopIdx[DIALOGUE_ANIM_MAX];
    int idleIdx[DIALOGUE_ANIM_MAX];
    float fw, fh, scale;
    float x, y, startingPos, alpha;
    Player play;
    int curAnim;   /* indice em def.anims, -1 = nenhum */
    bool idle;
} Portrait;

typedef struct {
    char name[DIALOGUE_NAME_MAX];
    Sound base;
    Sound alias[SFX_ALIASES];
    int next;
    bool ok;
} Sfx;

/* ------------------------------------------------------------------------- */
/* Estado                                                                    */
/* ------------------------------------------------------------------------- */

static bool active, ended, finished;
static char modRoot[512];
static const DialogueFile *file;   /* NULL = editor (uma fala por vez) */
static int currentText;
static DialogueLine cur;

static Portrait portraits[PORTRAIT_MAX];
static int portraitCount;
static int lastCharacter = -1;
static bool lastAngry;

static RayAnimationHandler bubble;
static bool hasBubble;
static int boxIdx[BOX_SLOTS];
static float boxW, boxH;
static int boxSlot;
static Player boxPlay;
static bool boxFlip, boxVisible, boxAlive;
static float fallbackT;

static bool bgAlive;
static float bgAlpha;

static char text[DIALOGUE_TEXT_MAX];
static int rowStart[TEXT_ROWS_MAX], rowEnd[TEXT_ROWS_MAX], rowCount;
static int letterCount, shown;
static float delay, curDelay;
static bool textDone;

static Sfx sfx[SFX_MAX];
static int sfxCount;
static int lineSound = -1, closeSound = -1;

/* ------------------------------------------------------------------------- */
/* Animacao (com reverse, que o RayAnimatedObject nao tem)                   */
/* ------------------------------------------------------------------------- */

static void Player_Play(Player *p, int anim, float fps, bool loop)
{
    *p = (Player){.anim = anim, .fps = fps, .loop = loop, .finished = anim < 0};
}

static void Player_Restart(Player *p)
{
    p->frame = 0;
    p->time = 0.0f;
    p->finished = false;
}

static void Player_Update(Player *p, const RayAnimationHandler *set, float dt)
{
    if (p->anim < 0 || p->finished || !set->animations || p->fps <= 0.0f)
        return;
    const int count = set->animations[p->anim].frameCount;
    if (count <= 0)
        return;
    const float ft = 1.0f / p->fps;
    p->time += dt;
    while (p->time >= ft && !p->finished) {
        p->time -= ft;
        if (!p->reverse) {
            if (p->frame + 1 < count) ++p->frame;
            else if (p->loop) p->frame = 0;
            else p->finished = true;
        } else {
            if (p->frame > 0) --p->frame;
            else if (p->loop) p->frame = count - 1;
            else p->finished = true;
        }
    }
}

/* addByPrefix: nome exato; senao um grupo que comeca com o prefixo; senao o prefixo e
   grupo + digitos (o atlas tira os digitos finais do nome). */
static int FindPrefix(const RayAnimationHandler *set, const char *prefix)
{
    if (!prefix || !prefix[0] || !set->animations)
        return -1;
    const int exact = AnimationSet_FindAnimation(set, prefix);
    if (exact >= 0)
        return exact;
    const size_t n = strlen(prefix);
    for (int i = 0; i < set->animationCount; ++i) {
        if (strncmp(set->animations[i].name, prefix, n) == 0)
            return i;
    }
    for (int i = 0; i < set->animationCount; ++i) {
        const size_t len = strlen(set->animations[i].name);
        if (len == 0 || len >= n || strncmp(set->animations[i].name, prefix, len) != 0)
            continue;
        const char *rest = prefix + len;
        while (*rest >= '0' && *rest <= '9')
            ++rest;
        if (*rest == '\0')
            return i;
    }
    return -1;
}

/* Tamanho do quadro sem recorte (o atlas nao guarda frameWidth). */
static void Extents(const RayAnimation *a, float *w, float *h)
{
    for (int i = 0; i < a->frameCount; ++i) {
        const Frame *f = &a->frames[i];
        *w = fmaxf(*w, (float)(f->w - f->fx));
        *h = fmaxf(*h, (float)(f->h - f->fy));
    }
}

/* Quadro com a origem da Flixel: (left, top) e o canto do quadro inteiro ja escalado.
   flip espelha o quadro inteiro (o recorte troca de lado), como o flipX. */
static void DrawFrame(Texture2D tex, const Frame *f, float left, float top, float s, bool flip, float fw, Color tint)
{
    const float fx = flip ? fw - ((float)-f->fx + (float)f->w) : (float)-f->fx;
    const Rectangle src = {(float)f->x, (float)f->y, flip ? -(float)f->w : (float)f->w, (float)f->h};
    const Rectangle dst = {left + fx * s, top + (float)-f->fy * s, (float)f->w * s, (float)f->h * s};
    DrawTexturePro(tex, src, dst, (Vector2){0.0f, 0.0f}, 0.0f, tint);
}

/* ------------------------------------------------------------------------- */
/* Sons                                                                      */
/* ------------------------------------------------------------------------- */

static int SfxFor(const char *name)
{
    if (!name || !name[0])
        return -1;
    for (int i = 0; i < sfxCount; ++i) {
        if (strcmp(sfx[i].name, name) == 0)
            return sfx[i].ok ? i : -1;
    }
    if (sfxCount >= SFX_MAX)
        return -1;

    Sfx *s = &sfx[sfxCount];
    memset(s, 0, sizeof(*s));
    snprintf(s->name, sizeof(s->name), "%s", name);
    char rel[DIALOGUE_NAME_MAX + 16], path[640];
    snprintf(rel, sizeof(rel), "sounds/%s.ogg", name);
    if (Dialogue_FindAsset(modRoot, rel, path, sizeof(path))) {
        s->base = LoadSound(path);
        s->ok = IsSoundValid(s->base);
        for (int k = 0; s->ok && k < SFX_ALIASES; ++k)
            s->alias[k] = LoadSoundAlias(s->base);
    }
    const int idx = sfxCount++;   /* guarda tambem os que faltam: nao procura de novo */
    return s->ok ? idx : -1;
}

static void SfxPlay(int i)
{
    if (i < 0)
        return;
    Sfx *s = &sfx[i];
    PlaySound(s->next == 0 ? s->base : s->alias[s->next - 1]);
    s->next = (s->next + 1) % (SFX_ALIASES + 1);
}

static void SfxFreeAll(void)
{
    for (int i = 0; i < sfxCount; ++i) {
        if (!sfx[i].ok)
            continue;
        for (int k = 0; k < SFX_ALIASES; ++k)
            UnloadSoundAlias(sfx[i].alias[k]);
        UnloadSound(sfx[i].base);
    }
    sfxCount = 0;
    lineSound = closeSound = -1;
}

/* ------------------------------------------------------------------------- */
/* Texto digitado                                                            */
/* ------------------------------------------------------------------------- */

static float Measure(int start, int end)
{
    char tmp[DIALOGUE_TEXT_MAX];
    const int len = end - start;
    memcpy(tmp, text + start, (size_t)len);
    tmp[len] = '\0';
    return MeasureTextEx(mainFont, tmp, TEXT_SIZE, TEXT_SPACING).x;
}

static void PushRow(int start, int end)
{
    if (rowCount >= TEXT_ROWS_MAX)
        return;
    rowStart[rowCount] = start;
    rowEnd[rowCount] = end;
    ++rowCount;
}

/* Quebra em '\n' e, se passar da largura, no ultimo espaco. */
static void LayoutText(void)
{
    rowCount = 0;
    int start = 0, lastSpace = -1;
    const int n = (int)strlen(text);
    for (int i = 0; i <= n; ++i) {
        const char c = text[i];
        if (c != ' ' && c != '\n' && c != '\0')
            continue;
        if (lastSpace > start && Measure(start, i) > TEXT_WRAP) {
            PushRow(start, lastSpace);
            start = lastSpace + 1;
        }
        if (c == ' ') {
            lastSpace = i;
        } else {
            PushRow(start, i);
            start = i + 1;
            lastSpace = -1;
        }
    }
}

static void SetText(const char *t, float speed)
{
    snprintf(text, sizeof(text), "%s", t);
    for (char *c = text; *c; ++c) {
        if (*c == '\r' || *c == '\t')
            *c = ' ';
    }
    LayoutText();

    letterCount = 0;
    for (const char *c = text; *c; ++c) {
        if (*c != ' ' && *c != '\n' && ((unsigned char)*c & 0xC0) != 0x80)   /* so o 1o byte do UTF-8 */
            ++letterCount;
    }
    shown = 0;
    delay = speed;
    curDelay = 0.0f;
    textDone = delay <= 0.0f || letterCount == 0;
    if (textDone)
        shown = letterCount;
}

static void FinishText(void)
{
    shown = letterCount;
    textDone = true;
    curDelay = 0.0f;
}

/* TypedAlphabet.update: um som por frame (letras muito rapidas: so nas pares). */
static void TypeText(float dt)
{
    if (textDone)
        return;
    bool played = false;
    curDelay += dt;
    while (curDelay >= delay && !textDone) {
        ++shown;
        if (shown >= letterCount) {
            textDone = true;
            curDelay = 0.0f;
        }
        if (!played && (delay > 0.025f || shown % 2 == 0))
            SfxPlay(lineSound);
        played = true;
        curDelay -= delay;
    }
}

static void DrawTypedText(void)
{
    const float y0 = TEXT_Y - (rowCount > 2 ? LONG_TEXT_ADD : 0.0f);
    int remaining = shown;
    char tmp[DIALOGUE_TEXT_MAX];
    for (int r = 0; r < rowCount && remaining > 0; ++r) {
        int len = 0;
        for (int i = rowStart[r]; i < rowEnd[r]; ++i) {
            const unsigned char c = (unsigned char)text[i];
            if (c != ' ' && (c & 0xC0) != 0x80) {
                if (remaining <= 0)
                    break;
                --remaining;
            }
            tmp[len++] = (char)c;
        }
        tmp[len] = '\0';
        DrawTextEx(mainFont, tmp, (Vector2){TEXT_X, y0 + (float)r * TEXT_LINE_H}, TEXT_SIZE, TEXT_SPACING, BLACK);
    }
}

/* ------------------------------------------------------------------------- */
/* Retratos                                                                  */
/* ------------------------------------------------------------------------- */

static void FreePortrait(Portrait *p)
{
    if (p->set.animations || p->set.texture.id)
        AnimationSet_FreeAll(&p->set);
    p->set = (RayAnimationHandler){0};
}

static void LoadPortraitAtlas(Portrait *p)
{
    const char *image = p->def.image[0] ? p->def.image : p->name;
    char rel[DIALOGUE_NAME_MAX + 32], png[640], xml[640];
    snprintf(rel, sizeof(rel), "images/dialogue/%s.png", image);
    if (!Dialogue_FindAsset(modRoot, rel, png, sizeof(png)))
        return;
    snprintf(rel, sizeof(rel), "images/dialogue/%s.xml", image);
    if (!Dialogue_FindAsset(modRoot, rel, xml, sizeof(xml)))
        return;
    Texture2D tex = Render_LoadTexture(png);
    if (!tex.id)
        return;
    if (!Atlas_LoadSparrowXML(xml, tex, &p->set)) {
        UnloadTexture(tex);
        p->set = (RayAnimationHandler){0};
        return;
    }
    if (p->def.noAntialiasing)
        SetTextureFilter(tex, TEXTURE_FILTER_POINT);
}

/* spawnCharacters da Psych: entra por fora da tela (esquerda, direita ou por baixo). */
static void PlacePortrait(Portrait *p)
{
    const float w = p->fw * p->scale;
    float x = LEFT_CHAR_X, y = DEFAULT_CHAR_Y;
    p->x = x + OFFSET_POS;
    p->y = y;
    bool saveY = false;
    if (p->def.pos == DIALOGUE_POS_CENTER) {
        p->x = SCREEN_W / 2.0f - w / 2.0f;
        y = p->y;
        p->y = SCREEN_H + 50.0f;
        saveY = true;
    } else if (p->def.pos == DIALOGUE_POS_RIGHT) {
        x = SCREEN_W - w + RIGHT_CHAR_X;
        p->x = x - OFFSET_POS;
    }
    x += p->def.position[0];
    y += p->def.position[1];
    p->x += p->def.position[0];
    p->y += p->def.position[1];
    p->startingPos = saveY ? y : x;
    p->alpha = 0.0f;
}

static void LoadPortrait(Portrait *p, const char *name)
{
    memset(p, 0, sizeof(*p));
    snprintf(p->name, sizeof(p->name), "%s", name);
    p->curAnim = -1;
    Player_Play(&p->play, -1, 24.0f, false);

    char rel[DIALOGUE_NAME_MAX + 32], path[640];
    snprintf(rel, sizeof(rel), "images/dialogue/%s.json", name);
    p->found = name[0] && Dialogue_FindAsset(modRoot, rel, path, sizeof(path)) && DialogueChar_Load(path, &p->def);
    p->hasDef = p->found;
    if (!p->found) {
        snprintf(rel, sizeof(rel), "images/dialogue/%s.json", DIALOGUE_DEFAULT_CHAR);
        p->hasDef = Dialogue_FindAsset(modRoot, rel, path, sizeof(path)) && DialogueChar_Load(path, &p->def);
        if (!p->hasDef)
            (void)DialogueChar_Load(NULL, &p->def);   /* so os padroes */
    }

    if (p->hasDef)
        LoadPortraitAtlas(p);
    for (int i = 0; i < p->def.animCount; ++i) {
        p->loopIdx[i] = FindPrefix(&p->set, p->def.anims[i].loopName);
        p->idleIdx[i] = FindPrefix(&p->set, p->def.anims[i].idleName);
    }

    /* largura de referencia: o primeiro quadro (Psych usa o 1o do atlas) */
    int ref = -1;
    if (p->def.animCount > 0)
        ref = p->idleIdx[0] >= 0 ? p->idleIdx[0] : p->loopIdx[0];
    if (ref < 0 && p->set.animationCount > 0)
        ref = 0;
    if (ref >= 0)
        Extents(&p->set.animations[ref], &p->fw, &p->fh);

    p->scale = CHAR_SCALE * p->def.scale;
    PlacePortrait(p);
}

static int FindOrLoadPortrait(const char *name)
{
    for (int i = 0; i < portraitCount; ++i) {
        if (strcmp(portraits[i].name, name) == 0)
            return i;
    }
    int slot = portraitCount;
    if (slot >= PORTRAIT_MAX) {
        if (file)
            return -1;   /* jogo: usa o primeiro, como a Psych */
        slot = lastCharacter == PORTRAIT_MAX - 1 ? PORTRAIT_MAX - 2 : PORTRAIT_MAX - 1;   /* editor: recicla */
        FreePortrait(&portraits[slot]);
        if (lastCharacter == slot)
            lastCharacter = -1;
    } else {
        ++portraitCount;
    }
    LoadPortrait(&portraits[slot], name);
    return slot;
}

/* playAnim da Psych. Sem a expressao: uma qualquer. Sem loop_name (ou igual ao idle): so idle. */
static void PortraitPlay(Portrait *p, const char *animName, bool playIdle)
{
    int a = -1;
    for (int i = 0; i < p->def.animCount && a < 0; ++i) {
        if (strcmp(p->def.anims[i].anim, animName) == 0)
            a = i;
    }
    if (a < 0 && p->def.animCount > 0)
        a = GetRandomValue(0, p->def.animCount - 1);
    if (a >= 0) {
        const DialogueCharAnim *d = &p->def.anims[a];
        if (!d->loopName[0] || strcmp(d->loopName, d->idleName) == 0)
            playIdle = true;
    }
    /* play(force = false): a mesma animacao rodando continua */
    if (a >= 0 && a == p->curAnim && playIdle == p->idle && p->play.anim >= 0 && !p->play.finished)
        return;
    p->curAnim = a;
    p->idle = playIdle;
    const int idx = a < 0 ? -1 : (playIdle ? p->idleIdx[a] : p->loopIdx[a]);
    Player_Play(&p->play, idx, 24.0f, playIdle);
}

static void DrawPortrait(const Portrait *p)
{
    if (p->alpha <= 0.001f || p->play.anim < 0 || !p->set.animations)
        return;
    const RayAnimation *a = &p->set.animations[p->play.anim];
    if (a->frameCount <= 0)
        return;
    const int frame = p->play.frame < a->frameCount ? p->play.frame : a->frameCount - 1;
    float ox = 0.0f, oy = 0.0f;
    if (p->curAnim >= 0) {
        const DialogueCharAnim *d = &p->def.anims[p->curAnim];
        ox = p->idle ? d->idleOffset[0] : d->loopOffset[0];
        oy = p->idle ? d->idleOffset[1] : d->loopOffset[1];
    }
    const float s = p->scale;
    const float left = p->x - ox + 0.5f * p->fw * (1.0f - s);
    const float top = p->y - oy + 0.5f * p->fh * (1.0f - s);
    DrawFrame(p->set.texture, &a->frames[frame], left, top, s, false, p->fw, Fade(WHITE, p->alpha));
}

/* Ativo entra (startingPos, alpha 1); os outros saem e somem. */
static void SlidePortraits(float dt)
{
    if (lastCharacter < 0)
        return;
    const float step = SCROLL_SPEED * dt;
    for (int i = 0; i < portraitCount; ++i) {
        Portrait *p = &portraits[i];
        if (i != lastCharacter) {
            switch (p->def.pos) {
                case DIALOGUE_POS_LEFT:   p->x = fmaxf(p->x - step, p->startingPos + OFFSET_POS); break;
                case DIALOGUE_POS_CENTER: p->y = fminf(p->y + step, p->startingPos + SCREEN_H); break;
                default:                  p->x = fminf(p->x + step, p->startingPos - OFFSET_POS); break;
            }
            p->alpha = fmaxf(p->alpha - 3.0f * dt, 0.0f);
        } else {
            switch (p->def.pos) {
                case DIALOGUE_POS_LEFT:   p->x = fminf(p->x + step, p->startingPos); break;
                case DIALOGUE_POS_CENTER: p->y = fmaxf(p->y - step, p->startingPos); break;
                default:                  p->x = fmaxf(p->x - step, p->startingPos); break;
            }
            p->alpha = fminf(p->alpha + 3.0f * dt, 1.0f);
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Bolha                                                                     */
/* ------------------------------------------------------------------------- */

static void LoadBubble(void)
{
    hasBubble = false;
    bubble = (RayAnimationHandler){0};
    boxW = boxH = 0.0f;
    char png[640], xml[640];
    if (Dialogue_FindAsset(modRoot, "images/speech_bubble.png", png, sizeof(png)) &&
        Dialogue_FindAsset(modRoot, "images/speech_bubble.xml", xml, sizeof(xml))) {
        Texture2D tex = Render_LoadTexture(png);
        if (tex.id && Atlas_LoadSparrowXML(xml, tex, &bubble))
            hasBubble = true;
        else if (tex.id)
            UnloadTexture(tex);
    }

    bool any = false;
    for (int i = 0; i < BOX_SLOTS; ++i) {
        boxIdx[i] = hasBubble ? FindPrefix(&bubble, kBoxPrefix[i]) : -1;
        if (boxIdx[i] >= 0) {
            Extents(&bubble.animations[boxIdx[i]], &boxW, &boxH);
            any = true;
        }
    }
    if (hasBubble && !any) {   /* atlas sem nenhuma animacao da bolha: usa a desenhada */
        AnimationSet_FreeAll(&bubble);
        bubble = (RayAnimationHandler){0};
        hasBubble = false;
    }
}

static int BoxAnim(int slot)
{
    int i = boxIdx[slot];
    if (i < 0) i = boxIdx[slot & ~BOX_CENTER];
    if (i < 0) i = boxIdx[slot & BOX_OPEN];
    if (i < 0) i = boxIdx[0];
    return i;
}

static void PlayBox(int slot)
{
    boxSlot = slot;
    Player_Play(&boxPlay, BoxAnim(slot), BOX_FPS, !(slot & BOX_OPEN));
    if (!hasBubble && (slot & BOX_OPEN))
        fallbackT = 0.0f;
}

static bool BoxFinished(void)
{
    return hasBubble ? boxPlay.finished : ((boxSlot & BOX_OPEN) && fallbackT >= 1.0f);
}

static DialoguePos SpeakerPos(void)
{
    return lastCharacter >= 0 ? portraits[lastCharacter].def.pos : DIALOGUE_POS_LEFT;
}

static void DrawFallbackBox(void)
{
    const float t = fallbackT;
    const float k = 1.0f - (1.0f - t) * (1.0f - t);   /* ease out */
    if (k <= 0.01f)
        return;
    const Rectangle full = {130.0f, 430.0f, 1020.0f, 250.0f};
    const float cx = full.x + full.width * 0.5f, cy = full.y + full.height * 0.5f;
    const Rectangle r = {cx - full.width * k * 0.5f, cy - full.height * k * 0.5f, full.width * k, full.height * k};
    const float thick = (boxSlot & BOX_ANGRY) ? 11.0f : 6.0f;
    const Rectangle in = {r.x + thick, r.y + thick, r.width - thick * 2.0f, r.height - thick * 2.0f};

    /* rabicho apontando para quem fala */
    const DialoguePos pos = SpeakerPos();
    const float bx = pos == DIALOGUE_POS_LEFT ? r.x + r.width * 0.2f
                   : pos == DIALOGUE_POS_RIGHT ? r.x + r.width * 0.8f : cx;
    const float lean = pos == DIALOGUE_POS_LEFT ? -48.0f : pos == DIALOGUE_POS_RIGHT ? 48.0f : 0.0f;
    const float baseY = r.y + thick * 2.0f, tipY = r.y - 64.0f * k, half = 36.0f * k;

    DrawTriangle((Vector2){bx + lean * k, tipY - thick * 1.4f}, (Vector2){bx - half - thick, baseY},
                 (Vector2){bx + half + thick, baseY}, BLACK);
    DrawRectangleRounded(r, 0.3f, 16, BLACK);
    DrawRectangleRounded(in, 0.3f, 16, WHITE);
    DrawTriangle((Vector2){bx + lean * k, tipY}, (Vector2){bx - half, baseY + 1.0f},
                 (Vector2){bx + half, baseY + 1.0f}, WHITE);
}

static void DrawBox(void)
{
    if (!hasBubble) {
        DrawFallbackBox();
        return;
    }
    if (boxPlay.anim < 0)
        return;
    const RayAnimation *a = &bubble.animations[boxPlay.anim];
    if (a->frameCount <= 0)
        return;
    const int frame = boxPlay.frame < a->frameCount ? boxPlay.frame : a->frameCount - 1;

    /* updateBoxOffsets */
    float ox = 10.0f, oy = 0.0f;
    if ((boxSlot & (BOX_ANGRY | BOX_CENTER)) == BOX_ANGRY) { ox = 50.0f; oy = 65.0f; }
    else if ((boxSlot & (BOX_ANGRY | BOX_CENTER)) == (BOX_ANGRY | BOX_CENTER)) { ox = 50.0f; oy = 30.0f; }
    if (!boxFlip)
        oy += 10.0f;
    const float left = BOX_X - ox + 0.5f * boxW * (1.0f - BOX_SCALE);
    const float top = BOX_Y - oy + 0.5f * boxH * (1.0f - BOX_SCALE);
    DrawFrame(bubble.texture, &a->frames[frame], left, top, BOX_SCALE, boxFlip, boxW, WHITE);
}

/* ------------------------------------------------------------------------- */
/* Falas                                                                     */
/* ------------------------------------------------------------------------- */

/* startNextDialog */
static void StartLine(const DialogueLine *src)
{
    cur = *src;
    if (!cur.text[0])
        snprintf(cur.text, sizeof(cur.text), " ");
    if (!(cur.speed == cur.speed))   /* NaN */
        cur.speed = DIALOGUE_DEFAULT_SPEED;
    const bool angry = strcmp(cur.boxState, "angry") == 0;

    int character = FindOrLoadPortrait(cur.portrait);
    if (character < 0)
        character = portraitCount > 0 ? 0 : -1;
    const DialoguePos pos = character >= 0 ? portraits[character].def.pos : DIALOGUE_POS_LEFT;
    const int center = pos == DIALOGUE_POS_CENTER ? BOX_CENTER : 0;
    const int angryBit = angry ? BOX_ANGRY : 0;

    boxVisible = true;
    if (character != lastCharacter) {
        PlayBox(center | BOX_OPEN | angryBit);
        boxFlip = pos == DIALOGUE_POS_LEFT;
    } else if (angry != lastAngry) {
        PlayBox(center | angryBit);
    }
    lastCharacter = character;
    lastAngry = angry;

    SetText(cur.text, cur.speed);
    lineSound = SfxFor(cur.sound[0] ? cur.sound : "dialogue");
    if (lineSound < 0)
        lineSound = SfxFor("dialogue");   /* som da fala nao existe: usa o padrao */

    if (character >= 0) {
        Portrait *p = &portraits[character];
        PortraitPlay(p, cur.expression, textDone);
        p->play.fps = fminf(fmaxf(24.0f - ((cur.speed - 0.05f) / 5.0f) * 480.0f, 12.0f), 48.0f);
    }
}

static void BeginEnding(void)
{
    ended = true;
    boxSlot |= BOX_OPEN;
    if (hasBubble) {
        Player_Play(&boxPlay, BoxAnim(boxSlot), BOX_FPS, false);
        if (boxPlay.anim >= 0)
            boxPlay.frame = bubble.animations[boxPlay.anim].frameCount - 1;
        boxPlay.reverse = true;
    }
}

/* ------------------------------------------------------------------------- */
/* API                                                                       */
/* ------------------------------------------------------------------------- */

void DialogueBox_Begin(const char *root)
{
    DialogueBox_End();
    snprintf(modRoot, sizeof(modRoot), "%s", root ? root : "");
    active = true;
    ended = finished = false;
    file = NULL;
    currentText = 0;
    lastCharacter = -1;
    lastAngry = false;
    bgAlive = true;
    bgAlpha = 0.0f;
    boxAlive = true;
    boxVisible = false;
    boxFlip = false;
    boxSlot = 0;
    fallbackT = 0.0f;
    rowCount = 0;
    textDone = true;
    LoadBubble();
    Player_Play(&boxPlay, BoxAnim(0), BOX_FPS, true);
    closeSound = SfxFor("dialogueClose");
}

void DialogueBox_Start(const DialogueFile *f)
{
    if (!active || !f)
        return;
    file = f;
    currentText = 0;
    for (int i = 0; i < f->count; ++i)
        (void)FindOrLoadPortrait(f->lines[i].portrait);
    if (f->count <= 0) {
        BeginEnding();
        return;
    }
    StartLine(&f->lines[currentText++]);
}

void DialogueBox_ShowLine(const DialogueLine *line, bool instant)
{
    if (!active || ended || !line)
        return;
    StartLine(line);
    if (instant)
        FinishText();
}

bool DialogueBox_Update(float dt, bool accept, bool skip)
{
    if (!active || finished)
        return true;
    if (dt > 0.1f)
        dt = 0.1f;   /* travada: sem pular a abertura inteira */
    if (!file)
        skip = false;   /* editor */

    if (!ended) {
        bgAlpha = fminf(bgAlpha + 0.5f * dt, 0.5f);

        Portrait *speaker = lastCharacter >= 0 ? &portraits[lastCharacter] : NULL;
        if (accept || skip) {
            if (!textDone && !skip)
                FinishText();
            else if (file && (skip || currentText >= file->count))
                BeginEnding();
            else if (file)
                StartLine(&file->lines[currentText++]);
            SfxPlay(closeSound);
        } else if (textDone) {
            if (speaker && !speaker->idle && speaker->play.finished && speaker->curAnim >= 0)
                PortraitPlay(speaker, speaker->def.anims[speaker->curAnim].anim, true);
        } else if (speaker && speaker->play.finished) {
            Player_Restart(&speaker->play);
        }

        if (!ended && BoxFinished())
            PlayBox(boxSlot & ~BOX_OPEN);
        if (!ended)
            SlidePortraits(dt);
    } else {
        if (boxAlive && (hasBubble ? (boxPlay.anim < 0 || boxPlay.frame <= 0) : fallbackT <= 0.0f))
            boxAlive = false;
        if (bgAlive) {
            bgAlpha -= 0.5f * dt;
            if (bgAlpha <= 0.0f) {
                bgAlpha = 0.0f;
                bgAlive = false;
            }
        }
        const float step = SCROLL_SPEED * dt;
        for (int i = 0; i < portraitCount; ++i) {
            Portrait *p = &portraits[i];
            if (p->def.pos == DIALOGUE_POS_LEFT) p->x -= step;
            else if (p->def.pos == DIALOGUE_POS_CENTER) p->y += step;
            else p->x += step;
            p->alpha = fmaxf(p->alpha - 10.0f * dt, 0.0f);
        }
        if (!boxAlive && !bgAlive) {
            finished = true;
            return true;
        }
    }

    /* membros (super.update da Flixel) */
    if (boxAlive) {
        if (hasBubble)
            Player_Update(&boxPlay, &bubble, dt);
        else if (boxVisible)
            fallbackT = fminf(fmaxf(fallbackT + (ended ? -dt : dt) / FALLBACK_OPEN_SECS, 0.0f), 1.0f);
    }
    for (int i = 0; i < portraitCount; ++i)
        Player_Update(&portraits[i].play, &portraits[i].set, dt);
    if (!ended)
        TypeText(dt);
    return false;
}

void DialogueBox_Draw(void)
{
    if (!active || finished)
        return;
    if (bgAlive && bgAlpha > 0.0f)   /* maior que a tela: cobre o zoom da camera do HUD */
        DrawRectangle(-640, -360, 2560, 1440, Fade(WHITE, bgAlpha));
    for (int i = 0; i < portraitCount; ++i)
        DrawPortrait(&portraits[i]);
    if (boxAlive && boxVisible)
        DrawBox();
    if (ended || !boxVisible)
        return;
    DrawTypedText();

    if (file) {
        static const char *const skipText = "Press BACK to Skip";
        const float size = 20.0f;
        const Vector2 m = MeasureTextEx(mainFont, skipText, size, 1.0f);
        const Vector2 at = {SCREEN_W - 20.0f - m.x, SCREEN_H - 34.0f};
        for (int i = 0; i < 4; ++i) {
            const Vector2 o = {at.x + (i & 1 ? 2.0f : -2.0f), at.y + (i & 2 ? 2.0f : -2.0f)};
            DrawTextEx(mainFont, skipText, o, size, 1.0f, BLACK);
        }
        DrawTextEx(mainFont, skipText, at, size, 1.0f, WHITE);
    }
}

void DialogueBox_End(void)
{
    for (int i = 0; i < portraitCount; ++i)
        FreePortrait(&portraits[i]);
    portraitCount = 0;
    if (hasBubble)
        AnimationSet_FreeAll(&bubble);
    bubble = (RayAnimationHandler){0};
    hasBubble = false;
    SfxFreeAll();
    active = false;
    file = NULL;
    lastCharacter = -1;
}

const DialogueCharDef *DialogueBox_Portrait(const char *name, bool *found)
{
    if (found)
        *found = false;
    if (!active || !name)
        return NULL;
    const int i = FindOrLoadPortrait(name);
    if (i < 0)
        return NULL;
    if (found)
        *found = portraits[i].found;
    return portraits[i].hasDef ? &portraits[i].def : NULL;
}

bool DialogueBox_HasBubble(void)
{
    return hasBubble;
}

bool DialogueBox_TextDone(void)
{
    return textDone;
}
