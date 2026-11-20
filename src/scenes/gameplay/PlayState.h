/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
#pragma once
#ifndef PLAYSTATE_H
#define PLAYSTATE_H

void PlayState_SetScene(void);
void PlayState_SetSongDir(const char* dir);
void PlayState_SetSongDirKeep(const char* dir);
void PlayState_SetReturnEditor(char on);
void PlayState_SetReturnWeek(char on);
void PlayState_SetDiff(const char* diff);
void PlayState_SetCharsOverride(const char* p2);

/* Modo do test play do chart editor. PlayState_SetSongDir devolve sempre a FULL.
   Os "PLAY ONLY" trocam so o volume das faixas; RECEPTORS mostra so notas e receptores sobre preto. */
typedef enum PlayStateTestMode {
    PLAYSTATE_TEST_FULL = 0,
    PLAYSTATE_TEST_INST,
    PLAYSTATE_TEST_VOCALS,
    PLAYSTATE_TEST_BF_VOCALS,
    PLAYSTATE_TEST_OPP_VOCALS,
    PLAYSTATE_TEST_RECEPTORS
} PlayStateTestMode;
void PlayState_SetTestMode(PlayStateTestMode mode);

#endif
