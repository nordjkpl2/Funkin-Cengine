/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * OptionsMenuWarns.c - tela "Warns" (aberta pela opcao em [Other]): lista os avisos de carregamento
 * (stage/personagem/audio nao encontrado, falta de memoria no cache...). Antes eles apareciam antes
 * da gameplay; agora ficam aqui, para o jogador nao ser incomodado.
 * ESC/BACKSPACE/ENTER voltam; UP/DOWN rolam quando ha mais avisos que cabem na tela.
 */
#include "scenes/api/FontLoader.h"
#include "scenes/menu/options/OptionsMenuInternal.h"
#include "core/Log.h"
#include "rendering/TextMeasure.h"
#include <stdio.h>

bool warnsScreen;               // tela aberta
static int warnsScroll;         // primeiro aviso visivel
static int warnsVisibleRows;    // linhas que cabem (calculado no Draw)

#define WARNS_ROW_Y 130.0f
#define WARNS_ROW_H 30.0f
#define WARNS_BOTTOM_Y 640.0f

static int WarnsMaxScroll(void) {
    const int total = Log_WarnCount() < LOG_WARN_MAX ? Log_WarnCount() : LOG_WARN_MAX;
    const int max = total - warnsVisibleRows;
    return max > 0 ? max : 0;
}

void WarnsScreen_Begin(void) {
    warnsScreen = true;
    warnsScroll = 0;
    while (GetKeyPressed() != 0) {}   // descarta o Enter que abriu a tela
}

void WarnsScreen_Update(void) {
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE) ||
        IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        warnsScreen = false;
        return;
    }

    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) warnsScroll++;
    else if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) warnsScroll--;

    const int max = WarnsMaxScroll();
    if (warnsScroll > max) warnsScroll = max;
    if (warnsScroll < 0) warnsScroll = 0;
}

void WarnsScreen_Draw(void) {
    DrawRectangle(0, 0, 1280, 720, BLACK);

    char title[48];
    (void)snprintf(title, sizeof(title), "WARNS (%d)", Log_WarnCount());
    DrawTextEx(mainFont, title, (Vector2) {80, 50}, 70, TEXT_SPACING, YELLOW);

    const int count = Log_WarnCount();
    if (count == 0) {
        DrawTextEx(mainFont, "Nenhum aviso desde o ultimo carregamento de gameplay.",
                   (Vector2) {80, WARNS_ROW_Y}, 24, 1, GRAY);
    }

    warnsVisibleRows = (int)((WARNS_BOTTOM_Y - WARNS_ROW_Y) / WARNS_ROW_H);
    const int total = count < LOG_WARN_MAX ? count : LOG_WARN_MAX;
    const int first = warnsScroll;
    const int last = first + warnsVisibleRows < total ? first + warnsVisibleRows : total;

    for (int i = first; i < last; i++)
        DrawTextEx(mainFont, Log_WarnAt(i), (Vector2) {80, WARNS_ROW_Y + (float)(i - first) * WARNS_ROW_H},
                   20, 1, WHITE);

    if (count > total) {
        char more[80];
        (void)snprintf(more, sizeof(more), "... e mais %d (veja assets/logs.txt)", count - total);
        DrawTextEx(mainFont, more, (Vector2) {80, WARNS_BOTTOM_Y}, 20, 1, GRAY);
    }

    DrawTextEx(mainFont, "UP / DOWN: scroll     ENTER / ESC / BACKSPACE: back",
               (Vector2) {80, 688}, 22, 1, GRAY);
}
