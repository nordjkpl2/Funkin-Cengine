/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Stage Editor: IO. Le com o mesmo parser do jogo (StageLuaParse/StageJson) e escreve o .lua no
   subconjunto que esse parser entende (makeLuaSprite, scaleObject, addLuaSprite...). */
#include "scenes/modding/stageeditor/StageEditorIO.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"
#include "logic/stage/StageJson.h"
#include "logic/stage/StageLuaParse.h"
#include "yyjson.h"

#define STAGEED_TEXT_MAX (256L * 1024L)

/* ------------------------------------------------------------------------- */
/* Listas                                                                    */
/* ------------------------------------------------------------------------- */

static bool Entry_Known(const StageEdEntry *out, int count, const char *name)
{
    for (int i = 0; i < count; ++i) {
        if (strcmp(out[i].name, name) == 0)
            return true;
    }
    return false;
}

static bool Name_Known(char names[][MODED_NAME_MAX], int count, const char *name)
{
    for (int i = 0; i < count; ++i) {
        if (strcmp(names[i], name) == 0)
            return true;
    }
    return false;
}

int StageEd_ScanList(StageEdEntry *out, int capacity)
{
    static char roots[MODED_ROOTS_MAX][MODED_ROOT_MAX];
    static char names[STAGEED_LIST_MAX][MODED_NAME_MAX];
    int count = 0;
    if (!out || capacity <= 0)
        return 0;

    const int rootCount = ModEd_Roots(roots, MODED_ROOTS_MAX);
    for (int r = 0; r < rootCount; ++r) {
        char dir[MODED_ROOT_MAX + 16];
        snprintf(dir, sizeof(dir), "%s/stages", roots[r]);

        /* .json e .lua do mesmo nome contam como um stage so. */
        const char *exts[2] = {".json", ".lua"};
        for (int e = 0; e < 2; ++e) {
            const int n = ModEd_ListStems(dir, exts[e], names, STAGEED_LIST_MAX);
            for (int i = 0; i < n && count < capacity; ++i) {
                if (!ModEd_IsSafeName(names[i]) || Entry_Known(out, count, names[i]))
                    continue;
                StageEdEntry *entry = &out[count++];
                snprintf(entry->name, sizeof(entry->name), "%s", names[i]);
                snprintf(entry->root, sizeof(entry->root), "%s", roots[r]);
            }
        }
    }
    return count;
}

int StageEd_ScanImages(char out[][MODED_NAME_MAX], int capacity)
{
    static char roots[MODED_ROOTS_MAX][MODED_ROOT_MAX];
    static char names[STAGEED_LIST_MAX][MODED_NAME_MAX];
    int count = 0;
    if (!out || capacity <= 0)
        return 0;

    const int rootCount = ModEd_Roots(roots, MODED_ROOTS_MAX);
    for (int r = 0; r < rootCount && count < capacity; ++r) {
        char dir[MODED_ROOT_MAX + 16];
        snprintf(dir, sizeof(dir), "%s/images", roots[r]);

        const int n = ModEd_ListStems(dir, ".png", names, STAGEED_LIST_MAX);
        for (int i = 0; i < n && count < capacity; ++i) {
            if (!Name_Known(out, count, names[i]))
                snprintf(out[count++], MODED_NAME_MAX, "%s", names[i]);
        }
    }
    return count;
}

bool StageEd_ImagePath(const char *root, const char *image, char *out, size_t capacity)
{
    char path[MODED_ROOT_MAX + MODED_NAME_MAX + 32];
    if (!image || !image[0] || !ModEd_IsSafeName(image))
        return false;

    if (root && root[0]) {
        snprintf(path, sizeof(path), "%s/images/%s.png", root, image);
        if (FileExists(path)) {
            snprintf(out, capacity, "%s", path);
            return true;
        }
    }
    snprintf(path, sizeof(path), "assets/images/%s.png", image);
    if (!FileExists(path))
        return false;
    snprintf(out, capacity, "%s", path);
    return true;
}

/* ------------------------------------------------------------------------- */
/* Leitura                                                                   */
/* ------------------------------------------------------------------------- */

static char *ReadText(const char *path)
{
    FILE *file = fopen(path, "rb");
    long size;
    char *buffer;

    if (!file)
        return NULL;
    if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0 || size > STAGEED_TEXT_MAX ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    buffer = (char *)malloc((size_t)size + 1u);
    if (!buffer) {
        fclose(file);
        return NULL;
    }
    if (fread(buffer, 1, (size_t)size, file) != (size_t)size) {
        free(buffer);
        fclose(file);
        return NULL;
    }
    fclose(file);
    buffer[size] = '\0';
    return buffer;
}

bool StageEd_Load(const StageEdEntry *entry, StageSprite *sprites, int capacity, int *count,
                  StageMeta *meta, char *msg, size_t msgCap)
{
    char luaPath[MODED_ROOT_MAX + MODED_NAME_MAX + 32];
    char jsonPath[MODED_ROOT_MAX + MODED_NAME_MAX + 32];
    int n = 0;

    if (!entry || !sprites || !count || !meta)
        return false;

    snprintf(luaPath, sizeof(luaPath), "%s/stages/%s.lua", entry->root, entry->name);
    snprintf(jsonPath, sizeof(jsonPath), "%s/stages/%s.json", entry->root, entry->name);

    Stage_LoadJson(jsonPath, meta);

    char *text = ReadText(luaPath);
    if (text) {
        int ignoredGlitch = 0, ignoredAnim = 0;
        n = Stage_ParseLua(text, sprites, capacity, &ignoredGlitch, &ignoredAnim);
        free(text);
        if (msg && msgCap)
            snprintf(msg, msgCap, "carregado: %d objetos", n);
    } else if (msg && msgCap) {
        snprintf(msg, msgCap, "stage novo (sem .lua ainda)");
    }

    if (n < 0)
        n = 0;
    for (int i = 0; i < n; ++i) {
        if (sprites[i].scaleX <= 0.0f)
            sprites[i].scaleX = 1.0f;
        if (sprites[i].scaleY <= 0.0f)
            sprites[i].scaleY = 1.0f;
    }
    *count = n;
    return true;
}

/* ------------------------------------------------------------------------- */
/* Escrita                                                                   */
/* ------------------------------------------------------------------------- */

static bool WriteJson(const char *path, const StageMeta *meta, char *msg, size_t msgCap)
{
    yyjson_doc *source = NULL;
    yyjson_mut_doc *doc = NULL;

    if (FileExists(path)) {
        source = yyjson_read_file(path, 0, NULL, NULL);
        if (!source) {
            snprintf(msg, msgCap, "JSON invalido, nao salvei: %s", path);
            return false;
        }
        doc = yyjson_doc_mut_copy(source, NULL);
        yyjson_doc_free(source);
    }
    if (!doc)
        doc = yyjson_mut_doc_new(NULL);
    if (!doc) {
        snprintf(msg, msgCap, "sem memoria para salvar");
        return false;
    }

    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    if (!root || !yyjson_mut_is_obj(root)) {
        root = yyjson_mut_obj(doc);
        yyjson_mut_doc_set_root(doc, root);
    }

    ModEd_PutReal(doc, root, "defaultZoom", meta->defaultZoom);
    ModEd_PutBool(doc, root, "isPixelStage", meta->isPixelStage != 0);
    ModEd_PutBool(doc, root, "hide_girlfriend", meta->hideGirlfriend != 0);
    ModEd_PutRealArr2(doc, root, "boyfriend", meta->boyfriend[0], meta->boyfriend[1]);
    ModEd_PutRealArr2(doc, root, "girlfriend", meta->girlfriend[0], meta->girlfriend[1]);
    ModEd_PutRealArr2(doc, root, "opponent", meta->opponent[0], meta->opponent[1]);

    const bool ok = yyjson_mut_write_file(path, doc, YYJSON_WRITE_PRETTY, NULL, NULL);
    yyjson_mut_doc_free(doc);
    if (!ok)
        snprintf(msg, msgCap, "ERRO ao salvar: %s", path);
    return ok;
}

static void WriteSpriteLua(FILE *f, const StageSprite *s, int index)
{
    char tag[24];
    snprintf(tag, sizeof(tag), "s%d", index);

    if (s->isGraphic) {
        fprintf(f, "makeGraphic('%s', %.0f, %.0f, '#%02X%02X%02X%02X')\n", tag,
                s->graphicWidth, s->graphicHeight, s->colorR, s->colorG, s->colorB, s->colorA);
    } else if (s->isAnimated) {
        fprintf(f, "makeAnimatedLuaSprite('%s', '%s', %.2f, %.2f)\n", tag, s->image, s->x, s->y);
        if (s->animPrefix[0]) {
            fprintf(f, "addAnimationByPrefix('%s', 'anim', '%s', %.2f, %s)\n", tag, s->animPrefix,
                    s->animFps > 0.0f ? s->animFps : 24.0f, s->animLoop ? "true" : "false");
        }
    } else {
        fprintf(f, "makeLuaSprite('%s', '%s', %.2f, %.2f)\n", tag, s->image, s->x, s->y);
    }

    fprintf(f, "setLuaSpriteScrollFactor('%s', %.3f, %.3f)\n", tag, s->scrollX, s->scrollY);
    fprintf(f, "scaleObject('%s', %.4f, %.4f)\n", tag, s->scaleX, s->scaleY);
    fprintf(f, "addLuaSprite('%s', %s)\n\n", tag, s->front ? "true" : "false");
}

bool StageEd_Save(const StageEdEntry *entry, const StageSprite *sprites, int count,
                  const StageMeta *meta, char *msg, size_t msgCap)
{
    char dir[MODED_ROOT_MAX + 16];
    char luaPath[MODED_ROOT_MAX + MODED_NAME_MAX + 32];
    char jsonPath[MODED_ROOT_MAX + MODED_NAME_MAX + 32];

    if (!entry || !meta || (count > 0 && !sprites))
        return false;

    snprintf(dir, sizeof(dir), "%s/stages", entry->root);
    snprintf(luaPath, sizeof(luaPath), "%s/%s.lua", dir, entry->name);
    snprintf(jsonPath, sizeof(jsonPath), "%s/%s.json", dir, entry->name);
    MakeDirectory(dir);

    /* JSON primeiro: se ele existe e esta quebrado, nao mexe em nada. */
    if (!WriteJson(jsonPath, meta, msg, msgCap))
        return false;

    const bool hadLua = FileExists(luaPath);
    if (hadLua && !ModEd_BackupOnce(luaPath)) {
        snprintf(msg, msgCap, "nao consegui criar backup de %s", luaPath);
        return false;
    }

    FILE *f = fopen(luaPath, "wb");
    if (!f) {
        snprintf(msg, msgCap, "ERRO ao salvar: %s", luaPath);
        return false;
    }
    fprintf(f, "-- gerado pelo Stage Editor (sprites). Scripts antigos ficam em .lua.bak\n");
    fprintf(f, "function onCreate()\n");
    for (int i = 0; i < count; ++i)
        WriteSpriteLua(f, &sprites[i], i);
    fprintf(f, "end\n");
    const bool ok = fclose(f) == 0;

    if (msg && msgCap) {
        if (ok && hadLua)
            snprintf(msg, msgCap, "salvo: %s (original em .lua.bak)", luaPath);
        else if (ok)
            snprintf(msg, msgCap, "salvo: %s", luaPath);
        else
            snprintf(msg, msgCap, "ERRO ao salvar: %s", luaPath);
    }
    return ok;
}
