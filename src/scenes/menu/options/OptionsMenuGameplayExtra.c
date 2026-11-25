/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * OptionsMenuGameplayExtra.c - tela "Gameplay Extra" (aberta pela opcao em [Other]):
 * Co-Op (2 jogadores no mesmo teclado, com nome e teclas de cada um) e Play as (Boyfriend ou Opponent).
 * Os valores ficam em gameOptions (GameData) e sao gravados ao fechar a tela.
 */
#include "scenes/api/FontLoader.h"
#include "scenes/menu/options/OptionsMenuInternal.h"
#include "GameData.h"
#include <stdio.h>
#include <string.h>

/* Linhas da tela, de cima para baixo. */
enum { GX_COOP, GX_PLAY_AS, GX_P1_NAME, GX_P1_KEYS, GX_P2_NAME, GX_P2_KEYS, GX_COUNT };

#define GX_ROW_Y 170.0f
#define GX_ROW_H 64.0f
#define GX_ROW_X 120.0f

bool gameplayExtraScreen;     // tela aberta
static int gxSel;             // linha selecionada (GX_*)
static int gxEditName = -1;   // jogador com o nome em edicao (0 ou 1); -1 = nenhum
static int gxCapture = -1;    // jogador com as teclas em captura (0 ou 1); -1 = nenhum
static int gxStep;            // 0..3 = esperando a tecla dessa direcao; 4 = confirmar
static int gxKeys[4];         // teclas capturadas ate agora
static char gxName[2][COOP_NAME_MAX]; // copia de trabalho do nome enquanto edita
static const char* gxMsg = "";

static const char* const GX_LANES[4] = {"LEFT", "DOWN", "UP", "RIGHT"};

/* Descarta teclas e digitacao acumuladas (o Enter que abriu a acao nao pode contar). */
static void DrainInput(void) {
    while (GetKeyPressed() != 0) {}
    while (GetCharPressed() != 0) {}
}

void GameplayExtra_Begin(void) {
    gameplayExtraScreen = true;
    gxSel = GX_COOP;
    gxEditName = -1;
    gxCapture = -1;
    gxMsg = "";
    DrainInput();
}

static void Close(void) {
    GameData_Flush();
    gameplayExtraScreen = false;
}

static void BeginName(int p) {
    memcpy(gxName[p], gameOptions.coopName[p], COOP_NAME_MAX);
    gxEditName = p;
    gxMsg = "";
    DrainInput();
}

/* Fim da edicao do nome: keep = grava; senao descarta. Nome so com espacos vira vazio (usa "Player N"). */
static void EndName(bool keep) {
    if (keep) {
        bool blank = true;
        for (const char* c = gxName[gxEditName]; *c; c++)
            if (*c != ' ') { blank = false; break; }
        if (blank) gxName[gxEditName][0] = 0;
        memcpy(gameOptions.coopName[gxEditName], gxName[gxEditName], COOP_NAME_MAX);
        GameData_Flush();
    }
    gxEditName = -1;
}

static void UpdateName(void) {
    const int p = gxEditName;

    if (IsKeyPressed(KEY_ESCAPE)) { EndName(false); return; }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) { EndName(true); return; }

    if (IsKeyPressed(KEY_BACKSPACE)) {
        const size_t len = strlen(gxName[p]);
        if (len > 0) gxName[p][len - 1] = 0;
    }

    int ch;
    while ((ch = GetCharPressed()) != 0) {
        const size_t len = strlen(gxName[p]);
        if (ch >= 0x20 && ch <= 0x7E && len < COOP_NAME_MAX - 1) {
            gxName[p][len] = (char)ch;
            gxName[p][len + 1] = 0;
        }
    }
}

static void BeginCapture(int p) {
    gxCapture = p;
    gxStep = 0;
    gxMsg = "";
    memset(gxKeys, 0, sizeof(gxKeys));
    DrainInput();
}

/* Tecla ja usada: nas capturadas agora ou nas do outro jogador. */
static bool KeyTaken(int p, int key) {
    for (int k = 0; k < gxStep; k++)
        if (gxKeys[k] == key) return true;
    for (int k = 0; k < 4; k++)
        if (gameOptions.coopKeys[1 - p][k] == key) return true;
    return false;
}

static void UpdateCapture(void) {
    const int p = gxCapture;
    const int key = GetKeyPressed();   // consome a fila sempre, para nao sobrar tecla de um passo para outro

    if (IsKeyPressed(KEY_ESCAPE)) { gxCapture = -1; return; }   // cancela e mantem as antigas

    if (gxStep == 4) {
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
            memcpy(gameOptions.coopKeys[p], gxKeys, sizeof(gxKeys));
            GameData_Flush();
            gxCapture = -1;
        } else if (IsKeyPressed(KEY_BACKSPACE)) {
            gxStep = 3;
            gxMsg = "";
        }
        return;
    }

    if (IsKeyPressed(KEY_BACKSPACE)) {   // volta uma direcao
        if (gxStep > 0) gxStep--;
        gxMsg = "";
        return;
    }

    if (key == 0) return;
    if (key == KEY_ENTER || key == KEY_KP_ENTER) { gxMsg = "ENTER e reservado"; return; }
    if (KeyTaken(p, key)) { gxMsg = "Tecla ja usada"; return; }

    gxKeys[gxStep++] = key;
    gxMsg = "";
}

void GameplayExtra_Update(void) {
    if (gxCapture >= 0) { UpdateCapture(); return; }
    if (gxEditName >= 0) { UpdateName(); return; }

    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) { Close(); return; }

    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) gxSel = (gxSel + 1) % GX_COUNT;
    else if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) gxSel = (gxSel + GX_COUNT - 1) % GX_COUNT;

    const bool side = IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A) || IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D);
    const bool enter = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_SPACE);

    if (gxSel == GX_COOP && (side || enter)) {
        gameOptions.coopOn = !gameOptions.coopOn;
        GameData_Flush();
    } else if (gxSel == GX_PLAY_AS && (side || enter)) {
        gameOptions.playAsOpponent = !gameOptions.playAsOpponent;
        GameData_Flush();
    } else if (enter) {
        switch (gxSel) {
            case GX_P1_NAME:  BeginName(0); break;
            case GX_P2_NAME:  BeginName(1); break;
            case GX_P1_KEYS:  BeginCapture(0); break;
            case GX_P2_KEYS:  BeginCapture(1); break;
            default: break;
        }
    }
}

/* Texto das 4 teclas; slots sem tecla (count) mostram "...". */
static void KeysText(const int* keys, int count, char* out, size_t size) {
    char name[4][16];
    for (int k = 0; k < 4; k++) {
        if (k < count) Options_KeyName(keys[k], name[k], sizeof(name[k]));
        else (void)snprintf(name[k], sizeof(name[k]), "...");
    }
    (void)snprintf(out, size, "%s  %s  %s  %s", name[0], name[1], name[2], name[3]);
}

static void GxRow(int row, const char* text, Color color) {
    const float y = GX_ROW_Y + (float)row * GX_ROW_H;
    if (row == gxSel)
        DrawRectangleRounded((Rectangle) {GX_ROW_X - 20.0f, y - 6.0f, 1120.0f, GX_ROW_H - 8.0f}, 0.35f, 8,
                             (Color) {255, 255, 255, 30});
    DrawTextEx(mainFont, text, (Vector2) {GX_ROW_X, y}, 40, 2, color);
}

static void DrawNameLine(int p, int row) {
    char name[COOP_NAME_MAX + 32];
    if (gxEditName == p)
        (void)snprintf(name, sizeof(name), "Player %d name: %s_", p + 1, gxName[p]);
    else if (gameOptions.coopName[p][0])
        (void)snprintf(name, sizeof(name), "Player %d name: %s", p + 1, gameOptions.coopName[p]);
    else
        (void)snprintf(name, sizeof(name), "Player %d name: (Player %d)", p + 1, p + 1);
    GxRow(row, name, row == gxSel ? WHITE : GRAY);
}

static void DrawKeysLine(int p, int row) {
    char keys[64], line[128];
    const bool capturing = gxCapture == p;
    if (capturing) KeysText(gxKeys, gxStep, keys, sizeof(keys));
    else KeysText(gameOptions.coopKeys[p], 4, keys, sizeof(keys));
    (void)snprintf(line, sizeof(line), "Player %d keys: %s", p + 1, keys);
    GxRow(row, line, row == gxSel ? WHITE : GRAY);
}

void GameplayExtra_Draw(void) {
    DrawRectangle(0, 0, 1280, 720, (Color) {0, 0, 0, 230});
    DrawTextEx(mainFont, "GAMEPLAY EXTRA", (Vector2) {80, 40}, 60, TEXT_SPACING, YELLOW);

    char line[96];
    (void)snprintf(line, sizeof(line), "Co-Op: [%s]", gameOptions.coopOn ? "ON" : "OFF");
    GxRow(GX_COOP, line, gxSel == GX_COOP ? WHITE : GRAY);

    (void)snprintf(line, sizeof(line), "Play as: << %s >>", gameOptions.playAsOpponent ? "Opponent" : "Boyfriend");
    GxRow(GX_PLAY_AS, line, gxSel == GX_PLAY_AS ? WHITE : GRAY);

    DrawNameLine(0, GX_P1_NAME);
    DrawKeysLine(0, GX_P1_KEYS);
    DrawNameLine(1, GX_P2_NAME);
    DrawKeysLine(1, GX_P2_KEYS);

    // Rodape: o que as teclas fazem agora.
    if (gxCapture >= 0) {
        if (gxStep < 4) {
            (void)snprintf(line, sizeof(line), "Press the key for %s (player %d)", GX_LANES[gxStep], gxCapture + 1);
            DrawTextEx(mainFont, line, (Vector2) {80, 560}, 36, 3, WHITE);
        } else {
            DrawTextEx(mainFont, "ENTER to confirm", (Vector2) {80, 560}, 36, 3, GREEN);
        }
        DrawTextEx(mainFont, "BACKSPACE: back     ESC: cancel", (Vector2) {80, 650}, 24, 2, GRAY);
    } else if (gxEditName >= 0) {
        DrawTextEx(mainFont, "Type the name (optional, empty = Player N)", (Vector2) {80, 560}, 30, 2, WHITE);
        DrawTextEx(mainFont, "ENTER confirm     ESC cancel", (Vector2) {80, 650}, 24, 2, GRAY);
    } else {
        DrawTextEx(mainFont, "ENTER / LEFT / RIGHT: change     ESC / BACKSPACE: back", (Vector2) {80, 650}, 24, 2, GRAY);
    }

    if (gxMsg[0] != 0)
        DrawTextEx(mainFont, gxMsg, (Vector2) {80, 610}, 28, 2, RED);
}
