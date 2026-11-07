/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateText.c - textos do HUD: tempo, titulo, score, misses e BOTPLAY.
 */

#include "scenes/gameplay/PlayStateText.h"
#include "scenes/gameplay/PlayStateAudio.h"
#include <stdio.h>
#include <string.h>

static Font vcrFont;

static int lastScoreDrawn = -1;
static char scoreBuf[32] = "Score: 0";
static Vector2 scorePos;

static int lastMissesDrawn = -1;
static char missBuf[32] = "Misses: 0";
static Vector2 missPos;

// Nome da musica exibido acima do score.
static char songDisplayName[128] = "Unknown Song";
static Vector2 songDisplayNamePos = {910.0f, 650.0f};

// Canto inferior direito (titulo, score e misses alinhados a direita).
#define TEXT_RIGHT_X 1270.0f
// Tempo do topo, centralizado igual a barra de vida.
#define TEXT_CENTER_X 600.0f

static int lastTimeSec = -1;
static char timeBuf[32] = "0:00 / 0:00";
static Vector2 timePos;

// Extrai o nome da pasta da musica e prepara um titulo curto para o HUD.
static void UpdateSongDisplayName(const char* songDir) {
    const char* end = songDir + strlen(songDir);

    while (end > songDir && (end[-1] == '/' || end[-1] == '\\'))
        end--;

    const char* start = end;
    while (start > songDir && start[-1] != '/' && start[-1] != '\\')
        start--;

    size_t length = (size_t)(end - start);
    if (length == 0) {
        snprintf(songDisplayName, sizeof(songDisplayName), "Unknown Song");
    } else {
        if (length >= sizeof(songDisplayName))
            length = sizeof(songDisplayName) - 1;

        memcpy(songDisplayName, start, length);
        songDisplayName[length] = '\0';
    }

    // Deixa nomes de pastas como "song-name" mais legiveis no HUD.
    for (char* c = songDisplayName; *c; c++) {
        if (*c == '_' || *c == '-')
            *c = ' ';
    }

    Vector2 textSize = MeasureTextEx(vcrFont, songDisplayName, 13.0f, 1.0f);
    while (textSize.x > 350.0f && strlen(songDisplayName) > 4) {
        length = strlen(songDisplayName) - 1;
        songDisplayName[length] = '\0';

        if (length >= 3) {
            songDisplayName[length - 3] = '.';
            songDisplayName[length - 2] = '.';
            songDisplayName[length - 1] = '.';
        }

        textSize = MeasureTextEx(vcrFont, songDisplayName, 13.0f, 1.0f);
    }

    songDisplayNamePos.x = TEXT_RIGHT_X - textSize.x;
    songDisplayNamePos.y = 651.0f;
}

void PlayStateText_Init(const char* songDir) {
    // A fonte fica carregada entre as musicas (rasterizar o TTF a cada musica era desperdicio).
    if (vcrFont.texture.id == 0) {
        vcrFont = LoadFontEx("assets/fonts/vcr.ttf", 32, 0, 0);
        SetTextureFilter(vcrFont.texture, TEXTURE_FILTER_BILINEAR);
    }

    snprintf(scoreBuf, sizeof(scoreBuf), "Score: 0");
    Vector2 ss = MeasureTextEx(vcrFont, scoreBuf, 16, 1);
    scorePos = (Vector2) {TEXT_RIGHT_X - ss.x, 672};

    snprintf(missBuf, sizeof(missBuf), "Misses: 0");
    Vector2 ms = MeasureTextEx(vcrFont, missBuf, 16, 1);
    missPos = (Vector2) {TEXT_RIGHT_X - ms.x, 690};

    snprintf(timeBuf, sizeof(timeBuf), "0:00 / 0:00");
    Vector2 ts = MeasureTextEx(vcrFont, timeBuf, 20.0f, 1);
    timePos = (Vector2) {TEXT_CENTER_X - (ts.x / 2.0f), 15.0f};

    UpdateSongDisplayName(songDir);
}

void PlayStateText_Unload(void) {
    // Mantida: reaproveitada pela proxima musica; o SO libera ao fechar o jogo.
}

void PlayStateText_Reset(void) {
    lastScoreDrawn = -1;
    lastMissesDrawn = -1;
    lastTimeSec = -1;
}

void PlayStateText_Draw(int score, int misses, float songTime, char showScore) {
    // Tempo
    {
        int currentSec = (int)(songTime < 0 ? 0 : songTime);
        if (currentSec != lastTimeSec) {
            lastTimeSec = currentSec;
            float total = PlayStateAudio_Length();
            if (total < 0) total = 0;
            snprintf(timeBuf, sizeof(timeBuf), "%d:%02d / %d:%02d",
                currentSec / 60, currentSec % 60, (int)total / 60, (int)total % 60);

            Vector2 textSize = MeasureTextEx(vcrFont, timeBuf, 20.0f, 1);
            timePos = (Vector2) {TEXT_CENTER_X - (textSize.x / 2.0f), 15.0f};
        }

        const float tsh = 1.5f;
        DrawTextEx(vcrFont, timeBuf, (Vector2) {timePos.x + tsh, timePos.y}, 20.0f, 1, BLACK);
        DrawTextEx(vcrFont, timeBuf, timePos, 20.0f, 1, WHITE);
    }

    // Nome da musica, alinhado a direita acima do score.
    {
        const Color titleShadow = {0, 0, 0, 180};
        const Color titleColor = {225, 225, 225, 220};
        DrawTextEx(vcrFont, songDisplayName,
            (Vector2) {songDisplayNamePos.x + 1.0f, songDisplayNamePos.y + 1.0f},
            13.0f, 1.0f, titleShadow);
        DrawTextEx(vcrFont, songDisplayName, songDisplayNamePos,
            13.0f, 1.0f, titleColor);
    }

    // Score e Misses
    if (showScore) {
        if (score != lastScoreDrawn) {
            snprintf(scoreBuf, sizeof(scoreBuf), "Score: %d", score);
            lastScoreDrawn = score;
            Vector2 ss = MeasureTextEx(vcrFont, scoreBuf, 16, 1);
            scorePos = (Vector2) {TEXT_RIGHT_X - ss.x, 672};
        }
        const float shadow = 1.5f;
        DrawTextEx(vcrFont, scoreBuf, (Vector2) {scorePos.x + shadow, scorePos.y - shadow}, 16, 1, BLACK);
        DrawTextEx(vcrFont, scoreBuf, scorePos, 16, 1, WHITE);

        if (misses != lastMissesDrawn) {
            snprintf(missBuf, sizeof(missBuf), "Misses: %d", misses);
            lastMissesDrawn = misses;
            Vector2 ms = MeasureTextEx(vcrFont, missBuf, 16, 1);
            missPos = (Vector2) {TEXT_RIGHT_X - ms.x, 690};
        }
        DrawTextEx(vcrFont, missBuf, (Vector2) {missPos.x + shadow, missPos.y - shadow}, 16, 1, BLACK);
        DrawTextEx(vcrFont, missBuf, missPos, 16, 1, WHITE);
    }
}

void PlayStateText_DrawBotplayMark(void) {
    const Vector2 botplayPos = {22.0f, 20.0f};
    const Color botplayShadow = {0, 0, 0, 110};
    const Color botplayColor = {255, 255, 255, 125};

    DrawTextEx(vcrFont, "BOTPLAY",
        (Vector2) {botplayPos.x + 2.0f, botplayPos.y + 2.0f},
        22.0f, 1.0f, botplayShadow);
    DrawTextEx(vcrFont, "BOTPLAY", botplayPos,
        22.0f, 1.0f, botplayColor);
}
