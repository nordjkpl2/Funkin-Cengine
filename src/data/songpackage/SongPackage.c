/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * SongPackage.c - exporta musica + dependencias (personagens, icones, stage, skin, imagens de
 * evento, dialogo, note types) para exports/<musica>/ no layout de mod.
 */

#include "data/songpackage/SongPackage.h"
#include "core/ModList.h"
#include "core/ModPath.h"
#include "data/dialogue/Dialogue.h"
#include "logic/stage/Stage.h"
#include "logic/stage/StageLuaParse.h"
#include "raylib.h"
#include "yyjson.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SP_PATH      512
#define SP_MAX_DEPTH 16
#define SP_MAX_SEEN  4096

typedef struct {
    char root[SP_PATH];    /* raiz do mod da musica ("assets" fora de mod) */
    char songDir[SP_PATH]; /* normalizado com '/' */
    char out[SP_PATH];     /* exports/<musica> */
    unsigned long long seen[SP_MAX_SEEN]; /* hash dos destinos ja copiados nesta exportacao */
    int seenCount;
    int files;
    int errors;
    long long bytes;
} SpCtx;

/* ------------------------------------------------------------------------- */
/* Texto e caminho                                                           */
/* ------------------------------------------------------------------------- */

static void SpNormalize(char *dst, size_t size, const char *src)
{
    size_t n = 0;
    for (; src[n] != '\0' && n + 1 < size; ++n)
        dst[n] = src[n] == '\\' ? '/' : src[n];
    while (n > 0 && dst[n - 1] == '/') --n;
    dst[n] = '\0';
}

/* Relativo e preso na raiz: sem '/' inicial, "..", ':' (subpastas com '/' valem). */
static bool SpSafeRel(const char *rel)
{
    return rel != NULL && rel[0] != '\0' && rel[0] != '/' && rel[0] != '\\' &&
           strstr(rel, "..") == NULL && strchr(rel, ':') == NULL;
}

/* Um componente so (nome de personagem, icone...). */
static bool SpSafeName(const char *name)
{
    return SpSafeRel(name) && strchr(name, '/') == NULL && strchr(name, '\\') == NULL;
}

static bool SpJoin(char out[SP_PATH], const char *a, const char *b)
{
    const int n = snprintf(out, SP_PATH, "%s/%s", a, b);
    return n > 0 && n < SP_PATH;
}

static char SpLower(char ch)
{
    return (ch >= 'A' && ch <= 'Z') ? (char)(ch - 'A' + 'a') : ch;
}

/* Resto de text depois de prefix (sem diferenciar maiuscula, como no Windows). NULL = nao bate. */
static const char *SpAfterPrefixI(const char *text, const char *prefix)
{
    for (; *prefix != '\0'; ++prefix, ++text)
        if (*text == '\0' || SpLower(*text) != SpLower(*prefix)) return NULL;
    return text;
}

/* path dentro de root -> parte relativa; NULL = fora (assets base, outro mod). */
static const char *SpRelTo(const char *path, const char *root)
{
    const size_t n = strlen(root);
    if (n == 0 || strncmp(path, root, n) != 0 || path[n] != '/') return NULL;
    return path + n + 1;
}

/* ------------------------------------------------------------------------- */
/* Copia                                                                     */
/* ------------------------------------------------------------------------- */

/* true se o destino ja foi copiado nesta exportacao (FNV-1a 64, sem caixa). */
static bool SpSeen(SpCtx *c, const char *dst)
{
    unsigned long long h = 1469598103934665603ULL;
    for (const char *p = dst; *p != '\0'; ++p)
        h = (h ^ (unsigned char)SpLower(*p == '\\' ? '/' : *p)) * 1099511628211ULL;
    for (int i = 0; i < c->seenCount; ++i)
        if (c->seen[i] == h) return true;
    if (c->seenCount < SP_MAX_SEEN) c->seen[c->seenCount++] = h;
    return false;
}

static void SpCopyFile(SpCtx *c, const char *src, const char *dst)
{
    if (SpSeen(c, dst)) return;
    (void)MakeDirectory(GetDirectoryPath(dst));

    FILE *in = fopen(src, "rb");
    FILE *out = in != NULL ? fopen(dst, "wb") : NULL;
    if (out == NULL) {
        if (in != NULL) fclose(in);
        ++c->errors;
        return;
    }
    unsigned char buf[65536];
    size_t n;
    bool ok = true;
    long long total = 0;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            ok = false;
            break;
        }
        total += (long long)n;
    }
    if (ferror(in)) ok = false;
    fclose(in);
    if (fclose(out) != 0) ok = false;
    if (!ok) {
        remove(dst);
        ++c->errors;
        return;
    }
    ++c->files;
    c->bytes += total;
}

/* <root>/<rel> -> <out>/<rel>. true se o arquivo existia. */
static bool SpCopyRel(SpCtx *c, const char *root, const char *rel)
{
    char src[SP_PATH], dst[SP_PATH];
    if (!SpSafeRel(rel) || !SpJoin(src, root, rel) || !IsPathFile(src) || !SpJoin(dst, c->out, rel))
        return false;
    SpCopyFile(c, src, dst);
    return true;
}

static void SpCopyTree(SpCtx *c, const char *src, const char *dst, int depth)
{
    if (depth > SP_MAX_DEPTH || !DirectoryExists(src)) return;
    FilePathList list = LoadDirectoryFiles(src);
    for (unsigned i = 0; i < list.count; ++i) {
        const char *name = GetFileName(list.paths[i]);
        char from[SP_PATH], to[SP_PATH];
        if (!SpSafeName(name) || !SpJoin(from, src, name) || !SpJoin(to, dst, name)) continue;
        if (IsPathFile(from)) SpCopyFile(c, from, to);
        else SpCopyTree(c, from, to, depth + 1);
    }
    UnloadDirectoryFiles(list);
}

/* <root>/<dirRel>/<stem>.* e as variantes de xml (-char, _char, -atlas, _atlas, Atlas).
 * Pasta com o nome exato (atlas do Animate) vai inteira. Devolve quantos achou. */
static int SpCopyStem(SpCtx *c, const char *root, const char *dirRel, const char *stem)
{
    static const char *const suffixes[] = {".", "-char.", "_char.", "-atlas.", "_atlas.", "Atlas."};
    char dir[SP_PATH];
    int found = 0;

    if (!SpSafeRel(dirRel) || !SpSafeName(stem) || !SpJoin(dir, root, dirRel) || !DirectoryExists(dir))
        return 0;

    FilePathList list = LoadDirectoryFiles(dir);
    for (unsigned i = 0; i < list.count; ++i) {
        const char *name = GetFileName(list.paths[i]);
        const char *rest = SpAfterPrefixI(name, stem);
        char from[SP_PATH], rel[SP_PATH], to[SP_PATH];
        if (rest == NULL || !SpJoin(from, dir, name) || !SpJoin(rel, dirRel, name) || !SpJoin(to, c->out, rel))
            continue;
        if (rest[0] == '\0') {
            if (!IsPathFile(from)) {
                SpCopyTree(c, from, to, 1);
                ++found;
            }
            continue;
        }
        for (size_t k = 0; k < sizeof(suffixes) / sizeof(suffixes[0]); ++k) {
            if (SpAfterPrefixI(rest, suffixes[k]) == NULL) continue;
            if (IsPathFile(from)) {
                SpCopyFile(c, from, to);
                ++found;
            }
            break;
        }
    }
    UnloadDirectoryFiles(list);
    return found;
}

/* Imagem relativa a images/ ("stages/bg" -> images/stages/bg.png/.xml...). */
static int SpCopyImage(SpCtx *c, const char *root, const char *image)
{
    char norm[SP_PATH], dirRel[SP_PATH];
    SpNormalize(norm, sizeof(norm), image);
    if (!SpSafeRel(norm)) return 0;

    const char *slash = strrchr(norm, '/');
    const char *stem = slash != NULL ? slash + 1 : norm;
    const int n = slash != NULL ? snprintf(dirRel, sizeof(dirRel), "images/%.*s", (int)(slash - norm), norm)
                                : snprintf(dirRel, sizeof(dirRel), "images");
    if (n <= 0 || n >= (int)sizeof(dirRel)) return 0;
    return SpCopyStem(c, root, dirRel, stem);
}

/* Caminho achado pela busca do engine: so copia o que esta na raiz da musica. */
static void SpCopyFound(SpCtx *c, const char *path)
{
    char norm[SP_PATH];
    SpNormalize(norm, sizeof(norm), path);
    const char *rel = SpRelTo(norm, c->root);
    if (rel != NULL) (void)SpCopyRel(c, c->root, rel);
}

/* ------------------------------------------------------------------------- */
/* Dependencias                                                              */
/* ------------------------------------------------------------------------- */

/* Mesmos testes de Char_ResolveCtx/ModList_FindCharacterRoot (+ personagem .xml). */
static bool SpHasCharacter(const char *root, const char *name)
{
    static const char *const fmts[] = {
        "%s/images/characters/%s.animset", "%s/images/characters/%s.xml", "%s/characters/%s.json",
        "%s/characters/%s.xml", "%s/data/characters/%s.xml"
    };
    char path[SP_PATH];
    for (size_t i = 0; i < sizeof(fmts) / sizeof(fmts[0]); ++i) {
        const int n = snprintf(path, sizeof(path), fmts[i], root, name);
        if (n > 0 && n < (int)sizeof(path) && IsPathFile(path)) return true;
    }
    return false;
}

static void SpIcon(SpCtx *c, const char *root, const char *icon)
{
    char rel[SP_PATH];
    if (!SpSafeName(icon)) return;
    snprintf(rel, sizeof(rel), "images/icons/%s.png", icon);
    (void)SpCopyRel(c, root, rel);
    snprintf(rel, sizeof(rel), "images/icons/icon-%s.png", icon);
    (void)SpCopyRel(c, root, rel);
}

/* json + atlas (campo "image", aceita lista "a,b") + animset + icone. */
static void SpCharacter(SpCtx *c, const char *name)
{
    char root[SP_PATH], json[SP_PATH], icon[SONG_PACKAGE_NAME_MAX] = "";

    if (!SpSafeName(name)) return;
    if (SpHasCharacter(c->root, name)) {
        snprintf(root, sizeof(root), "%s", c->root);
    } else if (SpHasCharacter("assets", name) || !ModList_FindCharacterRoot(name, root, sizeof(root))) {
        return; /* base do engine ou inexistente */
    }

    (void)SpCopyStem(c, root, "characters", name);
    (void)SpCopyStem(c, root, "data/characters", name);
    (void)SpCopyStem(c, root, "images/characters", name);

    snprintf(json, sizeof(json), "%s/characters/%s.json", root, name);
    yyjson_doc *doc = IsPathFile(json) ? yyjson_read_file(json, YYJSON_READ_ALLOW_TRAILING_COMMAS, NULL, NULL) : NULL;
    yyjson_val *obj = yyjson_doc_get_root(doc);
    const char *image = yyjson_get_str(yyjson_obj_get(obj, "image"));
    const char *health = yyjson_get_str(yyjson_obj_get(obj, "healthicon"));
    if (health != NULL) snprintf(icon, sizeof(icon), "%s", health);

    while (image != NULL && *image != '\0') {
        char part[SP_PATH];
        const char *comma = strchr(image, ',');
        const size_t len = comma != NULL ? (size_t)(comma - image) : strlen(image);
        if (len > 0 && len < sizeof(part)) {
            memcpy(part, image, len);
            part[len] = '\0';
            char *start = part;
            while (*start == ' ') ++start;
            for (size_t e = strlen(start); e > 0 && start[e - 1] == ' '; --e) start[e - 1] = '\0';
            (void)SpCopyImage(c, root, start);
            const char *slash = strrchr(start, '/');
            (void)SpCopyStem(c, root, "images/characters", slash != NULL ? slash + 1 : start);
        }
        image = comma != NULL ? comma + 1 : NULL;
    }
    yyjson_doc_free(doc);

    SpIcon(c, root, icon[0] != '\0' ? icon : name);
    SpIcon(c, root, name);
    if (strcmp(root, c->root) != 0) {
        SpIcon(c, c->root, icon[0] != '\0' ? icon : name);
        SpIcon(c, c->root, name);
    }
}

/* .lua/.json do stage + imagens dos sprites do lua. */
static void SpStage(SpCtx *c, const char *stage)
{
    char path[300];
    if (stage == NULL || stage[0] == '\0') return;

    if (Stage_FindLua(stage, c->songDir, path)) {
        SpCopyFound(c, path);
        char *text = LoadFileText(path);
        StageSprite *sprites = text != NULL ? (StageSprite *)malloc(sizeof(StageSprite) * STAGE_MAX_SPRITES) : NULL;
        if (sprites != NULL) {
            const int count = Stage_ParseLua(text, sprites, STAGE_MAX_SPRITES, NULL, NULL);
            for (int i = 0; i < count; ++i)
                if (!sprites[i].isGraphic && sprites[i].image[0] != '\0')
                    (void)SpCopyImage(c, c->root, sprites[i].image);
            free(sprites);
        }
        UnloadFileText(text);
    }
    if (Stage_FindJsonAllowStage(stage, c->songDir, path)) SpCopyFound(c, path);
}

/* Mesma ordem do NoteSkinCache: mod da musica, assets base, mods ativos. */
static void SpNoteSkin(SpCtx *c, const char *skin)
{
    char base[SP_PATH], png[SP_PATH];
    if (skin == NULL || skin[0] == '\0') return;
    if (skin[0] == '@') SpNormalize(base, sizeof(base), skin + 1);
    else snprintf(base, sizeof(base), "NOTE_%s_assets", skin);
    if (!SpSafeRel(base) || SpCopyImage(c, c->root, base) > 0) return;

    snprintf(png, sizeof(png), "assets/images/%s.png", base);
    if (IsPathFile(png)) return;
    for (int i = 0; i < ModList_Count(); ++i) {
        const char *root = ModList_Root(i);
        if (root != NULL && ModList_Enabled(i) && SpCopyImage(c, root, base) > 0) return;
    }
}

/* dialogue.json da musica: retrato images/dialogue/<nome>.json + atlas do "image". */
static void SpDialogue(SpCtx *c)
{
    char path[SP_PATH];
    if (!SpJoin(path, c->songDir, "dialogue.json") || !IsPathFile(path)) return;

    DialogueFile *file = (DialogueFile *)malloc(sizeof(*file));
    DialogueCharDef *def = (DialogueCharDef *)malloc(sizeof(*def));
    if (file != NULL && def != NULL && Dialogue_Load(path, file)) {
        for (int i = 0; i < file->count; ++i) {
            const char *portrait = file->lines[i].portrait;
            bool dup = false;
            for (int j = 0; j < i && !dup; ++j) dup = strcmp(file->lines[j].portrait, portrait) == 0;
            if (dup || !SpSafeName(portrait)) continue;

            char rel[SP_PATH], src[SP_PATH], image[SP_PATH];
            snprintf(rel, sizeof(rel), "images/dialogue/%s.json", portrait);
            (void)SpCopyRel(c, c->root, rel);
            const bool hasDef = SpJoin(src, c->root, rel) && IsPathFile(src) &&
                                DialogueChar_Load(src, def) && def->image[0] != '\0';
            snprintf(image, sizeof(image), "dialogue/%s", hasDef ? def->image : portrait);
            (void)SpCopyImage(c, c->root, image);
        }
    }
    free(file);
    free(def);
}

/* ------------------------------------------------------------------------- */
/* API                                                                       */
/* ------------------------------------------------------------------------- */

static void SpAddName(char list[][SONG_PACKAGE_NAME_MAX], int *count, const char *name)
{
    if (name == NULL || name[0] == '\0' || *count >= SONG_PACKAGE_MAX) return;
    for (int i = 0; i < *count; ++i)
        if (strcmp(list[i], name) == 0) return;
    snprintf(list[*count], SONG_PACKAGE_NAME_MAX, "%s", name);
    ++*count;
}

void SongPackage_AddCharacter(SongPackageReq *req, const char *name)
{
    if (req != NULL) SpAddName(req->characters, &req->characterCount, name);
}

void SongPackage_AddImage(SongPackageReq *req, const char *name)
{
    if (req != NULL) SpAddName(req->images, &req->imageCount, name);
}

char SongPackage_IsPackage(const char *dir)
{
    char path[SP_PATH];
    return (char)(dir != NULL && SpJoin(path, dir, SONG_PACKAGE_MARKER) && IsPathFile(path));
}

/* [A-Za-z0-9 _-]; resto vira '_' (vira o nome do mod no import). */
static void SpPackageName(char *out, size_t size, const char *songDir)
{
    const char *base = GetFileName(songDir);
    size_t n = 0;
    for (; *base != '\0' && n + 1 < size; ++base) {
        const char ch = *base;
        const bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
                        ch == '-' || ch == '_' || ch == ' ';
        out[n++] = ok ? ch : '_';
    }
    out[n] = '\0';
    if (n == 0) snprintf(out, size, "song");
}

static bool SpWriteMarker(const SpCtx *c, const char *name, const char *songRel)
{
    char path[SP_PATH];
    if (!SpJoin(path, c->out, SONG_PACKAGE_MARKER)) return false;
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (doc == NULL) return false;
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_obj_add_str(doc, root, "format", "cengine-song-package");
    yyjson_mut_obj_add_int(doc, root, "version", 1);
    yyjson_mut_obj_add_strcpy(doc, root, "song", name);
    yyjson_mut_obj_add_strcpy(doc, root, "songDir", songRel);
    const bool ok = yyjson_mut_write_file(path, doc, YYJSON_WRITE_PRETTY, NULL, NULL);
    yyjson_mut_doc_free(doc);
    return ok;
}

static void SpMsg(char *msg, size_t size, const char *text)
{
    if (msg != NULL && size > 0) snprintf(msg, size, "%s", text);
}

char SongPackage_Export(const SongPackageReq *req, char *msg, size_t msgSize)
{
    char name[SONG_PACKAGE_NAME_MAX], to[SP_PATH];

    SpMsg(msg, msgSize, "");
    if (req == NULL || req->songDir == NULL || req->songDir[0] == '\0' || !DirectoryExists(req->songDir)) {
        SpMsg(msg, msgSize, "EXPORT: SAVE THE SONG FIRST");
        return 0;
    }
    SpCtx *c = (SpCtx *)calloc(1, sizeof(*c));
    if (c == NULL) {
        SpMsg(msg, msgSize, "EXPORT: OUT OF MEMORY");
        return 0;
    }

    SpNormalize(c->songDir, sizeof(c->songDir), req->songDir);
    if (!ModPath_Root(c->songDir, c->root, sizeof(c->root)))
        snprintf(c->root, sizeof(c->root), "assets");
    SpPackageName(name, sizeof(name), c->songDir);
    snprintf(c->out, sizeof(c->out), "%s/%s", SONG_PACKAGE_DIR, name);
    if (MakeDirectory(c->out) != 0) {
        SpMsg(msg, msgSize, "EXPORT: COULD NOT CREATE exports/");
        free(c);
        return 0;
    }

    /* 1) pasta da musica inteira: charts, audio, events, scripts, dialogo */
    const char *songRel = SpRelTo(c->songDir, c->root);
    if (strcmp(c->songDir, c->root) == 0) {
        songRel = "";
        SpCopyTree(c, c->songDir, c->out, 0); /* mod de uma musica so: o mod e a musica */
    } else {
        char fallback[SP_PATH];
        if (songRel == NULL) {
            snprintf(fallback, sizeof(fallback), "songs/%s", name);
            songRel = fallback;
        }
        if (SpJoin(to, c->out, songRel)) SpCopyTree(c, c->songDir, to, 0);
    }

    /* 2) o que a musica usa */
    for (int i = 0; i < req->characterCount; ++i) SpCharacter(c, req->characters[i]);
    SpStage(c, req->stage);
    SpNoteSkin(c, req->noteSkin);
    for (int i = 0; i < req->imageCount; ++i) (void)SpCopyImage(c, c->root, req->images[i]);
    SpDialogue(c);
    {
        char src[SP_PATH];
        if (SpJoin(src, c->root, "custom_notetypes") && SpJoin(to, c->out, "custom_notetypes"))
            SpCopyTree(c, src, to, 0);
    }

    if (!SpWriteMarker(c, name, songRel)) ++c->errors;

    const bool ok = c->errors == 0 && c->files > 0;
    if (msg != NULL && msgSize > 0) {
        if (ok)
            snprintf(msg, msgSize, "EXPORTED %s (%d files, %lld KB)", c->out, c->files, c->bytes / 1024LL);
        else
            snprintf(msg, msgSize, "EXPORT %s: %d errors, %d files", c->out, c->errors, c->files);
    }
    free(c);
    return (char)ok;
}
