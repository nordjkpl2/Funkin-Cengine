/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStatePlayerInput.c - input do jogador: teclas, julgamento, score e misses.
 * Co-Op: cada jogador (slot 0 e 1) tem o seu conjunto de teclas e so acerta as notas do seu lado.
 */

#include "scenes/gameplay/notes/PlayStatePlayerInput.h"
#include "scenes/gameplay/HUD/PlayStateHealth.h"
#include "scenes/gameplay/notes/PlayStateNoteSpawn.h"
#include <math.h>
#include "GameData.h"

// se motal = 0 fica imortal se mortal = 1 morre
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

/* ------------------------------------------------------------------ */
/* Quem controla cada lado (Co-Op e Play as)                           */
/* ------------------------------------------------------------------ */

/* Lado do BF (mustHit) e do jogador 1: BF normalmente, oponente com Play as. O jogador 2 (Co-Op) pega o outro lado. */
int PlayStatePlayerInput_Owner(const Note* note) {
    if (gameOptions.botplay) return -1;
    const char p1Side = gameOptions.playAsOpponent ? 0 : 1;
    if (note->mustHit == p1Side) return 0;
    return gameOptions.coopOn ? 1 : -1;
}

int PlayStatePlayerInput_Slots(void) { return gameOptions.coopOn ? 2 : 1; }

int PlayStatePlayerInput_Key(int slot, int lane) {
    if (!gameOptions.coopOn) return game_scheme_keys[gameOptions.scheme][lane];
    return gameOptions.coopKeys[slot][lane];
}

char PlayStatePlayerInput_SlotSideBf(int slot) {
    if (slot == 0) return gameOptions.playAsOpponent ? 0 : 1;
    return gameOptions.playAsOpponent ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/* Acerto e miss                                                       */
/* ------------------------------------------------------------------ */

static void HitNote(Note* note, float songTime) {
    float diff = fabsf(note->time - songTime);
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

    PlayStateHealth_AddForSide(HEALTH_GAIN_HIT, (char)note->mustHit);
    note->pressed = 1;
    events.hit(note, rating);
}

void PlayStatePlayerInput_MissNote(Note* note) {
    if (gameOptions.botplay) return; // BOTPLAY NUNCA DISPARA MISS

    note->missed = 1;
    PlayStateHealth_AddForSide(-HEALTH_LOSS_MISS, (char)note->mustHit);
    misses++;
    if (note->mustHit) events.miss(note->id); // so a nota do lado do BF tem animacao de erro
}

static void GhostMiss(int slot, int lane) {
    if (gameOptions.botplay) return; // BOTPLAY NUNCA DISPARA MISS
    if (lane < 0 || lane > 3) return;

    PlayStateHealth_AddForSide(-HEALTH_LOSS_MISS, PlayStatePlayerInput_SlotSideBf(slot));
    misses++;
    if (PlayStatePlayerInput_SlotSideBf(slot)) events.miss(lane);
}

/* Input de um jogador (slot): teclas dele e so as notas do lado dele. */
static void UpdateSlot(int slot, float songTime, PlayStatePlayerKeys* keys) {
    Note* notes = PlayStateNoteSpawn_Notes();
    const size_t noteCount = PlayStateNoteSpawn_Count();
    const size_t firstNote = PlayStateNoteSpawn_First();

    char* pressed = keys->pressed;
    int* keyPressed = keys->keyPressed;
    char* keyDown = keys->keyDown;

    char anyPressed = 0;
    for (int i = 0; i < 4; i++) {
        pressed[i] = 0;
        const int key = PlayStatePlayerInput_Key(slot, i);
        keyPressed[i] = IsKeyPressed(key);
        keyDown[i] = IsKeyDown(key);
        if (keyPressed[i])
            anyPressed = 1;
    }

    // Ghost: uma unica varredura guarda a primeira nota acertavel de cada lane;
    // o acerto usa esses candidatos (mesma ordem de janela/condicoes da varredura de acerto).
    if (gameOptions.ghost == 1 && anyPressed) {
        Note* candidate[4] = {NULL, NULL, NULL, NULL};
        char anyHittable = 0;
        for (size_t i = firstNote; i < noteCount; i++) {
            Note* n = notes + i;
            if (PlayStatePlayerInput_Owner(n) != slot || n->missed || n->pressed)
                continue;
            if (Note_TooEarly(n, songTime))
                break;
            if (n->id < 0 || n->id > 3)
                continue;
            if (!candidate[n->id] && Note_CanBeHit(n, songTime)) {
                candidate[n->id] = n;
                anyHittable = 1;
            }
        }
        if (anyHittable) {
            for (int i = 0; i < 4; i++) {
                if (keyPressed[i] && !candidate[i]) {
                    keyPressed[i] = 0;
                    keyDown[i] = 0;
                }
            }
        }

        for (int i = 0; i < 4; i++) {
            if (keyPressed[i] && candidate[i] && !candidate[i]->pressed && !pressed[i]) {
                pressed[i] = 1;
                HitNote(candidate[i], songTime);
            }
        }
        return;
    }

    if (anyPressed) {
        int pressedNotes = 0;
        for (size_t i = firstNote; i < noteCount; i++) {
            Note* note = notes + i;

            if (PlayStatePlayerInput_Owner(note) != slot || note->missed)
                continue;

            if (Note_TooEarly(note, songTime))
                break;

            if (note->id < 0 || note->id > 3)
                continue;

            if (keyPressed[note->id] && Note_CanBeHit(note, songTime) && !note->pressed && !pressed[note->id]) {
                pressed[note->id] = 1;
                HitNote(note, songTime);

                pressedNotes++;
                if (pressedNotes == 4)
                    break;
            }
        }
    }

    if (gameOptions.ghost == 2) {
        for (int i = 0; i < 4; i++) {
            if (keyPressed[i] && !pressed[i])
                GhostMiss(slot, i);
        }
    }
}

char PlayStatePlayerInput_Update(float songTime, PlayStatePlayerKeys keys[2]) {
    // SE BOTPLAY ESTIVER ATIVO: O acerto agora ocorre diretamente no receptor (em PlayState_Draw)
    // exatamente como o oponente, deixando as notas 100% visíveis e imunes a morte!
    // Vida do botplay nao e tocada aqui: fica no 0.5 do Reset (sem drain) ou so cai com o drain, que nunca mata.
    if (gameOptions.botplay)
        return 0;

    const int slots = PlayStatePlayerInput_Slots();
    for (int slot = 0; slot < slots; slot++)
        UpdateSlot(slot, songTime, &keys[slot]);
    return 1;
}
