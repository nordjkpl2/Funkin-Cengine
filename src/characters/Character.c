/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * Character.c - API publica de personagem (bf/dad), icone, resolvedor de contexto e pre-decodificacao.
 */

#include "characters/Character.h"
#include "characters/CharacterInternal.h"
#include "logic/animations/CharacterAnims.h"
#include "characters/CharacterJson.h"
#include "characters/CharacterXML.h"
#include "core/ModPath.h"
#include "core/ModList.h"
#include "core/Log.h"
#include "rendering/cpu/ImagePrefetch.h"

#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static CharCtx songCtx; /* mod da musica atual (Character_SetSongDir) */

/* ------------------------------------------------------------------------- */
/* Caminhos e textos                                                         */
/* ------------------------------------------------------------------------- */

bool Char_IsSafeName(const char *name)
{
    if (!name || !name[0] || strstr(name, ".."))
        return false;

    for (const unsigned char *p = (const unsigned char *)name; *p; ++p) {
        if (*p == '/' || *p == '\\' || *p == ':' || *p < 32)
            return false;
    }
    return true;
}

void Char_CopyString(char *out, size_t capacity, const char *source)
{
    if (!out || !capacity)
        return;
    if (!source) {
        out[0] = '\0';
        return;
    }

    size_t length = strlen(source);
    if (length >= capacity)
        length = capacity - 1u;
    memcpy(out, source, length);
    out[length] = '\0';
}

static const char *Char_BaseName(const char *path)
{
    const char *forward = strrchr(path, '/');
    const char *backward = strrchr(path, '\\');
    const char *last = (!forward || (backward && backward > forward)) ? backward : forward;
    return last ? last + 1 : path;
}

/* "pasta/nome.png" -> "nome" */
void Char_StemName(const char *source, char *out, size_t capacity)
{
    Char_CopyString(out, capacity, Char_BaseName(source));
    char *extension = strrchr(out, '.');
    if (extension)
        *extension = '\0';
}

/* Procura um arquivo no mod e depois em assets/. `fmt` monta o caminho relativo
   (ex.: "images/characters/%s.png"). Devolve o caminho completo em `out`. */
bool Char_Find(const CharCtx *ctx, char out[CHAR_PATH_MAX],
                      const char *fmt, ...) __attribute__((format(printf, 3, 4)));

bool Char_Find(const CharCtx *ctx, char out[CHAR_PATH_MAX],
                      const char *fmt, ...)
{
    char relative[CHAR_PATH_MAX];
    va_list args;
    va_start(args, fmt);
    int length = vsnprintf(relative, sizeof(relative), fmt, args);
    va_end(args);
    if (length <= 0 || (size_t)length >= sizeof(relative))
        return false;

    return ModPath_Find(ctx->root, relative, out, CHAR_PATH_MAX);
}

/* .animset + .png do MESMO lugar (mod ou assets/). Misturar um animset do mod
   com o png de assets/ (ou o contrario) embaralha os frames. */
bool Char_FindPair(const CharCtx *ctx, char setOut[CHAR_PATH_MAX], char pngOut[CHAR_PATH_MAX],
                          const char *setName, const char *pngName)
{
    for (int pass = 0; pass < 2; ++pass) {
        const char *root = pass == 0 ? ctx->root : "assets";
        if (!root[0])
            continue;

        char setRel[CHAR_PATH_MAX], pngRel[CHAR_PATH_MAX];
        int a = snprintf(setRel, sizeof(setRel), "images/characters/%s.animset", setName);
        int b = snprintf(pngRel, sizeof(pngRel), "images/characters/%s.png", pngName);
        if (a > 0 && a < CHAR_PATH_MAX && b > 0 && b < CHAR_PATH_MAX &&
            ModPath_FindIn(root, setRel, setOut, CHAR_PATH_MAX) &&
            ModPath_FindIn(root, pngRel, pngOut, CHAR_PATH_MAX))
            return true;
    }
    return false;
}

/* Contexto de busca para um personagem. Vale o mod da musica (e assets/); se o personagem nao
   existe ali, usa o 1o mod ativo que o tem (ModList), senao o contexto da musica. */
void Char_ResolveCtx(CharCtx *out, const char *name)
{
    *out = songCtx;
    if (!name || !Char_IsSafeName(name))
        return;

    char path[CHAR_PATH_MAX];
    if (Char_Find(&songCtx, path, "images/characters/%s.animset", name) ||
        Char_Find(&songCtx, path, "images/characters/%s.xml", name) ||
        Char_Find(&songCtx, path, "characters/%s.json", name))
        return;

    char root[CHAR_PATH_MAX];
    if (ModList_FindCharacterRoot(name, root, sizeof(root)))
        snprintf(out->root, sizeof(out->root), "%s", root);
}

/* Psych: width/height = frameWidth/Height do atlas * escala, e a escala cresce a
   partir do centro do frame (o playAnim troca o offset do updateHitbox). Le o
   primeiro SubTexture do XML do atlas (mesmo nome do png). Sem XML: box = 0 e
   shift = 0, e o chamador usa o tamanho do frame. */
static float Char_TagNumber(const char *tag, const char *tagEnd, const char *key, float fallback)
{
    const size_t keyLength = strlen(key);
    for (const char *p = tag; p + keyLength + 2u < tagEnd; ++p) {
        const bool boundary = p == tag || p[-1] == ' ' || p[-1] == '\t' || p[-1] == '\n' || p[-1] == '\r';
        if (boundary && !strncmp(p, key, keyLength) && p[keyLength] == '=' &&
            (p[keyLength + 1u] == '"' || p[keyLength + 1u] == '\''))
            return strtof(p + keyLength + 2u, NULL);
    }
    return fallback;
}

void Char_PsychBox(const char *pngPath, float scale, Vector2 *box, Vector2 *shift)
{
    *box = (Vector2){ 0.0f, 0.0f };
    *shift = (Vector2){ 0.0f, 0.0f };

    char xmlPath[CHAR_PATH_MAX];
    Char_CopyString(xmlPath, sizeof(xmlPath), pngPath);
    char *extension = strrchr(xmlPath, '.');
    if (!extension || strlen(extension) != 4u)
        return;
    memcpy(extension, ".xml", 5u);

    FILE *file = fopen(xmlPath, "rb");
    if (!file)
        return;
    char text[4096];
    const size_t length = fread(text, 1, sizeof(text) - 1u, file);
    fclose(file);
    text[length] = '\0';

    const char *tag = strstr(text, "<SubTexture");
    const char *tagEnd = tag ? strchr(tag, '>') : NULL;
    if (!tagEnd)
        return;

    const float width = Char_TagNumber(tag, tagEnd, "frameWidth", Char_TagNumber(tag, tagEnd, "width", 0.0f));
    const float height = Char_TagNumber(tag, tagEnd, "frameHeight", Char_TagNumber(tag, tagEnd, "height", 0.0f));
    if (width <= 0.0f || height <= 0.0f)
        return;

    const float grow = fabsf(scale);
    *box = (Vector2){ width * grow, height * grow };
    if (fabsf(grow - 1.0f) > 0.0001f)
        *shift = (Vector2){ -0.5f * width * (grow - 1.0f), -0.5f * height * (grow - 1.0f) };
}

/* Em pares {sprite, nome}: pula vazio e repetido. */
bool Char_SkipName(const char *const names[2], int i)
{
    return !names[i] || !names[i][0] || (i == 1 && !strcmp(names[0], names[1]));
}

void Character_SetSongDir(const char *songDir)
{
    songCtx.root[0] = '\0';
    if (!songDir || !songDir[0])
        return;

    (void)ModPath_Root(songDir, songCtx.root, sizeof(songCtx.root));
}


/* ------------------------------------------------------------------------- */
/* Icone da barra de vida                                                    */
/* ------------------------------------------------------------------------- */

/* healthIcon (do JSON) -> nome do personagem -> icone padrao (bf ou dad). */
void Char_LoadIcon(const CharCtx *ctx, Character *character,
                          const char *name, const char *healthIcon, CharRole role)
{
    const char *iconName = (healthIcon && healthIcon[0] && Char_IsSafeName(healthIcon))
                               ? healthIcon : name;
    char path[CHAR_PATH_MAX];

    /* Ordem: nome, depois "icon-nome" (aceita icon-fsm.png e fsm.png). */
    bool found = Char_Find(ctx, path, "images/icons/%s.png", iconName);
    if (!found && strcmp(iconName, name) != 0)
        found = Char_Find(ctx, path, "images/icons/%s.png", name);
    if (!found)
        found = Char_Find(ctx, path, "images/icons/icon-%s.png", iconName);
    if (!found && strcmp(iconName, name) != 0)
        found = Char_Find(ctx, path, "images/icons/icon-%s.png", name);

    if (!found) {
        Log_WarnF("Icone nao encontrado: '%s' (usando padrao)", iconName);
        Char_CopyString(path, sizeof(path),
            role == CHAR_DAD ? "assets/images/icons/dad.png"
                             : "assets/images/icons/bf.png");
    }
    character->icon = Render_LoadTexture(path);
}

/* ------------------------------------------------------------------------- */
/* API: bf / dad                                                             */
/* ------------------------------------------------------------------------- */

void Character_Load(Character *restrict character,
                    const char *restrict characterName,
                    CharRole role)
{
    if (!character)
        return;

    CharCtx resolvedCtx;
    Char_ResolveCtx(&resolvedCtx, characterName);
    const CharCtx *ctx = &resolvedCtx;

    Render_DefaultAnimated(&character->object);
    character->idled = false;
    character->psychCamera = false;
    character->psychBox = (Vector2){ 0.0f, 0.0f };
    character->psychShift = (Vector2){ 0.0f, 0.0f };
    character->idleTimer = 0.0f;
    character->singDuration = 4.0f;
    character->hasHealthColor = false;
    character->healthColor = (Color){ 255, 255, 255, 255 };
    Char_InitAnimationSlots(character);

    if (!Char_IsSafeName(characterName))
        characterName = (role == CHAR_DAD) ? "dad" : "bf";

    /* XML tem prioridade, inclusive para mods que trocam o dad/bf. */
    if (Char_TryLoadXMLCharacter(ctx, character, characterName, role))
        return;

    const bool isDadName = !strcmp(characterName, "dad");
    const bool isBfName = !strcmp(characterName, "bf");
    const bool isGfName = !strcmp(characterName, "gf");

    if (!isDadName && !isBfName && !isGfName &&
        Char_LoadLegacyCustom(ctx, character, characterName, role)) {
        return;
    }

    /* Nome desconhecido: usa o padrao do lado (dad) ou o bf. */
    if (!isDadName && !isBfName && !isGfName)
        Log_WarnF("Personagem '%s' nao encontrado: usando %s padrao", characterName,
                  role == CHAR_DAD ? "dad" : "bf");
    const CharRole builtin = (role == CHAR_DAD || isDadName) ? CHAR_DAD : CHAR_BF;
    if (Char_LoadBuiltin(ctx, character, builtin))
        return;
    if (!isBfName && Char_LoadBuiltin(ctx, character, CHAR_BF))
        return;

    Log_Fatal("Character_Load: could not load character assets");
}

/* Frame de derrota (2o frame do icone). Sem ele, cai no frame normal. */
Rectangle Character_IconLoseFrame(Texture2D icon)
{
    Rectangle r = Character_IconFrame(icon);
    if (icon.id != 0 && icon.height > 0 && (float)icon.width >= 2.0f * (float)icon.height)
        r.x = r.width;
    return r;
}

Rectangle Character_IconFrame(Texture2D icon)
{
    if (icon.id == 0 || icon.height <= 0)
        return (Rectangle){ 0.0f, 0.0f, 150.0f, 150.0f };

    /* 2 ou 3 frames lado a lado; o frame nunca passa da largura/2 */
    const float height = (float)icon.height;
    float width = height;
    if (width * 2.0f > (float)icon.width)
        width = (float)icon.width * 0.5f;
    return (Rectangle){ 0.0f, 0.0f, width, height };
}

void Character_LoadDeathAnimations(Character *restrict character, int *restrict anims)
{
    if (!character || !anims)
        return;

    RayAnimationHandler *set = &character->object.animationSet;

    static const char *const startNames[] = { "firstDeath", "deathStart", "BF dies" };
    static const char *const loopNames[] = { "deathLoop", "BF Dead Loop" };
    static const char *const confirmNames[] = { "deathConfirm", "BF Dead confirm" };

    anims[0] = CHAR_FIND_ANY(set, startNames);
    anims[1] = CHAR_FIND_ANY(set, loopNames);
    anims[2] = CHAR_FIND_ANY(set, confirmNames);

    const int fallback = character->animations.idle >= 0 ? character->animations.idle : 0;
    for (int i = 0; i < 3; ++i) {
        if (anims[i] < 0)
            anims[i] = fallback;
    }

    if (anims[1] >= 0 && anims[1] < set->animationCount)
        set->animations[anims[1]].looped = true;
}

/* ------------------------------------------------------------------------- */
/* Pre-decodificacao do png (mesma ordem de busca de Character_Load/Girlfriend_Load) */
/* ------------------------------------------------------------------------- */

void Character_PrefetchTexture(const char *name, CharRole role)
{
    char pngPath[CHAR_PATH_MAX], setPath[CHAR_PATH_MAX];
    char xmlPath[CHAR_PATH_MAX], atlasPath[CHAR_PATH_MAX], spriteName[128];

    if (!Char_IsSafeName(name))
        name = (role == CHAR_GF) ? "gf" : (role == CHAR_DAD) ? "dad" : "bf";

    CharCtx resolvedCtx;
    Char_ResolveCtx(&resolvedCtx, name);
    const CharCtx *ctx = &resolvedCtx;

    /* 1) XML (Codename) */
    if (Char_FindCharacterXML(ctx, name, xmlPath, spriteName) &&
        Char_FindAtlasXML(ctx, spriteName, name, xmlPath, atlasPath) &&
        Char_FindCharacterPNG(ctx, spriteName, name, pngPath)) {
        ImagePrefetch_Start(pngPath);
        return;
    }

    const bool isDad = !strcmp(name, "dad");
    const bool isBf = !strcmp(name, "bf");
    const bool isGf = !strcmp(name, "gf");

    /* 2) animset + png (+ characters/<nome>.json); a gf sempre tenta, bf/dad so com nome custom */
    if (role == CHAR_GF || (!isDad && !isBf && !isGf)) {
        CharJson json;
        char jsonPath[CHAR_PATH_MAX];
        Char_JsonInit(&json);
        if (Char_Find(ctx, jsonPath, "characters/%s.json", name))
            Char_ReadJson(jsonPath, &json);

        const char *textureName = json.imageBase[0] ? json.imageBase : name;
        if (Char_FindPair(ctx, setPath, pngPath, name, textureName) ||
            (strcmp(textureName, name) != 0 && Char_FindPair(ctx, setPath, pngPath, name, name))) {
            ImagePrefetch_Start(pngPath);
            return;
        }
        if (role == CHAR_GF) {
            if (!isGf && Char_FindPair(ctx, setPath, pngPath, "gf", "gf"))
                ImagePrefetch_Start(pngPath);
            return;
        }
    }

    /* 3) padrao (bf/dad) */
    const BuiltinDef *def = (role == CHAR_DAD || isDad) ? &BUILTIN_DAD : &BUILTIN_BF;
    if (Char_FindPair(ctx, setPath, pngPath, def->name, def->name))
        ImagePrefetch_Start(pngPath);
}
