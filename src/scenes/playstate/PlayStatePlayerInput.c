/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStatePlayerInput.c - input do jogador: teclas, julgamento, score e misses.
 */

#include "scenes/playstate/PlayStatePlayerInput.h"
#include "scenes/playstate/PlayStateHealth.h"
#include "scenes/playstate/PlayStateNoteSpawn.h"
#include <math.h>

static int score;
static int misses;
static PlayStatePlayerInputEvents events;

void PlayStatePlayerInput_SetEvents(const PlayStatePlayerInputEvents* newEvents) {
    events = *newEvents;
}

void PlayStatePlayerInput_Reset(void) {
    score = 0;
    misses = 0;
}

int PlayStatePlayerInput_Score(void) { return score; }
int PlayStatePlayerInput_Misses(void) { return misses; }
void PlayStatePlayerInput_AddScore(int amount) { score += amount; }

static void HitNote(Note* note, float songTime) {
    float diff = fabs(note->time - songTime);
    PlayerRating rating;

    if (diff > 0.1503f) {
        score += 50;
        rating = PLAYER_RATING_SHIT;
    }
    else if (diff > 0.126f) {
        score += 100;
        rating = PLAYER_RATING_BAD;
    }
    else if (diff > 0.033f) {
        score += 200;
        rating = PLAYER_RATING_GOOD;
    }
    else {
        score += 350;
        rating = PLAYER_RATING_SICK;
    }

    PlayStateHealth_Add(0.0115f);
    note->pressed = 1;
    events.hit(note, rating);
}

void PlayStatePlayerInput_MissNote(Note* note) {
    if (game_options_botplay) return; // BOTPLAY NUNCA DISPARA MISS

    note->missed = 1;
    PlayStateHealth_Add(-0.02f);
    misses++;
    events.miss(note->id);
}

static void GhostMiss(int lane) {
    if (game_options_botplay) return; // BOTPLAY NUNCA DISPARA MISS
    if (lane < 0 || lane > 3) return;

    PlayStateHealth_Add(-0.02f);
    misses++;
    events.miss(lane);
}

char PlayStatePlayerInput_Update(float songTime, PlayStatePlayerKeys* keys) {
    Note* notes = PlayStateNoteSpawn_Notes();
    const size_t noteCount = PlayStateNoteSpawn_Count();
    const size_t firstNote = PlayStateNoteSpawn_First();

    // SE BOTPLAY ESTIVER ATIVO: O acerto agora ocorre diretamente no receptor (em PlayState_Draw)
    // exatamente como o oponente, deixando as notas 100% visíveis e imunes a morte!
    if (game_options_botplay) {
        PlayStateHealth_Set(0.5f); // Mantém a vida travada e imortal no botplay
        return 0;
    }

    char* pressed = keys->pressed;
    int* keyPressed = keys->keyPressed;
    char* keyDown = keys->keyDown;

    int pressedNotes = 0;
    char anyPressed = 0;
    for (int i = 0; i < 4; i++) {
        pressed[i] = 0;
        int key = game_scheme_keys[game_options_scheme][i];
        keyPressed[i] = IsKeyPressed(key);
        keyDown[i] = IsKeyDown(key);
        if (keyPressed[i])
            anyPressed = 1;
    }

    if (game_options_ghost == 1 && anyPressed) {
        char hittable[4] = {0, 0, 0, 0};
        char anyHittable = 0;
        for (size_t i = firstNote; i < noteCount; i++) {
            Note* n = notes + i;
            if (!n->mustHit || n->missed || n->pressed)
                continue;
            if (Note_TooEarly(n, songTime))
                break;
            if (n->id < 0 || n->id > 3)
                continue;
            if (Note_CanBeHit(n, songTime)) {
                hittable[n->id] = 1;
                anyHittable = 1;
            }
        }
        if (anyHittable) {
            anyPressed = 0;
            for (int i = 0; i < 4; i++) {
                if (keyPressed[i] && !hittable[i]) {
                    keyPressed[i] = 0;
                    keyDown[i] = 0;
                }
                if (keyPressed[i])
                    anyPressed = 1;
            }
        }
    }

    if (!anyPressed)
        goto BRUH;

    for (size_t i = firstNote; i < noteCount; i++) {
        Note* note = notes + i;

        if (!note->mustHit || note->missed)
            continue;

        if (Note_TooEarly(note, songTime))
            goto BRUH;

        if (note->id < 0 || note->id > 3)
            continue;

        if (keyPressed[note->id] && Note_CanBeHit(note, songTime) && !note->pressed && !pressed[note->id]) {
            pressed[note->id] = 1;
            HitNote(note, songTime);

            pressedNotes++;
            if (pressedNotes == 4)
                goto BRUH;
        }
    }

BRUH:
    if (game_options_ghost == 2) {
        for (int i = 0; i < 4; i++) {
            if (keyPressed[i] && !pressed[i])
                GhostMiss(i);
        }
    }

    return 1;
}
