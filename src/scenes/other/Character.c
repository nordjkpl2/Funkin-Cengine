/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * Character.c - carrega bf, dad (oponente) e gf.
 *
 * Ordem de busca de cada personagem:
 *   1. XML (Codename)         characters/<nome>.xml + atlas Sparrow
 *   2. animset + JSON (Psych) images/characters/<nome>.animset, characters/<nome>.json
 *   3. bf/dad padrao do jogo  (tabelas BUILTIN_*)
 * Todo arquivo e procurado primeiro no mod da musica e depois em assets/.
 */

#include "scenes/Character.h"
#include "scenes/ModPath.h"
#include "scenes/AnimationSets_XML.h"
#include "Log.h"
#include "yyjson.h"
#include "yxml.h"

#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHAR_PATH_MAX       512
#define CHAR_MAX_DEFS       48
#define CHAR_JSON_MAX_SIZE  (1024 * 1024)
#define CHAR_XML_STACK_SIZE 2048u
#define CHAR_XML_READ_SIZE  4096u

/* Onde procurar: pasta do mod ("" = so assets/). Passado por parametro para
   o carregamento poder rodar fora da thread principal no futuro. */
typedef struct {
    char root[CHAR_PATH_MAX];
} CharCtx;

static CharCtx songCtx; /* mod da musica atual (Character_SetSongDir) */

/* ------------------------------------------------------------------------- */
/* Caminhos e textos                                                         */
/* ------------------------------------------------------------------------- */

static bool Char_IsSafeName(const char *name)
{
    if (!name || !name[0] || strstr(name, ".."))
        return false;

    for (const unsigned char *p = (const unsigned char *)name; *p; ++p) {
        if (*p == '/' || *p == '\\' || *p == ':' || *p < 32)
            return false;
    }
    return true;
}

static void Char_CopyString(char *out, size_t capacity, const char *source)
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
static void Char_StemName(const char *source, char *out, size_t capacity)
{
    Char_CopyString(out, capacity, Char_BaseName(source));
    char *extension = strrchr(out, '.');
    if (extension)
        *extension = '\0';
}

/* Procura um arquivo no mod e depois em assets/. `fmt` monta o caminho relativo
   (ex.: "images/characters/%s.png"). Devolve o caminho completo em `out`. */
static bool Char_Find(const CharCtx *ctx, char out[CHAR_PATH_MAX],
                      const char *fmt, ...) __attribute__((format(printf, 3, 4)));

static bool Char_Find(const CharCtx *ctx, char out[CHAR_PATH_MAX],
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
static bool Char_FindPair(const CharCtx *ctx, char setOut[CHAR_PATH_MAX], char pngOut[CHAR_PATH_MAX],
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

static void Char_PsychBox(const char *pngPath, float scale, Vector2 *box, Vector2 *shift)
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
static bool Char_SkipName(const char *const names[2], int i)
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
/* Animacoes                                                                 */
/* ------------------------------------------------------------------------- */

static int Char_FindAnimationAny(const RayAnimationHandler *set,
                                 const char *const *names, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        int index = AnimationSet_FindAnimation(set, names[i]);
        if (index >= 0)
            return index;
    }
    return -1;
}

#define CHAR_FIND_ANY(set, list) \
    Char_FindAnimationAny((set), (list), sizeof(list) / sizeof((list)[0]))

/* Ajusta fps/loop/offset de uma animacao pelo nome. -1 se nao existe. */
static int Char_ConfigureAnimation(RayAnimationHandler *set, const char *name,
                                   float fps, bool looped,
                                   float offsetX, float offsetY)
{
    int index = name ? AnimationSet_FindAnimation(set, name) : -1;
    if (index < 0)
        return -1;

    RayAnimation *animation = &set->animations[index];
    if (fps <= 0.0f)
        fps = 24.0f;

    animation->fps = fps;
    animation->frameTime = 1.0f / fps;
    animation->looped = looped;
    animation->animationOffset = (Vector2){ offsetX, offsetY };
    return index;
}

static void Char_InitAnimationSlots(Character *character)
{
    character->animations.idle = -1;
    for (int i = 0; i < 4; ++i) {
        character->animations.notes[i] = -1;
        character->missAnimations.notes[i] = -1;
    }
}

/* Camera padrao quando o personagem nao define uma. */
static Vector2 Char_DefaultCamera(CharRole role)
{
    return role == CHAR_DAD ? (Vector2){ 400.0f, 300.0f }
                            : (Vector2){ 50.0f, 50.0f };
}

/* Nomes comuns de animacao (Codename / Psych) para cada pose. */
static void Char_ApplyXMLAnimationSlots(Character *character,
                                        const RayAnimationHandler *set)
{
    static const char *const idleNames[] = { "idle", "dance", "danceIdle" };
    static const char *const singNames[4][3] = {
        { "singLEFT", "singLeft", "left" },
        { "singDOWN", "singDown", "down" },
        { "singUP", "singUp", "up" },
        { "singRIGHT", "singRight", "right" }
    };
    static const char *const missNames[4][4] = {
        { "singLEFTmiss", "singLeftMiss", "missLEFT", "singLEFT_miss" },
        { "singDOWNmiss", "singDownMiss", "missDOWN", "singDOWN_miss" },
        { "singUPmiss", "singUpMiss", "missUP", "singUP_miss" },
        { "singRIGHTmiss", "singRightMiss", "missRIGHT", "singRIGHT_miss" }
    };

    character->animations.idle = CHAR_FIND_ANY(set, idleNames);
    for (int i = 0; i < 4; ++i) {
        character->animations.notes[i] = CHAR_FIND_ANY(set, singNames[i]);
        character->missAnimations.notes[i] = CHAR_FIND_ANY(set, missNames[i]);
    }

    if (character->animations.idle < 0 && set->animationCount > 0)
        character->animations.idle = 0;
}

/* ------------------------------------------------------------------------- */
/* Icone da barra de vida                                                    */
/* ------------------------------------------------------------------------- */

/* healthIcon (do JSON) -> nome do personagem -> icone padrao (bf ou dad). */
static void Char_LoadIcon(const CharCtx *ctx, Character *character,
                          const char *name, const char *healthIcon, CharRole role)
{
    const char *iconName = (healthIcon && healthIcon[0] && Char_IsSafeName(healthIcon))
                               ? healthIcon : name;
    char path[CHAR_PATH_MAX];

    bool found = Char_Find(ctx, path, "images/icons/%s.png", iconName);
    if (!found && strcmp(iconName, name) != 0)
        found = Char_Find(ctx, path, "images/icons/%s.png", name);

    if (!found) {
        Log_WarnF("Icone nao encontrado: '%s' (usando padrao)", iconName);
        Char_CopyString(path, sizeof(path),
            role == CHAR_DAD ? "assets/images/icons/dad.png"
                             : "assets/images/icons/bf.png");
    }
    character->icon = Render_LoadTexture(path);
}

/* ------------------------------------------------------------------------- */
/* Personagem em XML (Codename)                                              */
/* ------------------------------------------------------------------------- */

/* Le so o atributo sprite="..." da tag <character>; o parse completo fica
   em AnimationSet_LoadCharacterXML(). */
static bool Char_ReadXMLSprite(const char *path, char *sprite, size_t capacity)
{
    FILE *file = fopen(path, "rb");
    if (!file)
        return false;

    yxml_t parser;
    unsigned char stack[CHAR_XML_STACK_SIZE];
    unsigned char input[CHAR_XML_READ_SIZE];
    yxml_init(&parser, stack, sizeof(stack));

    bool foundRoot = false;
    bool inCharacterTag = false;
    bool readingSprite = false;
    bool ok = true;
    size_t length = 0;
    size_t bytesRead;

    sprite[0] = '\0';

    while (ok && (bytesRead = fread(input, 1, sizeof(input), file)) != 0) {
        for (size_t i = 0; i < bytesRead && ok; ++i) {
            yxml_ret_t token = yxml_parse(&parser, input[i]);
            if (token < 0) {
                ok = false;
                break;
            }

            switch (token) {
                case YXML_ELEMSTART:
                    if (!strcmp(parser.elem, "character"))
                        foundRoot = inCharacterTag = true;
                    break;

                case YXML_ATTRSTART:
                    readingSprite = inCharacterTag && !strcmp(parser.attr, "sprite");
                    break;

                case YXML_ATTRVAL:
                    if (readingSprite) {
                        size_t part = strlen(parser.data);
                        if (part >= capacity - length) {
                            ok = false;
                            break;
                        }
                        memcpy(sprite + length, parser.data, part + 1u);
                        length += part;
                    }
                    break;

                case YXML_ATTREND:
                    readingSprite = false;
                    break;

                case YXML_ELEMEND:
                    if (!strcmp(parser.elem, "character"))
                        inCharacterTag = false;
                    break;

                default:
                    break;
            }
        }
    }

    ok = ok && !ferror(file) && yxml_eof(&parser) >= 0;
    fclose(file);
    return ok && foundRoot;
}

static bool Char_FindCharacterXML(const CharCtx *ctx, const char *name,
                                  char xmlPath[CHAR_PATH_MAX], char spriteName[128])
{
    static const char *const formats[] = {
        "characters/%s.xml",
        "data/characters/%s.xml",
        "images/characters/%s.character.xml",
        "images/characters/%s-char.xml",
        "images/characters/%s_char.xml",
        "images/characters/%s.xml"
    };

    for (size_t i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i) {
        char candidate[CHAR_PATH_MAX];
        char rawSprite[128];

        if (!Char_Find(ctx, candidate, formats[i], name) ||
            !Char_ReadXMLSprite(candidate, rawSprite, sizeof(rawSprite))) {
            continue;
        }

        Char_CopyString(xmlPath, CHAR_PATH_MAX, candidate);
        Char_StemName(rawSprite[0] ? rawSprite : name, spriteName, 128);
        if (!Char_IsSafeName(spriteName))
            Char_CopyString(spriteName, 128, name);
        return true;
    }
    return false;
}

static bool Char_FindAtlasXML(const CharCtx *ctx, const char *sprite, const char *name,
                              const char *characterXML, char atlasPath[CHAR_PATH_MAX])
{
    static const char *const formats[] = {
        "images/characters/%s.xml",
        "images/characters/%s.atlas.xml",
        "images/characters/%s-atlas.xml",
        "images/characters/%s_atlas.xml",
        "images/characters/%sAtlas.xml"
    };
    const char *const names[2] = { sprite, name };

    for (int n = 0; n < 2; ++n) {
        if (Char_SkipName(names, n))
            continue;

        for (size_t i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i) {
            char candidate[CHAR_PATH_MAX];
            /* o proprio XML do personagem nao serve de atlas */
            if (Char_Find(ctx, candidate, formats[i], names[n]) &&
                strcmp(candidate, characterXML) != 0) {
                Char_CopyString(atlasPath, CHAR_PATH_MAX, candidate);
                return true;
            }
        }
    }
    return false;
}

static bool Char_FindCharacterPNG(const CharCtx *ctx, const char *sprite, const char *name,
                                  char pngPath[CHAR_PATH_MAX])
{
    const char *const names[2] = { sprite, name };

    for (int i = 0; i < 2; ++i) {
        if (!Char_SkipName(names, i) &&
            Char_Find(ctx, pngPath, "images/characters/%s.png", names[i])) {
            return true;
        }
    }
    return false;
}

static void Char_ApplyXMLMetadata(const CharCtx *ctx, Character *character,
                                  RayCharacterXMLData *data,
                                  const char *name, CharRole role)
{
    character->object.animationSet = data->animations;
    data->animations = (RayAnimationHandler){0};

    const float scale = data->hasScale ? data->scale : 1.0f;
    character->object.scaleX = data->flipX ? -scale : scale;
    character->object.scaleY = scale;

    if (data->hasX)
        character->object.position.x = data->x;
    if (data->hasY)
        character->object.position.y = data->y;
    if (data->hasColor)
        character->object.color = data->color;
    if (data->holdTime > 0.0f)
        character->singDuration = data->holdTime;

    Vector2 camera = Char_DefaultCamera(role);
    if (data->hasCameraX)
        camera.x = data->cameraX;
    if (data->hasCameraY)
        camera.y = data->cameraY;
    character->cameraOffset = camera;

    Char_ApplyXMLAnimationSlots(character, &character->object.animationSet);
    AnimatedObject_SetAnimation(&character->object, character->animations.idle);

    if (role != CHAR_GF)
        Char_LoadIcon(ctx, character, name, NULL, role);
}

static bool Char_TryLoadXMLCharacter(const CharCtx *ctx, Character *character,
                                     const char *name, CharRole role)
{
    char characterXML[CHAR_PATH_MAX];
    char atlasXML[CHAR_PATH_MAX];
    char pngPath[CHAR_PATH_MAX];
    char spriteName[128];

    if (!Char_FindCharacterXML(ctx, name, characterXML, spriteName) ||
        !Char_FindAtlasXML(ctx, spriteName, name, characterXML, atlasXML) ||
        !Char_FindCharacterPNG(ctx, spriteName, name, pngPath)) {
        return false;
    }

    Texture2D texture = Render_LoadTexture(pngPath);
    if (!texture.id)
        return false;

    RayCharacterXMLData data = {0};
    if (!AnimationSet_LoadCharacterXML(characterXML, atlasXML, texture, &data)) {
        UnloadTexture(texture);
        return false;
    }

    if (!data.antialiasing)
        SetTextureFilter(data.animations.texture, TEXTURE_FILTER_POINT);

    Char_ApplyXMLMetadata(ctx, character, &data, name, role);
    return true;
}

/* ------------------------------------------------------------------------- */
/* Personagem animset + JSON (Psych)                                         */
/* ------------------------------------------------------------------------- */

/* Papel de uma animacao no jogo ("idle", "singLEFT"...) ligado ao nome dela no atlas. */
typedef struct {
    char role[16];
    char sprite[64];
    int fps;
    bool looped;
    float offsetX;
    float offsetY;
} CharAnimDef;

typedef struct {
    float scale;
    bool flipX;
    Vector2 camera;          /* camera_position (0,0 = nao definida) */
    Vector2 position;        /* position */
    float singDuration;      /* sing_duration */
    bool hasHealthColor;     /* healthbar_colors */
    Color healthColor;
    char healthIcon[64];     /* healthicon */
    char imageBase[64];      /* image (nome do png, sem pasta/extensao) */
    CharAnimDef defs[CHAR_MAX_DEFS];
    int defCount;
} CharJson;

/* yyjson_get_real devolve 0 para inteiros: aceita int e real. */
static bool Char_JsonNumber(yyjson_val *value, double *out)
{
    if (yyjson_is_real(value)) *out = yyjson_get_real(value);
    else if (yyjson_is_sint(value)) *out = (double)yyjson_get_sint(value);
    else if (yyjson_is_uint(value)) *out = (double)yyjson_get_uint(value);
    else return false;
    return *out == *out; /* descarta NaN */
}

/* Le [x, y]; so mexe no que for numero. */
static void Char_JsonVec2(yyjson_val *array, float *x, float *y)
{
    double number;
    if (!yyjson_is_arr(array) || yyjson_arr_size(array) < 2u)
        return;
    if (Char_JsonNumber(yyjson_arr_get(array, 0), &number)) *x = (float)number;
    if (Char_JsonNumber(yyjson_arr_get(array, 1), &number)) *y = (float)number;
}

static void Char_JsonReadColor(yyjson_val *array, CharJson *json)
{
    if (!yyjson_is_arr(array) || yyjson_arr_size(array) < 3u)
        return;

    double channel[3];
    for (size_t i = 0; i < 3u; ++i) {
        if (!Char_JsonNumber(yyjson_arr_get(array, i), &channel[i]))
            return;
        if (channel[i] < 0.0) channel[i] = 0.0;
        if (channel[i] > 255.0) channel[i] = 255.0;
    }
    json->hasHealthColor = true;
    json->healthColor = (Color){ (unsigned char)channel[0], (unsigned char)channel[1],
                                 (unsigned char)channel[2], 255 };
}

static bool Char_JsonReadDef(yyjson_val *entry, CharAnimDef *def)
{
    if (!yyjson_is_obj(entry))
        return false;

    yyjson_val *role = yyjson_obj_get(entry, "anim");
    yyjson_val *sprite = yyjson_obj_get(entry, "name");
    if (!yyjson_is_str(role) || !yyjson_is_str(sprite) ||
        !yyjson_get_str(role)[0] || !yyjson_get_str(sprite)[0]) {
        return false;
    }

    Char_CopyString(def->role, sizeof(def->role), yyjson_get_str(role));
    Char_CopyString(def->sprite, sizeof(def->sprite), yyjson_get_str(sprite));
    def->fps = 24;
    def->looped = false;
    def->offsetX = 0.0f;
    def->offsetY = 0.0f;

    double fps;
    if (Char_JsonNumber(yyjson_obj_get(entry, "fps"), &fps) && fps >= 1.0 && fps <= 1000.0)
        def->fps = (int)fps;

    yyjson_val *loop = yyjson_obj_get(entry, "loop");
    if (yyjson_is_bool(loop))
        def->looped = yyjson_get_bool(loop);

    Char_JsonVec2(yyjson_obj_get(entry, "offsets"), &def->offsetX, &def->offsetY);
    return true;
}

static void Char_JsonInit(CharJson *json)
{
    *json = (CharJson){ .scale = 1.0f, .singDuration = 4.0f,
                        .healthColor = { 255, 255, 255, 255 } };
}

/* Preenche so o que o JSON define; json deve ter passado por Char_JsonInit. */
static bool Char_ReadJson(const char *path, CharJson *json)
{
    int size = GetFileLength(path);
    if (size <= 0 || size > CHAR_JSON_MAX_SIZE)
        return false;

    yyjson_doc *document = yyjson_read_file(path, 0, NULL, NULL);
    if (!document)
        return false;

    yyjson_val *root = yyjson_doc_get_root(document);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(document);
        return false;
    }

    double number;
    if (Char_JsonNumber(yyjson_obj_get(root, "scale"), &number) && number > 0.05 && number < 8.0)
        json->scale = (float)number;
    if (Char_JsonNumber(yyjson_obj_get(root, "sing_duration"), &number) && number > 0.0 && number < 100.0)
        json->singDuration = (float)number;

    yyjson_val *flip = yyjson_obj_get(root, "flip_x");
    if (yyjson_is_bool(flip))
        json->flipX = yyjson_get_bool(flip);

    Char_JsonVec2(yyjson_obj_get(root, "camera_position"), &json->camera.x, &json->camera.y);
    Char_JsonVec2(yyjson_obj_get(root, "position"), &json->position.x, &json->position.y);
    Char_JsonReadColor(yyjson_obj_get(root, "healthbar_colors"), json);

    yyjson_val *icon = yyjson_obj_get(root, "healthicon");
    if (yyjson_is_str(icon) && Char_IsSafeName(yyjson_get_str(icon)))
        Char_CopyString(json->healthIcon, sizeof(json->healthIcon), yyjson_get_str(icon));

    yyjson_val *image = yyjson_obj_get(root, "image");
    if (yyjson_is_str(image) && yyjson_get_str(image)[0]) {
        char stem[128];
        Char_StemName(yyjson_get_str(image), stem, sizeof(stem));
        if (Char_IsSafeName(stem))
            Char_CopyString(json->imageBase, sizeof(json->imageBase), stem);
    }

    size_t index, max;
    yyjson_val *entry;
    yyjson_arr_foreach(yyjson_obj_get(root, "animations"), index, max, entry) {
        if (json->defCount >= CHAR_MAX_DEFS)
            break;
        if (Char_JsonReadDef(entry, &json->defs[json->defCount]))
            ++json->defCount;
    }

    yyjson_doc_free(document);
    return true;
}

/* Sem JSON (ou sem animacoes nele): o atlas usa os proprios nomes dos papeis. */
static void Char_AddDefaultDefs(CharJson *json)
{
    static const char *const roles[] = { "idle", "singLEFT", "singDOWN", "singUP", "singRIGHT" };

    for (size_t i = 0; i < sizeof(roles) / sizeof(roles[0]); ++i) {
        CharAnimDef *def = &json->defs[json->defCount++];
        Char_CopyString(def->role, sizeof(def->role), roles[i]);
        Char_CopyString(def->sprite, sizeof(def->sprite), roles[i]);
        def->fps = 24;
        def->looped = false;
        def->offsetX = 0.0f;
        def->offsetY = 0.0f;
    }
}

/* Liga o papel (ex.: "idle") a animacao do atlas; -1 se o JSON nao define. */
static int Char_BindRole(RayAnimationHandler *set, const CharJson *json, const char *role)
{
    for (int i = 0; i < json->defCount; ++i) {
        const CharAnimDef *def = &json->defs[i];
        if (!strcmp(def->role, role)) {
            return Char_ConfigureAnimation(set, def->sprite, (float)def->fps,
                                           def->looped, def->offsetX, def->offsetY);
        }
    }
    return -1;
}

static bool Char_LoadLegacyCustom(const CharCtx *ctx, Character *character,
                                  const char *name, CharRole role)
{
    char setPath[CHAR_PATH_MAX];
    char jsonPath[CHAR_PATH_MAX];
    char pngPath[CHAR_PATH_MAX];

    CharJson json;
    Char_JsonInit(&json);
    if (Char_Find(ctx, jsonPath, "characters/%s.json", name))
        Char_ReadJson(jsonPath, &json);

    const char *textureName = json.imageBase[0] ? json.imageBase : name;
    if (!Char_FindPair(ctx, setPath, pngPath, name, textureName) &&
        (!strcmp(textureName, name) || !Char_FindPair(ctx, setPath, pngPath, name, name))) {
        return false;
    }

    Texture2D texture = Render_LoadTexture(pngPath);
    if (!texture.id)
        return false;

    RayAnimationHandler set = AnimationSet_LoadAnimations(setPath, texture);
    if (json.defCount == 0)
        Char_AddDefaultDefs(&json);

    static const char *const singRoles[4] = { "singLEFT", "singDOWN", "singUP", "singRIGHT" };

    Char_InitAnimationSlots(character);
    character->animations.idle = Char_BindRole(&set, &json, "idle");
    for (int i = 0; i < 4; ++i) {
        character->animations.notes[i] = Char_BindRole(&set, &json, singRoles[i]);
        character->missAnimations.notes[i] = character->animations.notes[i];
    }
    if (character->animations.idle < 0 && set.animationCount > 0)
        character->animations.idle = 0;

    character->object.animationSet = set;
    /* Psych: o jogador inverte o flip_x do JSON (bf.json traz flip_x true e aparece sem espelhar). */
    const bool mirrored = (role == CHAR_BF) ? !json.flipX : json.flipX;
    character->object.scaleX = mirrored ? -json.scale : json.scale;
    character->object.scaleY = json.scale;
    character->object.color = WHITE;
    Char_PsychBox(pngPath, json.scale, &character->psychBox, &character->psychShift);
    character->object.position = (Vector2){ json.position.x + character->psychShift.x,
                                            json.position.y + character->psychShift.y };
    character->singDuration = json.singDuration;
    character->hasHealthColor = json.hasHealthColor;
    character->healthColor = json.healthColor;
    /* camera_position cru: PlayStateStage soma o ponto medio e as constantes da Psych. */
    character->cameraOffset = json.camera;
    character->psychCamera = true;

    AnimatedObject_SetAnimation(&character->object, character->animations.idle);
    Char_LoadIcon(ctx, character, name, json.healthIcon, role);
    return true;
}

/* ------------------------------------------------------------------------- */
/* bf e dad padrao do jogo                                                   */
/* ------------------------------------------------------------------------- */

typedef struct {
    const char *anim;        /* nome no atlas; NULL = sem animacao */
    float offsetX;
    float offsetY;
} BuiltinAnim;

typedef struct {
    const char *name;        /* arquivos: images/characters/<name>.animset/.png */
    BuiltinAnim idle;
    BuiltinAnim sing[4];     /* esquerda, baixo, cima, direita */
    BuiltinAnim miss[4];
} BuiltinDef;

static const BuiltinDef BUILTIN_BF = {
    "bf",
    { "BF idle dance", -5.0f, 0.0f },
    { { "BF NOTE LEFT", 5.0f, -6.0f },   { "BF NOTE DOWN", -20.0f, -51.0f },
      { "BF NOTE UP", -46.0f, 27.0f },   { "BF NOTE RIGHT", -48.0f, -7.0f } },
    { { "BF NOTE LEFT MISS", 7.0f, 19.0f },  { "BF NOTE DOWN MISS", -15.0f, -19.0f },
      { "BF NOTE UP MISS", -46.0f, 27.0f },  { "BF NOTE RIGHT MISS", -48.0f, 19.0f } }
};

static const BuiltinDef BUILTIN_DAD = {
    "dad",
    { "Dad idle dance", 0.0f, 0.0f },
    { { "Dad Sing Note LEFT", -9.0f, 10.0f }, { "Dad Sing Note DOWN", 0.0f, -30.0f },
      { "Dad Sing Note UP", -6.0f, 50.0f },   { "Dad Sing Note RIGHT", 0.0f, 27.0f } },
    { { NULL, 0.0f, 0.0f }, { NULL, 0.0f, 0.0f }, { NULL, 0.0f, 0.0f }, { NULL, 0.0f, 0.0f } }
};

static int Char_BindBuiltin(RayAnimationHandler *set, BuiltinAnim anim)
{
    return Char_ConfigureAnimation(set, anim.anim, 24.0f, false, anim.offsetX, anim.offsetY);
}

static bool Char_LoadBuiltin(const CharCtx *ctx, Character *character, CharRole role)
{
    const BuiltinDef *def = (role == CHAR_DAD) ? &BUILTIN_DAD : &BUILTIN_BF;
    char setPath[CHAR_PATH_MAX];
    char pngPath[CHAR_PATH_MAX];

    if (!Char_FindPair(ctx, setPath, pngPath, def->name, def->name))
        return false;

    Texture2D texture = Render_LoadTexture(pngPath);
    if (!texture.id)
        return false;

    RayAnimationHandler set = AnimationSet_LoadAnimations(setPath, texture);

    Char_InitAnimationSlots(character);
    character->animations.idle = Char_BindBuiltin(&set, def->idle);
    for (int i = 0; i < 4; ++i) {
        character->animations.notes[i] = Char_BindBuiltin(&set, def->sing[i]);
        character->missAnimations.notes[i] = Char_BindBuiltin(&set, def->miss[i]);
    }
    if (character->animations.idle < 0 && set.animationCount > 0)
        character->animations.idle = 0;

    character->cameraOffset = Char_DefaultCamera(role);
    character->object.animationSet = set;
    character->object.scaleX = 1.0f;
    character->object.scaleY = 1.0f;
    character->object.color = WHITE;

    AnimatedObject_SetAnimation(&character->object, character->animations.idle);
    Char_LoadIcon(ctx, character, def->name, NULL, role);
    return true;
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

    const CharCtx *ctx = &songCtx;

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
/* API: girlfriend                                                           */
/* ------------------------------------------------------------------------- */

void Girlfriend_Load(Girlfriend *restrict gf, const char *restrict name)
{
    if (!gf)
        return;

    const CharCtx *ctx = &songCtx;
    const char *characterName = (name && Char_IsSafeName(name)) ? name : "gf";

    Render_DefaultAnimated(&gf->object);
    gf->f = 0;

    /* XML: mesma descoberta dos outros personagens, sem carregar icone. */
    Character temporary = {0};
    Render_DefaultAnimated(&temporary.object);
    Char_InitAnimationSlots(&temporary);

    if (Char_TryLoadXMLCharacter(ctx, &temporary, characterName, CHAR_GF)) {
        static const char *const leftNames[] = { "danceLeft", "danceLEFT", "idle", "GF Dancing Beat0" };
        static const char *const rightNames[] = { "danceRight", "danceRIGHT", "idle", "GF Dancing Beat1" };

        gf->object = temporary.object;
        gf->anims[0] = CHAR_FIND_ANY(&gf->object.animationSet, leftNames);
        gf->anims[1] = CHAR_FIND_ANY(&gf->object.animationSet, rightNames);

        if (gf->anims[0] < 0 && gf->object.animationSet.animationCount > 0)
            gf->anims[0] = 0;
        if (gf->anims[1] < 0)
            gf->anims[1] = gf->anims[0];

        AnimatedObject_SetAnimation(&gf->object, gf->anims[0]);
        return;
    }

    /* animset + png (+ characters/<nome>.json); sem eles, cai na gf padrao */
    char setPath[CHAR_PATH_MAX];
    char jsonPath[CHAR_PATH_MAX];
    char pngPath[CHAR_PATH_MAX];

    CharJson json;
    Char_JsonInit(&json);

    if (Char_Find(ctx, jsonPath, "characters/%s.json", characterName))
        Char_ReadJson(jsonPath, &json);

    const char *textureName = json.imageBase[0] ? json.imageBase : characterName;
    bool found = Char_FindPair(ctx, setPath, pngPath, characterName, textureName) ||
                 (strcmp(textureName, characterName) != 0 &&
                  Char_FindPair(ctx, setPath, pngPath, characterName, characterName));
    if (!found && strcmp(characterName, "gf") != 0) {
        Log_WarnF("Girlfriend '%s' nao encontrada: usando gf padrao", characterName);
        Char_JsonInit(&json);
        found = Char_FindPair(ctx, setPath, pngPath, "gf", "gf");
    }
    if (!found)
        Log_Fatal("Girlfriend_Load: could not locate girlfriend assets");

    Texture2D texture = Render_LoadTexture(pngPath);
    if (!texture.id)
        Log_Fatal("Girlfriend_Load: could not load girlfriend texture");

    RayAnimationHandler set = AnimationSet_LoadAnimations(setPath, texture);

    /* Danca: danceLeft/danceRight do JSON; senao idle nas duas; senao as 2 primeiras. */
    int left = Char_BindRole(&set, &json, "danceLeft");
    int right = Char_BindRole(&set, &json, "danceRight");
    const int idle = Char_BindRole(&set, &json, "idle");
    if (left < 0) left = idle;
    if (right < 0) right = left;

    if (left < 0) {
        for (int i = 0; i < set.animationCount && i < 2; ++i) {
            set.animations[i].fps = 24.0f;
            set.animations[i].frameTime = 1.0f / 24.0f;
            set.animations[i].looped = false;
        }
        left = 0;
        right = set.animationCount > 1 ? 1 : 0;
    }

    gf->object.animationSet = set;
    gf->anims[0] = left;
    gf->anims[1] = right;
    gf->object.scaleX = json.flipX ? -json.scale : json.scale;
    gf->object.scaleY = json.scale;
    gf->object.color = WHITE;
    Vector2 gfBox, gfShift;
    Char_PsychBox(pngPath, json.scale, &gfBox, &gfShift);
    gf->object.position = (Vector2){ json.position.x + gfShift.x, json.position.y + gfShift.y };
    AnimatedObject_SetAnimation(&gf->object, gf->anims[0]);
}
