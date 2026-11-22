/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * NoteSkinCache.c - cache das skins de nota.
 *
 * A skin padrao carrega so no primeiro uso. Skins de mod sao procuradas uma vez
 * por nome, na pasta do mod da musica e depois nos assets base, e ficam em cache.
 */

#include "scenes/api/NoteSkinCache.h"
#include <string.h>
#include <stdbool.h>
#include "data/Song.h"
#include "core/ModPath.h"
#include "Log.h"
#include "Render.h"
#include "core/ModList.h"

#define NOTE_SKIN_MAX 16
#define NOTE_SKIN_NAME_MAX (SONG_NAME_MAX - 1)
#define SKIN_PATH_MAX 260

#define DEFAULT_SKIN_ANIMSET "assets/images/NOTE_assets.animset"
#define DEFAULT_SKIN_TEXTURE "assets/images/NOTE_assets.png"

typedef struct {
    RayAnimationHandler animations;
    char name[NOTE_SKIN_NAME_MAX + 1];
    char animationPath[SKIN_PATH_MAX];
    char texturePath[SKIN_PATH_MAX];
    char isLoaded;
} CachedNoteSkin;

static RayAnimationHandler defaultSkin;
static char defaultSkinLoaded;

static CachedNoteSkin noteSkins[NOTE_SKIN_MAX];
static int noteSkinCount;

/* ------------------------------------------------------------------ */
/* Animacoes da skin                                                   */
/* ------------------------------------------------------------------ */

/* FPS e deslocamento de cada animacao dos receptores (nome no .animset/.xml). */
static void NoteSkin_SetupAnimations(RayAnimationHandler* handler) {
    static const struct { const char* name; int fps; int offsetX, offsetY; } defs[] = {
        {"arrowDOWN", 1, 0, 0}, {"arrowUP", 1, 0, 0}, {"arrowLEFT", 1, 0, 0}, {"arrowRIGHT", 1, 0, 0},
        {"left press", 24, 0, -4}, {"right press", 24, 0, -4}, {"up press", 24, 0, -4}, {"down press", 24, 0, -4},
        {"left confirm", 24, 0, 37}, {"right confirm", 24, 0, 37}, {"up confirm", 24, 0, 37}, {"down confirm", 24, 0, 37},
    };

    for (size_t i = 0; i < sizeof(defs) / sizeof(defs[0]); i++) {
        int unused = 0; // exigido pela macro AnimationSet_SetData
        AnimationSet_SetData(unused, *handler, defs[i].name, defs[i].fps, 0, defs[i].offsetX, defs[i].offsetY);
    }
}

static RayAnimationHandler NoteSkin_Load(const char* animationPath, const char* texturePath) {
    RayAnimationHandler handler = AnimationSet_LoadAnimations(animationPath, Render_LoadTexture(texturePath));
    NoteSkin_SetupAnimations(&handler);
    return handler;
}

/* ------------------------------------------------------------------ */
/* Skin padrao                                                         */
/* ------------------------------------------------------------------ */

static void EnsureDefaultLoaded(void) {
    if (defaultSkinLoaded) return;

    defaultSkin = NoteSkin_Load(DEFAULT_SKIN_ANIMSET, DEFAULT_SKIN_TEXTURE);
    defaultSkinLoaded = 1;
}

/* ------------------------------------------------------------------ */
/* Skins de mod                                                        */
/* ------------------------------------------------------------------ */

static void CopyPath(char* destination, const char* source) {
    (void)snprintf(destination, SKIN_PATH_MAX, "%s", source);
}

static int FindSkinIndex(const char* name) {
    for (int i = 0; i < noteSkinCount; i++)
        if (noteSkins[i].name[0] == name[0] && strcmp(noteSkins[i].name, name) == 0)
            return i;

    return -1;
}

/* Guarda os caminhos na skin se os dois arquivos existirem. */
static bool TrySkinPaths(const char* animationPath, const char* texturePath, CachedNoteSkin* skin) {
    if (!FileExists(animationPath) || !FileExists(texturePath)) return false;

    CopyPath(skin->animationPath, animationPath);
    CopyPath(skin->texturePath, texturePath);
    return true;
}

/* "assets/mods/<mod>/songs/<musica>" ou "assets/mods/songs/<musica>" -> raiz do mod. */
static bool ExtractModRoot(const char* songDir, char* out, size_t capacity) {
    return ModPath_Root(songDir, out, capacity);
}

/* Nome-base do arquivo (sem extensao, relativo a images/): "NOTE_<skin>_assets" ou, com "@" na
   frente, o caminho do arrowSkin do Psych. false = nome invalido. */
static bool SkinBaseName(const char* skinName, char* out, size_t capacity) {
    if (skinName[0] == '@') {
        const char* rel = skinName + 1;
        if (rel[0] == 0 || rel[0] == '/' || rel[0] == '\\' || strstr(rel, "..") || strchr(rel, ':')) return false;
        return snprintf(out, capacity, "%s", rel) < (int)capacity;
    }
    return snprintf(out, capacity, "NOTE_%s_assets", skinName) < (int)capacity;
}

/* <raiz>/images/<base>.(xml|animset) + .png */
static bool TrySkinRoot(const char* root, const char* base, CachedNoteSkin* skin) {
    char animationPath[SKIN_PATH_MAX], texturePath[SKIN_PATH_MAX];
    (void)snprintf(texturePath, sizeof(texturePath), "%s/images/%s.png", root, base);
    (void)snprintf(animationPath, sizeof(animationPath), "%s/images/%s.xml", root, base);
    if (TrySkinPaths(animationPath, texturePath, skin)) return true;
    (void)snprintf(animationPath, sizeof(animationPath), "%s/images/%s.animset", root, base);
    return TrySkinPaths(animationPath, texturePath, skin);
}

/* Procura a skin: mod da musica, depois assets base, depois qualquer mod ativo. */
static bool NoteSkin_FindFiles(const char* skinName, const char* songDir, CachedNoteSkin* skin) {
    char base[SKIN_PATH_MAX];
    char modRoot[SKIN_PATH_MAX] = {0};

    if (!SkinBaseName(skinName, base, sizeof(base))) return false;

    if (ExtractModRoot(songDir, modRoot, sizeof(modRoot)) && TrySkinRoot(modRoot, base, skin)) return true;
    if (TrySkinRoot("assets", base, skin)) return true;

    for (int i = 0; i < ModList_Count(); i++)
        if (ModList_Enabled(i) && TrySkinRoot(ModList_Root(i), base, skin)) return true;
    return false;
}

/* Aviso para cada animacao de lane que a skin nao tem (a lane some sem erro visivel). */
static void WarnMissingLaneAnims(const char* skinName, const RayAnimationHandler* anims) {
    static const char* const names[] = {
        "arrowLEFT", "arrowDOWN", "arrowUP", "arrowRIGHT",
        "left press", "down press", "up press", "right press",
        "left confirm", "down confirm", "up confirm", "right confirm",
        "purple", "blue", "green", "red",
        "purple hold piece", "blue hold piece", "green hold piece", "red hold piece",
        "purple hold end", "blue hold end", "green hold end", "red hold end",
    };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        if (AnimationSet_FindAnimation(anims, names[i]) < 0)
            Log_WarnF("Skin '%s': animacao '%s' nao existe no XML (a lane pode sumir)", skinName, names[i]);
}

/* Aviso se a skin usada nao e a que a musica pediria pelo proprio mod (cache por nome pode trazer
   a de outro mod carregada antes). */
static void WarnIfSkinMismatch(const char* skinName, const char* songDir, const CachedNoteSkin* used) {
    char base[SKIN_PATH_MAX], modRoot[SKIN_PATH_MAX] = {0};
    CachedNoteSkin own = {0};

    if (!SkinBaseName(skinName, base, sizeof(base))) return;
    if (!ExtractModRoot(songDir, modRoot, sizeof(modRoot))) return;
    if (!TrySkinRoot(modRoot, base, &own)) {
        Log_WarnF("Skin '%s' nao existe no mod da musica (%s): usando %s", skinName, modRoot, used->texturePath);
        return;
    }
    if (strcmp(own.texturePath, used->texturePath) != 0)
        Log_WarnF("Skin '%s': usando %s, mas o mod da musica tem %s", skinName, used->texturePath, own.texturePath);
}

/* Devolve a skin em cache (carregando se preciso) ou NULL se nao existir. */
static CachedNoteSkin* ResolveNoteSkin(const char* skinName, const char* songDir) {
    if (!skinName || !skinName[0]) return NULL;

    const int index = FindSkinIndex(skinName);
    if (index >= 0) {
        CachedNoteSkin* skin = noteSkins + index;
        if (!skin->isLoaded) {
            skin->animations = NoteSkin_Load(skin->animationPath, skin->texturePath);
            skin->isLoaded = 1;
        }
        WarnIfSkinMismatch(skinName, songDir, skin);
        return skin;
    }

    if (noteSkinCount >= NOTE_SKIN_MAX) return NULL;

    CachedNoteSkin* slot = noteSkins + noteSkinCount;
    (void)snprintf(slot->name, sizeof(slot->name), "%s", skinName);
    slot->isLoaded = 0;

    if (!NoteSkin_FindFiles(skinName, songDir, slot)) {
        slot->name[0] = 0; // libera o slot
        Log_WarnF("Skin de notas '%s' nao encontrada (esperado %s.png/.xml em images/)", skinName, skinName);
        return NULL;
    }
    WarnIfSkinMismatch(skinName, songDir, slot);

    slot->animations = NoteSkin_Load(slot->animationPath, slot->texturePath);
    slot->isLoaded = 1;
    WarnMissingLaneAnims(skinName, &slot->animations);
    noteSkinCount++;
    return slot;
}

/* ------------------------------------------------------------------ */
/* API do cache                                                        */
/* ------------------------------------------------------------------ */

RayAnimationHandler Cache_GetNoteSkin(const char* skinName, const char* songDir) {
    const CachedNoteSkin* skin = ResolveNoteSkin(skinName, songDir);
    if (skin) return skin->animations;

    EnsureDefaultLoaded();
    return defaultSkin;
}

RayAnimationHandler* Cache_GetNoteSkinPtr(const char* skinName, const char* songDir) {
    CachedNoteSkin* skin = ResolveNoteSkin(skinName, songDir);
    return skin ? &skin->animations : Cache_GetDefaultNoteSkinPtr();
}

RayAnimationHandler* Cache_GetDefaultNoteSkinPtr(void) {
    EnsureDefaultLoaded();
    return &defaultSkin;
}

RayAnimationHandler Cache_GetNoteAnimations(void) {
    EnsureDefaultLoaded();
    return defaultSkin;
}

static void Cache_UnloadCustomNoteSkins(void) {
    for (int i = 0; i < noteSkinCount; i++)
        if (noteSkins[i].isLoaded)
            AnimationSet_FreeAll(&noteSkins[i].animations);

    memset(noteSkins, 0, sizeof(noteSkins));
    noteSkinCount = 0;
}

void NoteSkinCache_Reset(void) {
    defaultSkinLoaded = 0;
    Cache_UnloadCustomNoteSkins();
}

void NoteSkinCache_Free(void) {
    if (defaultSkinLoaded)
        AnimationSet_FreeAll(&defaultSkin);

    Cache_UnloadCustomNoteSkins();
}
