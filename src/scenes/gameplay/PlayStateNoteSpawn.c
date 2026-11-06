/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateNoteSpawn.c - notas da musica: carregar, atualizar e desenhar.
 */

#include "scenes/gameplay/PlayStateNoteSpawn.h"
#include <math.h>
#include <stdlib.h>

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

    notes = malloc(sizeof(Note) * noteCount);
    size_t noteIndex = 0;

    for (size_t i = 0; i < song->sectionCount; i++) {
        for (size_t j = 0; j < song->sections[i].noteCount; j++) {
            Note_Load(notes + noteIndex, song->sections[i].notes + j);
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

void PlayStateNoteSpawn_Unload(void) {
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
    if (game_options_scroll == 2 && !note->mustHit) return 0.4f;
    if (game_options_scroll == 1) return 0.85f;
    return NOTE_SCALE;
}

static void SetAnimIfChanged(RayAnimatedObject* object, int anim) {
    if (!object->currentAnim.isValid || object->currentAnim.animationIndex != anim)
        AnimatedObject_SetAnimation(object, anim);
}

/* Sustain ainda nao pressionado/perdido: desenha inteiro, sem clip. */
static void DrawSustainFull(char fullyAbove) {
    if (fullyAbove) return;
    Render_DrawAnimatedObject(&trail);
    Render_DrawAnimatedObject(&end);
}

/* Sustain em andamento: corta a parte que ja passou do receptor. */
static void DrawSustainClipped(const StrumNote* strum, float k) {
    const float receptorY = strum->object.position.y + trailOffset.y * k;
    const float trailBottom = trail.position.y + trail.scaleY * TRAIL_SEGMENT_H;

    if (game_options_scroll == 1) {
        if (trailBottom > receptorY)
            trail.scaleY = fmaxf(0.0f, (receptorY - trail.position.y) / TRAIL_SEGMENT_H);
    } else if (trail.position.y < receptorY) {
        trail.scaleY = fmaxf(0.0f, (trailBottom - receptorY) / TRAIL_SEGMENT_H);
        trail.position.y = receptorY;
    }

    if (trail.scaleY > 0.0f)
        Render_DrawAnimatedObject(&trail);
    if (trailBottom >= receptorY)
        Render_DrawAnimatedObject(&end);
}

/* Posiciona, anima, processa hold/miss e desenha o sustain da nota. */
static void DrawSustain(Note* note, const StrumNote* strum, const PlayStateNoteView* view,
                        const char* holdDown, float scrollSpeed, float endHeight, char fullyAbove) {
    // Middlescroll: sustain do oponente acompanha a escala menor do receptor.
    const float k = (game_options_scroll == 2 && !note->mustHit) ? (0.4f / NOTE_SCALE) : 1.0f;

    SetAnimIfChanged(&trail, Note_TrailAnimation(note->id));
    trail.scaleX = end.scaleX = NOTE_SCALE * k;
    trail.position.x = strum->object.position.x + trailOffset.x * k;
    trail.position.y = renderNote.position.y + trailOffset.y * k;
    trail.scaleY = fmaxf(0.01f, (scrollSpeed * note->length - endHeight) / TRAIL_SEGMENT_H);
    trail.color = normalColor;

    SetAnimIfChanged(&end, Note_EndAnimation(note->id));
    end.position.x = trail.position.x;
    end.position.y = trail.position.y + trail.scaleY * TRAIL_SEGMENT_H;
    end.scaleY = endHeight / END_SEGMENT_H;
    end.color = normalColor;

    if (note->missed) {
        trail.color = end.color = missedColor;
        DrawSustainFull(fullyAbove);
        return;
    }

    const char shouldHold = Note_ShouldHold(note, view->time);

    if (!note->mustHit) {
        if (shouldHold) events.opponentHold();
    }
    else if (game_options_botplay) {
        if (shouldHold) events.botHold();
    }
    else if (!note->pressed) {
        DrawSustainFull(fullyAbove);
        return;
    }
    else if (!view->paused && shouldHold) {
        const float tailLeft = (note->time + note->length) - view->time;
        if (!holdDown[note->id] && tailLeft > 0.12f) {
            events.miss(note);
            DrawSustainFull(fullyAbove);
            return;
        }
        events.playerHold();
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

void PlayStateNoteSpawn_Draw(const PlayStateNoteView* view) {
    rlSetBlendFactorsSeparate(0x0302, 0x0303, 1, 0x0303, 0x8006, 0x8006);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);

    const float time = view->time;
    const float speedMult = view->scrollVelocity * view->songSpeed;
    const float scrollSpeed = 450.0f * view->songSpeed;
    const float cutoffTime = time + (900.0f / scrollSpeed);
    const float endHeight = fmaxf(END_SEGMENT_H, 18.0f * view->songSpeed);

    char holdDown[4];
    for (int k = 0; k < 4; k++)
        holdDown[k] = game_options_botplay || IsKeyDown(game_scheme_keys[game_options_scheme][k]);

    CullPassedNotes(time, 400.0f / scrollSpeed);

    for (size_t i = firstNote; i < noteCount; i++) {
        Note* note = notes + i;

        if (note->id < 0 || note->id > 3)
            continue;

        if (note->time > cutoffTime)
            break;

        const StrumNote* strum = (note->mustHit ? view->bfStrums : view->dadStrums) + note->id;

        renderNote.position.x = strum->object.position.x;
        renderNote.position.y = strum->object.position.y - speedMult * (note->time - time);

        const char fullyAbove = (game_options_scroll == 1) ? (renderNote.position.y > 900.0f)
                                                           : (renderNote.position.y < -200.0f);

        if (note->pressed && fullyAbove && time >= note->time + note->length)
            continue;

        SetAnimIfChanged(&renderNote, Note_Animation(note->id));

        // Oponente e botplay acertam exatamente no tempo da nota.
        if (!note->pressed && time >= note->time) {
            if (!note->mustHit) {
                note->pressed = 1;
                events.opponentHit(note);
            } else if (game_options_botplay) {
                note->pressed = 1;
                events.botHit(note);
            }
        }

        if (note->length > 0)
            DrawSustain(note, strum, view, holdDown, scrollSpeed, endHeight, fullyAbove);

        if (!note->pressed) {
            if (!note->missed && Note_TooLate(note, time))
                events.miss(note);

            renderNote.scaleX = renderNote.scaleY = HeadScale(note);
            Render_DrawAnimatedObject(&renderNote);
        }
    }

    EndBlendMode();
}
