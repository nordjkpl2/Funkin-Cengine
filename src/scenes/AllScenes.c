/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * AllScenes.c - inicio/fim do jogo, fonte global e cache das skins de nota.
 *
 * A skin padrao carrega so no primeiro uso. Skins de mod sao procuradas uma vez
 * por nome, na pasta do mod da musica e depois nos assets base, e ficam em cache.
 */

#include "scenes/AllScenes.h"
#include <string.h>
#include <stdbool.h>
#include "Game.h"
#include "scenes/Song.h"

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

Font mainFont;
Vector2 VECTOR_ZERO;

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

/* "assets/mods/<mod>/songs/<musica>" -> "assets/mods/<mod>". */
static bool ExtractModRoot(const char* songDir, char* out, size_t capacity) {
    if (!songDir || !songDir[0]) return false;

    static const char marker[] = "assets/mods/";
    const char* modStart = strstr(songDir, marker);
    if (!modStart) return false;

    const char* slash = strchr(modStart + sizeof(marker) - 1, '/');
    if (!slash) return false;

    const char* songs = strstr(slash, "/songs/");
    size_t length = (size_t)((songs ? songs : slash) - songDir);
    if (length >= capacity) length = capacity - 1;

    memcpy(out, songDir, length);
    out[length] = 0;
    return out[0] != 0;
}

/* Procura NOTE_<skin>_assets.(xml|animset) + .png: mod primeiro, depois assets base. */
static bool NoteSkin_FindFiles(const char* skinName, const char* songDir, CachedNoteSkin* skin) {
    char animationPath[SKIN_PATH_MAX], texturePath[SKIN_PATH_MAX];
    char modRoot[SKIN_PATH_MAX] = {0};

    if (ExtractModRoot(songDir, modRoot, sizeof(modRoot))) {
        (void)snprintf(animationPath, sizeof(animationPath), "%s/images/NOTE_%s_assets.xml", modRoot, skinName);
        (void)snprintf(texturePath, sizeof(texturePath), "%s/images/NOTE_%s_assets.png", modRoot, skinName);
        if (TrySkinPaths(animationPath, texturePath, skin)) return true;
    }

    (void)snprintf(animationPath, sizeof(animationPath), "assets/images/NOTE_%s_assets.xml", skinName);
    (void)snprintf(texturePath, sizeof(texturePath), "assets/images/NOTE_%s_assets.png", skinName);
    if (TrySkinPaths(animationPath, texturePath, skin)) return true;

    (void)snprintf(animationPath, sizeof(animationPath), "assets/images/NOTE_%s_assets.animset", skinName);
    return TrySkinPaths(animationPath, texturePath, skin);
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
        return skin;
    }

    if (noteSkinCount >= NOTE_SKIN_MAX) return NULL;

    CachedNoteSkin* slot = noteSkins + noteSkinCount;
    (void)snprintf(slot->name, sizeof(slot->name), "%s", skinName);
    slot->isLoaded = 0;

    if (!NoteSkin_FindFiles(skinName, songDir, slot)) {
        slot->name[0] = 0; // libera o slot
        return NULL;
    }

    slot->animations = NoteSkin_Load(slot->animationPath, slot->texturePath);
    slot->isLoaded = 1;
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

/* ------------------------------------------------------------------ */
/* Inicio e fim do jogo                                                */
/* ------------------------------------------------------------------ */

void AllScenes_StartGame(void) {
    VECTOR_ZERO = (Vector2) {0, 0};

    RayGame_SetMusic("assets/music/freakyMenu.ogg", 0.3f, 1);
    RayGame_ToggleMusic(1);
    mainFont = LoadFontEx("assets/fonts/main.ttf", 64, 0, 0);

    defaultSkinLoaded = 0;
    Cache_UnloadCustomNoteSkins(); // zera o cache de skins
}

void AllScenes_DestroyGame(void) {
    if (defaultSkinLoaded)
        AnimationSet_FreeAll(&defaultSkin);

    Cache_UnloadCustomNoteSkins();
    UnloadFont(mainFont);
}
