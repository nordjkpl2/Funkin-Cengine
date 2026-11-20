/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateNoteSpawn.c - notas da musica: carregar, atualizar e desenhar.
 */

#include "scenes/api/NoteSkinCache.h"
#include "scenes/gameplay/PlayStateNoteSpawn.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "GameData.h"
#include "rlgl.h"

static RayAnimatedObject trail;
static RayAnimatedObject end;
static RayAnimatedObject renderNote;

static Note* notes;
static size_t noteCount;
static size_t firstNote;

static PlayStateNoteEvents events;

// sustain colors
static const Color normalColor = {255, 255, 255, 200};
static const Color missedColor = {150, 150, 150, 200};
static const Vector2 trailOffset = {40, 60};

#define NOTE_SCALE 0.7f
#define NOTE_OCC_TOP 300                    /* margem acima da tela (px) */
#define NOTE_OCC_ROWS (300 + 720 + 300)     /* linhas de pixel cobertas pela grade de ocupacao */
static unsigned short occupied[8][NOTE_OCC_ROWS];  /* (lado, casa) x linha de pixel; marcada com a geracao do Draw */
static unsigned short occupiedGen = 0;             /* geracao atual; evita memset por frame */
#define TRAIL_SEGMENT_H 44.0f /* altura do sprite do sustain na escala 1 */
#define END_SEGMENT_H 64.0f   /* altura do sprite da ponta na escala 1 */

// Ordena por tempo (qsort): garante notas em ordem para o culling.
static int Note_CompareChronological(const void* a, const void* b) {
    float diff = ((const Note*)a)->time - ((const Note*)b)->time;
    if (diff < 0) return -1;
    if (diff > 0) return 1;
    return 0;
}

void PlayStateNoteSpawn_Init(void) {
    Render_DefaultAnimated(&trail);
    trail.animationSet = Cache_GetNoteAnimations();
    trail.scaleX = 0.7f;

    Render_DefaultAnimated(&end);
    end.animationSet = Cache_GetNoteAnimations();
    end.scaleX = 0.7f;
    end.scaleY = 0.7f;

    Render_DefaultAnimated(&renderNote);
    renderNote.animationSet = Cache_GetNoteAnimations();
    renderNote.scaleX = 0.7f;
    renderNote.scaleY = 0.7f;
}

void PlayStateNoteSpawn_SetEvents(const PlayStateNoteEvents* newEvents) {
    events = *newEvents;
}

void PlayStateNoteSpawn_Load(const Song* song) {
    firstNote = 0;
    noteCount = 0;
    for (size_t i = 0; i < song->sectionCount; i++)
        noteCount += song->sections[i].noteCount;

    notes = NULL;
    if (noteCount == 0) return;

    notes = malloc(sizeof(Note) * noteCount);
    if (notes == NULL) {
        noteCount = 0;
        return;
    }
    size_t noteIndex = 0;

    for (size_t i = 0; i < song->sectionCount; i++) {
        for (size_t j = 0; j < song->sections[i].noteCount; j++) {
            Note_Load(notes + noteIndex, song->sections[i].notes + j);
            notes[noteIndex].sectionMustHit = song->sections[i].mustHit;
            noteIndex++;
        }
    }

    // Ordenar e critico: o culling e o break do Draw assumem ordem cronologica.
    if (noteCount > 0) {
        qsort(notes, noteCount, sizeof(Note), Note_CompareChronological);
    }
}

void PlayStateNoteSpawn_Reset(void) {
    firstNote = 0;

    for (size_t i = 0; i < noteCount; i++) {
        Note* note = notes + i;
        note->missed = 0;
        note->pressed = 0;
    }
}

static void UnloadRecolorShader(void);

void PlayStateNoteSpawn_Unload(void) {
    UnloadRecolorShader();
    free(notes);
    notes = NULL;
    noteCount = 0;
    firstNote = 0;
}

Note* PlayStateNoteSpawn_Notes(void) { return notes; }
size_t PlayStateNoteSpawn_Count(void) { return noteCount; }
size_t PlayStateNoteSpawn_First(void) { return firstNote; }

/* Escala da cabeca da nota conforme o modo de scroll. */
static float HeadScale(const Note* note) {
    if (gameOptions.scroll == 2 && !note->mustHit) return 0.4f;
    return NOTE_SCALE;
}

static void SetAnimIfChanged(RayAnimatedObject* object, int anim) {
    if (!object->currentAnim.isValid || object->currentAnim.animationIndex != anim)
        AnimatedObject_SetAnimation(object, anim);
}

/* Sustain ainda nao pressionado/perdido: desenha inteiro, sem clip. */
/* Recolor da skin: troca so o matiz de cada pixel (saturacao e brilho originais ficam),
   assim um amarelo nao vira marrom escuro como aconteceria com um simples multiplicador. */
static const char* const recolorFS =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "uniform vec3 targetHSV;\n"   /* matiz 0..1, saturacao 0..1, brilho 0..1 */
    "out vec4 finalColor;\n"
    "vec3 rgb2hsv(vec3 c) {\n"
    "    vec4 K = vec4(0.0, -1.0/3.0, 2.0/3.0, -1.0);\n"
    "    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));\n"
    "    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));\n"
    "    float d = q.x - min(q.w, q.y);\n"
    "    float e = 1.0e-10;\n"
    "    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);\n"
    "}\n"
    "vec3 hsv2rgb(vec3 c) {\n"
    "    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);\n"
    "    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);\n"
    "    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);\n"
    "}\n"
    "void main() {\n"
    "    vec4 texel = texture(texture0, fragTexCoord) * colDiffuse * fragColor;\n"
    "    vec3 hsv = rgb2hsv(texel.rgb);\n"
    "    hsv.x = targetHSV.x;\n"
    "    hsv.y *= targetHSV.y;\n"
    "    hsv.z *= targetHSV.z;\n"
    "    finalColor = vec4(hsv2rgb(hsv), texel.a);\n"
    "}\n";

static Shader recolorShader;
static int recolorLoc = -1;
static bool recolorReady;
static Color noteTint = {255, 255, 255, 255}; /* cor da coluna da nota que esta sendo desenhada */

static void EnsureRecolorShader(void) {
    if (recolorReady) return;
    recolorShader = LoadShaderFromMemory(NULL, recolorFS);
    recolorLoc = GetShaderLocation(recolorShader, "targetHSV");
    recolorReady = true;
}

/* Desenha um objeto de nota com a cor da coluna (branco = skin original, sem shader). */
static void DrawNoteObj(RayAnimatedObject* obj) {
    if (noteTint.r == 255 && noteTint.g == 255 && noteTint.b == 255) {
        Render_DrawAnimatedObject(obj);
        return;
    }
    EnsureRecolorShader();
    const Vector3 hsv = ColorToHSV(noteTint);
    const float target[3] = {hsv.x / 360.0f, hsv.y, hsv.z};
    rlDrawRenderBatchActive();   /* o lote pendente usa o uniform antigo */
    SetShaderValue(recolorShader, recolorLoc, target, SHADER_UNIFORM_VEC3);
    BeginShaderMode(recolorShader);
    Render_DrawAnimatedObject(obj);
    EndShaderMode();
}

static void UnloadRecolorShader(void) {
    if (!recolorReady) return;
    UnloadShader(recolorShader);
    recolorReady = false;
}

/* Menu de opcoes (preview): mesmo desenho de nota com a cor da coluna. */
void PlayStateNoteSpawn_DrawTinted(RayAnimatedObject* obj, Color tint) {
    noteTint = tint;
    DrawNoteObj(obj);
}

void PlayStateNoteSpawn_ReleaseTint(void) {
    UnloadRecolorShader();
}

static void DrawSustainFull(char fullyAbove) {
    if (fullyAbove) return;
    DrawNoteObj(&trail);
    DrawNoteObj(&end);
}

/* Sustain em andamento: corta a parte que ja passou do receptor. */
static void DrawSustainClipped(const StrumNote* strum, float k) {
    const float receptorY = strum->object.position.y + trailOffset.y * k;

    if (gameOptions.scroll == 1) {
        // Downscroll: a cauda fica acima da cabeca; o que ja passou do receptor (abaixo dele) some.
        const float trailBottom = trail.position.y + trail.scaleY * TRAIL_SEGMENT_H;
        if (trailBottom > receptorY)
            trail.scaleY = fmaxf(0.0f, (receptorY - trail.position.y) / TRAIL_SEGMENT_H);
        if (trail.scaleY > 0.0f)
            DrawNoteObj(&trail);
        if (end.position.y < receptorY)
            DrawNoteObj(&end);
        return;
    }

    const float trailBottom = trail.position.y + trail.scaleY * TRAIL_SEGMENT_H;
    if (trail.position.y < receptorY) {
        trail.scaleY = fmaxf(0.0f, (trailBottom - receptorY) / TRAIL_SEGMENT_H);
        trail.position.y = receptorY;
    }

    if (trail.scaleY > 0.0f)
        DrawNoteObj(&trail);
    if (trailBottom >= receptorY)
        DrawNoteObj(&end);
}

/* Tecla da lane segurada neste frame (botplay conta como sempre segurada). So e consultada quando ha sustain ativo. */
static char LaneHeld(int lane) {
    return gameOptions.botplay || IsKeyDown(game_scheme_keys[gameOptions.scheme][lane]);
}

/* Logica do sustain (hold/miss). Sem desenho: roda no Update. */
static void UpdateSustain(Note* note, const PlayStateNoteView* view) {
    if (note->missed) return;

    const char shouldHold = Note_ShouldHold(note, view->time);

    if (!note->mustHit) {
        if (shouldHold) events.opponentHold();
    }
    else if (gameOptions.botplay) {
        if (shouldHold) events.botHold();
    }
    else if (!note->pressed) {
        return;
    }
    else if (!view->paused && shouldHold) {
        const float tailLeft = (note->time + note->length) - view->time;
        if (tailLeft > 0.12f && !LaneHeld(note->id))
            events.miss(note);
        else
            events.playerHold();
    }
}

/* Posiciona, anima e desenha o sustain da nota (so le o estado). */
static void DrawSustain(const Note* note, const StrumNote* strum, float endHeight,
                        float scrollSpeed, char fullyAbove) {
    // Middlescroll: sustain do oponente acompanha a escala menor do receptor.
    const float k = (gameOptions.scroll == 2 && !note->mustHit) ? (0.4f / NOTE_SCALE) : 1.0f;

    const float lengthPx = scrollSpeed * note->length;
    const float headY = renderNote.position.y + trailOffset.y * k;

    SetAnimIfChanged(&trail, Note_TrailAnimation(note->id));
    trail.scaleX = end.scaleX = NOTE_SCALE * k;
    trail.position.x = strum->object.position.x + trailOffset.x * k;
    trail.scaleY = fmaxf(0.01f, (lengthPx - endHeight) / TRAIL_SEGMENT_H);
    noteTint = GameData_LaneTint(note->id);
    trail.color = normalColor;

    SetAnimIfChanged(&end, Note_EndAnimation(note->id));
    end.position.x = trail.position.x;
    end.scaleY = endHeight / END_SEGMENT_H;
    end.color = normalColor;

    // Cauda vai no sentido do futuro: para baixo no normal, para cima no downscroll.
    if (gameOptions.scroll == 1) {
        end.position.y = headY - lengthPx;
        trail.position.y = end.position.y + endHeight;
    } else {
        trail.position.y = headY;
        end.position.y = trail.position.y + trail.scaleY * TRAIL_SEGMENT_H;
    }

    if (note->missed) {
        trail.color = end.color = missedColor;
        DrawSustainFull(fullyAbove);
        return;
    }

    // Jogador ainda nao acertou a cabeca: sustain inteiro, sem corte.
    if (note->mustHit && !gameOptions.botplay && !note->pressed) {
        DrawSustainFull(fullyAbove);
        return;
    }

    DrawSustainClipped(strum, k);
}

/* Avanca firstNote sobre notas ja resolvidas e bem atras do receptor. */
static void CullPassedNotes(float time, float cullTimeThreshold) {
    while (firstNote < noteCount) {
        const Note* n = notes + firstNote;
        const char resolved = n->id < 0 || n->id > 3 || n->pressed || n->missed;
        if (!resolved || time - (n->time + n->length) <= cullTimeThreshold)
            break;
        firstNote++;
    }
}

/* Culling + hits automaticos + hold/miss. Sem desenho; roda no Update, depois do input. */
void PlayStateNoteSpawn_Update(const PlayStateNoteView* view) {
    const float time = view->time;
    const float scrollSpeed = 450.0f * view->songSpeed;
    const float cutoffTime = time + (900.0f / scrollSpeed);

    CullPassedNotes(time, 400.0f / scrollSpeed);

    for (size_t i = firstNote; i < noteCount; i++) {
        Note* note = notes + i;

        if (note->id < 0 || note->id > 3)
            continue;

        if (note->time > cutoffTime)
            break;

        // Oponente e botplay acertam exatamente no tempo da nota.
        if (!note->pressed && time >= note->time) {
            if (!note->mustHit) {
                note->pressed = 1;
                events.opponentHit(note);
            } else if (gameOptions.botplay) {
                note->pressed = 1;
                events.botHit(note);
            }
        }

        if (note->length > 0)
            UpdateSustain(note, view);

        if (!note->pressed && !note->missed && Note_TooLate(note, time))
            events.miss(note);
    }
}

/* So desenha o estado atual das notas (cabeca, sustain e ponta). Dentro da camera do HUD. */
void PlayStateNoteSpawn_Draw(const PlayStateNoteView* view) {
    rlSetBlendFactorsSeparate(0x0302, 0x0303, 1, 0x0303, 0x8006, 0x8006);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);

    const float time = view->time;
    const float speedMult = view->scrollVelocity * view->songSpeed;
    const float scrollSpeed = 450.0f * view->songSpeed;
    const float cutoffTime = time + (900.0f / scrollSpeed);
    const float endHeight = fmaxf(END_SEGMENT_H, 18.0f * view->songSpeed);

    if (++occupiedGen == 0) {   /* wraparound: limpa uma vez a cada 65535 frames */
        memset(occupied, 0, sizeof(occupied));
        occupiedGen = 1;
    }

    for (size_t i = firstNote; i < noteCount; i++) {
        const Note* note = notes + i;

        if (note->id < 0 || note->id > 3)
            continue;

        if (note->time > cutoffTime)
            break;

        // Hidden Notes: so desenha as notas de quem esta cantando na secao. So muda o desenho; o hit segue igual.
        if (!GameData_HiddenShowsSide(note->mustHit, note->sectionMustHit))
            continue;

        const StrumNote* strum = (note->mustHit ? view->bfStrums : view->dadStrums) + note->id;

        renderNote.position.x = strum->object.position.x;
        renderNote.position.y = strum->object.position.y - speedMult * (note->time - time);

        const char fullyAbove = (gameOptions.scroll == 1) ? (renderNote.position.y > 900.0f)
                                                           : (renderNote.position.y < -200.0f);

        if (note->pressed && fullyAbove && time >= note->time + note->length)
            continue;

        /* Cabeca curta fora da tela, ou empilhada no mesmo pixel de outra ja desenhada (spam gigante):
           nao gera draw call. O custo do desenho fica limitado a ~8 casas x altura da tela. */
        if (note->length <= 0.0f) {
            const float yf = renderNote.position.y;
            if (!(yf > -(float)NOTE_OCC_TOP && yf < (float)(NOTE_OCC_ROWS - NOTE_OCC_TOP)))
                continue;   /* fora da tela (ou NaN) */
            const int row = (int)yf + NOTE_OCC_TOP;
            if (note->pressed)
                continue;
            unsigned short* cell = &occupied[(note->mustHit ? 4 : 0) + note->id][row];
            if (*cell == occupiedGen)
                continue;
            *cell = occupiedGen;
        }

        SetAnimIfChanged(&renderNote, Note_Animation(note->id));

        if (note->length > 0)
            DrawSustain(note, strum, endHeight, scrollSpeed, fullyAbove);

        if (!note->pressed) {
            renderNote.scaleX = renderNote.scaleY = HeadScale(note);
            noteTint = GameData_LaneTint(note->id);
            renderNote.color = WHITE;
            DrawNoteObj(&renderNote);
        }
    }

    EndBlendMode();
}
