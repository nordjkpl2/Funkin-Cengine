/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
#include "GameData.h"
#include "core/Log.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
 * Save em assets/save.data: cabecalho "FNFS" + versao + registros (id u16, tamanho u16, bytes).
 *  - Cada campo escalar (char/int/float) tem UMA linha na tabela saveFields: id, padrao e faixa.
 *    Padrao e faixa sao aplicados automaticamente (GameData_Defaults e GameData_Sanitize).
 *  - Id usado nunca muda nem volta. Id desconhecido (campo removido ou de versao mais nova) e pulado.
 *  - Tamanho diferente do atual: copia o que cabe; o resto fica com o padrao.
 *  - Versao do cabecalho nao e checada: so o magic importa. Arquivo sem magic vai para save.data.bak antes de ser substituido.
 *  - Escrita em save.data.tmp e troca pelo arquivo final: queda no meio do salvamento nao corrompe o save.
 *
 * NOVO CAMPO ESCALAR: 1) campo em GameOptions (GameData.h); 2) UMA linha no fim de saveFields. Pronto.
 * Campo RAW (array/texto): alem da linha, tratar em GameData_SanitizeRaw e, se precisar, em GameData_Defaults.
 */
#define SAVE_FILE "assets/save.data"
#define SAVE_TMP  "assets/save.data.tmp"
#define SAVE_BAK  "assets/save.data.bak"
#define SAVE_MAGIC   "FNFS"
#define SAVE_VERSION 1       /* informativo: gravado no cabecalho, nao bloqueia a leitura */

#define SAVE_ID_MODS 0xFFFEu /* lista de mods desativados (tamanho = quantidade * MOD_NAME_MAX) */

GameOptions gameOptions;
char gameDisabledMods[MOD_DISABLED_MAX][MOD_NAME_MAX];
int  gameDisabledModCount;

const char* game_available_schemes[SCHEME_COUNT] = {"Arrows", "WASD", "DFJK", "ASKL", "Custom"};
int game_scheme_keys[SCHEME_COUNT][4] = {
    {KEY_LEFT, KEY_DOWN, KEY_UP, KEY_RIGHT},
    {KEY_A, KEY_S, KEY_W, KEY_D},
    {KEY_D, KEY_F, KEY_J, KEY_K},
    {KEY_A, KEY_S, KEY_K, KEY_L},
    {KEY_LEFT, KEY_DOWN, KEY_UP, KEY_RIGHT}   /* Custom: comeca igual ao Arrows */
};

/* ------------------------------------------------------------------ */
/* Tabela de save                                                      */
/* ------------------------------------------------------------------ */

typedef enum { SF_RAW = 0, SF_CHAR, SF_INT, SF_FLOAT } SaveKind;

typedef struct {
    uint16_t id;
    void* ptr;
    uint16_t size;
    SaveKind kind;
    double def, lo, hi; /* escalar: padrao e faixa (raw ignora) */
} SaveField;

/* O tipo do campo (char/int/float ou RAW) sai do proprio GameOptions. */
#define SAVE_FIELD(id_, field_, def_, lo_, hi_) { (id_), &gameOptions.field_, (uint16_t)sizeof(gameOptions.field_), \
    _Generic(gameOptions.field_, char: SF_CHAR, int: SF_INT, float: SF_FLOAT, default: SF_RAW), \
    (double)(def_), (double)(lo_), (double)(hi_) }

static const SaveField saveFields[] = {
    SAVE_FIELD(1,  menuMusic,          0,                    0,                  0),
    SAVE_FIELD(2,  showScore,          1,                    0,                  1),
    SAVE_FIELD(3,  bopIcons,           1,                    0,                  1),
    SAVE_FIELD(4,  vsync,              0,                    0,                  1),
    SAVE_FIELD(5,  fillScreen,         0,                    0,                  1),
    SAVE_FIELD(6,  botplay,            0,                    0,                  1),
    SAVE_FIELD(7,  ghost,              0,                    0,                  2),
    SAVE_FIELD(8,  scroll,             SCROLL_UP,            SCROLL_UP,          SCROLL_MIDDLEDOWN),
    SAVE_FIELD(9,  showFps,            1,                    0,                  1),
    SAVE_FIELD(10, hitSound,           0,                    0,                  1),
    SAVE_FIELD(11, scheme,             0,                    0,                  SCHEME_COUNT - 1),
    SAVE_FIELD(12, fps,                120,                  0,                  1000),
    SAVE_FIELD(13, zoomFactorUI,       1.0,                  0.1,                10.0),
    SAVE_FIELD(14, zoomFactorGAME,     1.0,                  0.1,                10.0),
    SAVE_FIELD(15, threadLimit,        0,                    0,                  64),
    SAVE_FIELD(16, scorePosition,      SCORE_POS_PSYCH,      SCORE_POS_CENGINE,  SCORE_POS_PSYCH),
    SAVE_FIELD(17, healthDrainPlay,    0,                    0,                  1),
    SAVE_FIELD(18, healthDrainBotplay, 0,                    0,                  1),
    SAVE_FIELD(19, strumBgPercent,     0,                    0,                  100),
    SAVE_FIELD(20, strumMobileVisible, 0,                    0,                  1),
    SAVE_FIELD(21, laneHSB,            0,                    0,                  0),
    SAVE_FIELD(22, hiddenNotesAlt,     0,                    0,                  3),
    SAVE_FIELD(23, horizontalMenu,     0,                    0,                  1),
    SAVE_FIELD(24, loseIcon,           1,                    0,                  1),
    SAVE_FIELD(25, streamingJsonParse, 0,                    0,                  1),
    SAVE_FIELD(26, hitSoundBotplay,    0,                    0,                  1),
    SAVE_FIELD(27, noteSplashes,       1,                    0,                  1),
    { 28, game_scheme_keys[SCHEME_CUSTOM], (uint16_t)sizeof(game_scheme_keys[SCHEME_CUSTOM]), SF_RAW, 0, 0, 0 },
    SAVE_FIELD(29, shaders,            1,                    0,                  1),
    SAVE_FIELD(30, coopOn,             0,                    0,                  1),
    SAVE_FIELD(31, playAsOpponent,     0,                    0,                  1),
    { 32, gameOptions.coopName, (uint16_t)sizeof(gameOptions.coopName), SF_RAW, 0, 0, 0 },
    { 33, gameOptions.coopKeys, (uint16_t)sizeof(gameOptions.coopKeys), SF_RAW, 0, 0, 0 },
    SAVE_FIELD(34, timePosition,       TIME_POS_DEFAULT,     TIME_POS_DEFAULT,   TIME_POS_RIGHTDOWN),
};
#define SAVE_FIELD_COUNT (sizeof(saveFields) / sizeof(saveFields[0]))

/* ------------------------------------------------------------------ */
/* Padroes e validacao                                                 */
/* ------------------------------------------------------------------ */

/* Teclas padrao do Co-Op: jogador 1 = WASD, jogador 2 = DFJK. */
static const int coopDefaultKeys[2][4] = {{KEY_A, KEY_S, KEY_W, KEY_D}, {KEY_D, KEY_F, KEY_J, KEY_K}};

static void GameData_Defaults(void) {
    gameDisabledModCount = 0;
    game_scheme_keys[SCHEME_CUSTOM][0] = KEY_LEFT;
    game_scheme_keys[SCHEME_CUSTOM][1] = KEY_DOWN;
    game_scheme_keys[SCHEME_CUSTOM][2] = KEY_UP;
    game_scheme_keys[SCHEME_CUSTOM][3] = KEY_RIGHT;

    memset(&gameOptions, 0, sizeof(gameOptions));
    memcpy(gameOptions.coopKeys, coopDefaultKeys, sizeof(coopDefaultKeys));
    for (size_t i = 0; i < SAVE_FIELD_COUNT; i++) {
        const SaveField* f = &saveFields[i];
        switch (f->kind) {
            case SF_CHAR:  *(char*)f->ptr = (char)f->def; break;
            case SF_INT:   *(int*)f->ptr = (int)f->def; break;
            case SF_FLOAT: *(float*)f->ptr = (float)f->def; break;
            default: break; /* RAW: zero (ou ajustado em GameData_SanitizeRaw) */
        }
    }
}

/* Hidden Notes: a lado (BF ou oponente) aparece? bfSide = 1 para BF; singerBf = quem canta na secao.
   0 Off: tudo. 1 Alternate: so quem canta. 2 Only Playable: so BF. 3 Alternate Playable: BF sempre + oponente quando ele canta. */
char GameData_HiddenShowsSide(char bfSide, char singerBf) {
    switch (gameOptions.hiddenNotesAlt) {
        case 1:  return (bfSide != 0) == (singerBf != 0);
        case 2:  return bfSide != 0;
        case 3:  return bfSide != 0 || !singerBf;
        default: return 1;
    }
}

Vector3 GameData_LaneShift(int lane) {
    if (lane < 0 || lane > 3) return (Vector3) {0.0f, 0.0f, 0.0f};
    const short* o = gameOptions.laneHSB[lane];
    return (Vector3) {o[0] / 360.0f, o[1] / 100.0f, o[2] / 100.0f};
}

char GameData_LaneShifted(int lane) {
    if (lane < 0 || lane > 3) return 0;
    const short* o = gameOptions.laneHSB[lane];
    return o[0] != 0 || o[1] != 0 || o[2] != 0;
}

static float Clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

Color GameData_ShiftColor(Color c, Vector3 shift) {
    const Vector3 hsv = ColorToHSV(c);
    float h = hsv.x / 360.0f + shift.x;
    h -= floorf(h);
    Color out = ColorFromHSV(h * 360.0f, Clamp01(hsv.y + shift.y), Clamp01(hsv.z + shift.z));
    out.a = c.a;
    return out;
}

void GameData_ResetOptions(void) {
    /* Mods desativados e o esquema Custom nao sao config: ficam como estao. */
    char mods[MOD_DISABLED_MAX][MOD_NAME_MAX];
    int modCount = gameDisabledModCount;
    memcpy(mods, gameDisabledMods, sizeof(mods));

    int customKeys[4];
    memcpy(customKeys, game_scheme_keys[SCHEME_CUSTOM], sizeof(customKeys));

    GameData_Defaults();

    gameDisabledModCount = modCount;
    memcpy(gameDisabledMods, mods, sizeof(mods));
    memcpy(game_scheme_keys[SCHEME_CUSTOM], customKeys, sizeof(customKeys));
}

/* Qualquer valor fora da faixa volta ao padrao (float) ou e limitado a faixa (char/int). */
static int Clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

static void ClampField(const SaveField* f) {
    switch (f->kind) {
        case SF_CHAR: { char* p = f->ptr; *p = (char)Clamp(*p, (int)f->lo, (int)f->hi); break; }
        case SF_INT:  { int* p = f->ptr;  *p = Clamp(*p, (int)f->lo, (int)f->hi); break; }
        case SF_FLOAT: {
            float* p = f->ptr;
            if (!(*p > f->lo && *p < f->hi)) *p = (float)f->def;
            break;
        }
        default: break;
    }
}

/* Campos que nao sao escalares: arrays, texto, teclas e mods. */
static void GameData_SanitizeRaw(void) {
    GameOptions* o = &gameOptions;

    for (int lane = 0; lane < 4; lane++) {
        o->laneHSB[lane][0] = (short)Clamp(o->laneHSB[lane][0], -180, 180);
        o->laneHSB[lane][1] = (short)Clamp(o->laneHSB[lane][1], -100, 100);
        o->laneHSB[lane][2] = (short)Clamp(o->laneHSB[lane][2], -100, 100);
    }

    o->menuMusic[MENU_MUSIC_NAME_MAX - 1] = 0;

    gameDisabledModCount = Clamp(gameDisabledModCount, 0, MOD_DISABLED_MAX);
    for (int i = 0; i < gameDisabledModCount; i++)
        gameDisabledMods[i][MOD_NAME_MAX - 1] = 0;

    static const int defaultArrows[4] = {KEY_LEFT, KEY_DOWN, KEY_UP, KEY_RIGHT};
    for (int k = 0; k < 4; k++)
        if (game_scheme_keys[SCHEME_CUSTOM][k] <= 0)
            game_scheme_keys[SCHEME_CUSTOM][k] = defaultArrows[k];

    for (int p = 0; p < 2; p++) {
        o->coopName[p][COOP_NAME_MAX - 1] = 0;
        for (int k = 0; k < 4; k++)
            if (o->coopKeys[p][k] <= 0) o->coopKeys[p][k] = coopDefaultKeys[p][k];
    }
}

static void GameData_Sanitize(void) {
    for (size_t i = 0; i < SAVE_FIELD_COUNT; i++)
        ClampField(&saveFields[i]);
    GameData_SanitizeRaw();
}

/* ------------------------------------------------------------------ */
/* Leitura e escrita                                                   */
/* ------------------------------------------------------------------ */

static const SaveField* FindField(uint16_t id) {
    for (size_t i = 0; i < SAVE_FIELD_COUNT; i++)
        if (saveFields[i].id == id)
            return &saveFields[i];
    return NULL;
}

/* Le o arquivo inteiro. Retorna 0 so se o magic nao bate (arquivo que nao e um save nosso). */
static char GameData_LoadFile(FILE* f) {
    char magic[4];
    uint16_t version = 0; /* lida e ignorada: registros sao autodescritos */
    if (fread(magic, 1, sizeof(magic), f) != sizeof(magic) || memcmp(magic, SAVE_MAGIC, sizeof(magic)) != 0)
        return 0;
    if (fread(&version, sizeof(version), 1, f) != 1)
        return 1; /* so o magic, sem registros: usa padroes e regrava */

    uint16_t head[2]; /* id, tamanho */
    while (fread(head, sizeof(uint16_t), 2, f) == 2) {
        const uint16_t id = head[0];
        const uint16_t size = head[1];

        if (id == SAVE_ID_MODS) {
            const uint16_t cap = (uint16_t)sizeof(gameDisabledMods);
            const uint16_t n = size < cap ? size : cap;
            memset(gameDisabledMods, 0, sizeof(gameDisabledMods));
            if (fread(gameDisabledMods, 1, n, f) != n) break;
            gameDisabledModCount = n / MOD_NAME_MAX;
            if (size > n) fseek(f, size - n, SEEK_CUR);
            continue;
        }

        const SaveField* field = FindField(id);
        if (field == NULL) {
            fseek(f, size, SEEK_CUR); /* campo desconhecido: pula */
            continue;
        }

        const uint16_t n = size < field->size ? size : field->size;
        if (fread(field->ptr, 1, n, f) != n) break;
        if (size > n) fseek(f, size - n, SEEK_CUR);
    }
    return 1;
}

static int GameData_WriteRecord(FILE* f, uint16_t id, const void* data, uint16_t size) {
    const uint16_t head[2] = { id, size };
    if (fwrite(head, sizeof(uint16_t), 2, f) != 2) return 0;
    return size == 0 || fwrite(data, 1, size, f) == size;
}

static int GameData_WriteFile(FILE* f) {
    if (fwrite(SAVE_MAGIC, 1, sizeof(SAVE_MAGIC) - 1, f) != sizeof(SAVE_MAGIC) - 1) return 0;
    const uint16_t version = SAVE_VERSION;
    if (fwrite(&version, sizeof(version), 1, f) != 1) return 0;

    for (size_t i = 0; i < SAVE_FIELD_COUNT; i++)
        if (!GameData_WriteRecord(f, saveFields[i].id, saveFields[i].ptr, saveFields[i].size))
            return 0;

    const uint16_t modsBytes = (uint16_t)(gameDisabledModCount * MOD_NAME_MAX);
    return GameData_WriteRecord(f, SAVE_ID_MODS, gameDisabledMods, modsBytes);
}

void GameData_Start(void) {
    GameData_Defaults();

    char loaded = 0;
    FILE* f = fopen(SAVE_FILE, "rb");
    if (f != NULL) {
        loaded = (char)GameData_LoadFile(f);
        fclose(f);
        if (!loaded) { /* nao e um save nosso: guarda uma copia antes de sobrescrever */
            remove(SAVE_BAK);
            if (rename(SAVE_FILE, SAVE_BAK) != 0)
                Log_WarnF("[ SAVE ] nao foi possivel guardar %s", SAVE_BAK);
        }
    }

    GameData_Sanitize();
    if (!loaded)
        GameData_Flush(); /* primeiro uso ou save invalido: grava o padrao */
}

void GameData_Flush(void) {
    FILE* f = fopen(SAVE_TMP, "wb");
    if (f == NULL) {
        Log_WarnF("[ SAVE ] nao foi possivel gravar %s", SAVE_TMP);
        return;
    }

    const int ok = GameData_WriteFile(f);
    if (fclose(f) != 0 || !ok) {
        remove(SAVE_TMP);
        Log_WarnF("[ SAVE ] falha ao gravar %s (save antigo mantido)", SAVE_TMP);
        return;
    }

    /* Windows: rename nao sobrescreve, entao remove o antigo antes. */
    remove(SAVE_FILE);
    if (rename(SAVE_TMP, SAVE_FILE) != 0)
        Log_WarnF("[ SAVE ] nao foi possivel trocar %s", SAVE_FILE);
}

void GameData_Stop(void) {
    GameData_Flush();
}
