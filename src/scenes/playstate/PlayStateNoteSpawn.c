/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateNoteSpawn.c - notas da musica: carregar, atualizar e desenhar.
 */

#include "scenes/playstate/PlayStateNoteSpawn.h"
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

// Função de ordenação para qsort (Garante que nunca ocorra bug de notas fora de ordem)
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

    // ORDENAÇÃO CRÍTICA: Resolve bug de notas passando do receptor e culling prematuro!
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

void PlayStateNoteSpawn_Draw(const PlayStateNoteView* view) {
    rlSetBlendFactorsSeparate(0x0302, 0x0303, 1, 0x0303, 0x8006, 0x8006);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);

    const float songSpeed = view->songSpeed;
    const float scrollVel = view->scrollVelocity;
    const float speedMult = scrollVel * songSpeed;
    const float scrollSpeed = 450.0f * songSpeed;
    const float cutoffTime = view->time + (900.0f / scrollSpeed);
    const float cullTimeThreshold = 400.0f / scrollSpeed;
    const float endHeight = fmaxf(64.0f, 18.0f * songSpeed);

    char holdDown[4];
    for (int k = 0; k < 4; k++)
        holdDown[k] = game_options_botplay || IsKeyDown(game_scheme_keys[game_options_scheme][k]);

    while (firstNote < noteCount) {
        Note* n = notes + firstNote;
        char resolved = (n->id < 0 || n->id > 3) || n->pressed || n->missed;
        float timePast = view->time - (n->time + n->length);
        if (resolved && timePast > cullTimeThreshold) {
            firstNote++;
        } else {
            break;
        }
    }

    for (size_t i = firstNote; i < noteCount; i++) {
        Note* note = notes + i;

        if (note->id < 0 || note->id > 3)
            continue;

        if (note->time > cutoffTime)
            break;

        const StrumNote* targetStrumNote = (note->mustHit ? view->bfStrums : view->dadStrums) + note->id;
        float timeDiff = note->time - view->time;

        renderNote.position.x = targetStrumNote->object.position.x;
        renderNote.position.y = targetStrumNote->object.position.y - (speedMult * timeDiff);

        char fullyAbove = (game_options_scroll == 1) ? (renderNote.position.y > 900.0f)
                                                     : (renderNote.position.y < -200.0f);

        if (note->pressed && fullyAbove && view->time >= note->time + note->length)
            continue;

        int headAnim = Note_Animation(note->id);
        if (!renderNote.currentAnim.isValid || renderNote.currentAnim.animationIndex != headAnim)
            AnimatedObject_SetAnimation(&renderNote, headAnim);

        // --- ACERTO DO OPONENTE ---
        if (!note->mustHit && !note->pressed && view->time >= note->time) {
            note->pressed = 1;
            events.opponentHit(note);
        }

        // --- ACERTO DO BOTPLAY (TOCA EXATAMENTE COMO O OPONENTE NO RECEPTOR) ---
        if (note->mustHit && game_options_botplay && !note->pressed && view->time >= note->time) {
            note->pressed = 1;
            events.botHit(note);
        }

        if (note->length > 0) {
            int trailAnim = Note_TrailAnimation(note->id);
            if (!trail.currentAnim.isValid || trail.currentAnim.animationIndex != trailAnim)
                AnimatedObject_SetAnimation(&trail, trailAnim);
            trail.position.x = targetStrumNote->object.position.x + trailOffset.x;
            trail.position.y = renderNote.position.y + trailOffset.y;
            trail.scaleY = fmaxf(0.01f, (scrollSpeed * note->length - endHeight) / 44.0f);
            trail.color = normalColor;

            int endAnim = Note_EndAnimation(note->id);
            if (!end.currentAnim.isValid || end.currentAnim.animationIndex != endAnim)
                AnimatedObject_SetAnimation(&end, endAnim);
            end.position.x = trail.position.x;
            end.position.y = trail.position.y + trail.scaleY * 44.0f;
            end.scaleY = endHeight / 64.0f;
            end.color = normalColor;

            if (!note->missed) {
                char shouldHold = Note_ShouldHold(note, view->time);

                if (!note->mustHit) {
                    if (shouldHold)
                        events.opponentHold();
                }
                else if (game_options_botplay) {
                    if (shouldHold)
                        events.botHold();
                }
                else if (!note->pressed)
                    goto SKIP_CLIP;
                else if (!view->paused && shouldHold) {
                    float tailLeft = (note->time + note->length) - view->time;
                    if (!holdDown[note->id] && tailLeft > 0.12f) {
                        events.miss(note);
                        goto SKIP_CLIP;
                    }

                    events.playerHold();
                }

                // Clip geométrico
                if (game_options_scroll == 2) {
                    float receptorX = targetStrumNote->object.position.x + trailOffset.x;
                    float trailRight = trail.position.x + trail.scaleX * 44.0f;
                    float velX = events.velocityX(note->id, note->mustHit);

                    if (velX > 0) {
                        if (trailRight > receptorX) {
                            trail.scaleX = fmaxf(0.0f, (trailRight - receptorX) / 44.0f);
                            trail.position.x = receptorX - trail.scaleX * 44.0f;
                        }
                        if (trail.scaleX > 0.0f)
                            Render_DrawAnimatedObject(&trail);
                        if (trailRight >= receptorX)
                            Render_DrawAnimatedObject(&end);
                    } else {
                        if (trail.position.x < receptorX) {
                            trail.scaleX = fmaxf(0.0f, (receptorX - trail.position.x) / 44.0f);
                        }
                        float origScaleX = trail.scaleX;
                        float origScaleY = trail.scaleY;
                        trail.scaleX *= 0.4f;
                        trail.scaleY *= 0.4f;
                        if (trail.scaleY > 0.0f)
                            Render_DrawAnimatedObject(&trail);
                        trail.scaleX = origScaleX;
                        trail.scaleY = origScaleY;
                        if (trail.position.x <= receptorX) {
                            float origEndScaleX = end.scaleX;
                            float origEndScaleY = end.scaleY;
                            end.scaleX = origEndScaleX * 0.4f;
                            end.scaleY = origEndScaleY * 0.4f;
                            Render_DrawAnimatedObject(&end);
                            end.scaleX = origEndScaleX;
                            end.scaleY = origEndScaleY;
                        }
                    }
                } else if (game_options_scroll == 1) {
                    float receptorY = targetStrumNote->object.position.y + trailOffset.y;
                    float trailBottom = trail.position.y + trail.scaleY * 44.0f;
                    if (trailBottom > receptorY) {
                        trail.scaleY = fmaxf(0.0f, (receptorY - trail.position.y) / 44.0f);
                    }
                    if (trail.scaleY > 0.0f)
                        Render_DrawAnimatedObject(&trail);
                    if (trailBottom >= receptorY)
                        Render_DrawAnimatedObject(&end);
                } else {
                    float receptorY = targetStrumNote->object.position.y + trailOffset.y;
                    float trailBottom = trail.position.y + trail.scaleY * 44.0f;
                    if (trail.position.y < receptorY) {
                        trail.scaleY = fmaxf(0.0f, (trailBottom - receptorY) / 44.0f);
                        trail.position.y = receptorY;
                    }
                    if (trail.scaleY > 0.0f)
                        Render_DrawAnimatedObject(&trail);
                    if (trailBottom >= receptorY)
                        Render_DrawAnimatedObject(&end);
                }
                goto CLIPPED;
            }
            else {
                trail.color = missedColor;
                end.color = missedColor;
            }

SKIP_CLIP:
            if (!fullyAbove) {
                Render_DrawAnimatedObject(&trail);
                Render_DrawAnimatedObject(&end);
            }
CLIPPED:
            ;
        }

        if (!note->pressed) {
            if (!note->missed && Note_TooLate(note, view->time)) {
                events.miss(note);
            }

            if (game_options_scroll == 2 && !note->mustHit) {
                float origScaleX = renderNote.scaleX;
                float origScaleY = renderNote.scaleY;
                renderNote.scaleX = 0.4f;
                renderNote.scaleY = 0.4f;
                Render_DrawAnimatedObject(&renderNote);
                renderNote.scaleX = origScaleX;
                renderNote.scaleY = origScaleY;
            } else if (game_options_scroll == 1) {
                float origScaleX = renderNote.scaleX;
                float origScaleY = renderNote.scaleY;
                renderNote.scaleX = 0.85f;
                renderNote.scaleY = 0.85f;
                Render_DrawAnimatedObject(&renderNote);
                renderNote.scaleX = origScaleX;
                renderNote.scaleY = origScaleY;
            } else {
                Render_DrawAnimatedObject(&renderNote);
            }
        }
    }

    EndBlendMode();
}
