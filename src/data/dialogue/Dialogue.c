/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * Dialogue.c - leitura/gravacao do dialogue.json e dos retratos (images/dialogue/<nome>.json).
 */

#include "data/dialogue/Dialogue.h"
#include "core/ModPath.h"
#include "raylib.h"
#include "yyjson.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static void CopyStr(char *dst, size_t cap, yyjson_val *v)
{
    if (yyjson_is_str(v))
        snprintf(dst, cap, "%s", yyjson_get_str(v));
}

static float ReadNum(yyjson_val *obj, const char *key, float fallback)
{
    yyjson_val *v = yyjson_obj_get(obj, key);
    return yyjson_is_num(v) ? (float)yyjson_get_num(v) : fallback;
}

static void ReadPair(yyjson_val *obj, const char *key, float out[2])
{
    yyjson_val *arr = yyjson_obj_get(obj, key);
    for (size_t i = 0; i < 2; ++i) {
        yyjson_val *v = yyjson_is_arr(arr) ? yyjson_arr_get(arr, i) : NULL;
        out[i] = yyjson_is_num(v) ? (float)yyjson_get_num(v) : 0.0f;
    }
}

void Dialogue_DefaultLine(DialogueLine *line)
{
    memset(line, 0, sizeof(*line));
    snprintf(line->portrait, sizeof(line->portrait), "%s", DIALOGUE_DEFAULT_CHAR);
    snprintf(line->expression, sizeof(line->expression), "talk");
    snprintf(line->boxState, sizeof(line->boxState), "normal");
    line->speed = DIALOGUE_DEFAULT_SPEED;
}

bool Dialogue_Load(const char *path, DialogueFile *out)
{
    out->count = 0;
    yyjson_doc *doc = path ? yyjson_read_file(path, YYJSON_READ_ALLOW_TRAILING_COMMAS, NULL, NULL) : NULL;
    if (!doc)
        return false;

    yyjson_val *list = yyjson_obj_get(yyjson_doc_get_root(doc), "dialogue");
    if (!yyjson_is_arr(list)) {
        yyjson_doc_free(doc);
        return false;
    }

    size_t idx, max;
    yyjson_val *item;
    yyjson_arr_foreach(list, idx, max, item) {
        if (out->count >= DIALOGUE_MAX_LINES || !yyjson_is_obj(item))
            continue;
        DialogueLine *line = &out->lines[out->count++];
        Dialogue_DefaultLine(line);
        line->expression[0] = '\0';   /* sem expressao: o retrato escolhe uma */
        CopyStr(line->portrait, sizeof(line->portrait), yyjson_obj_get(item, "portrait"));
        CopyStr(line->expression, sizeof(line->expression), yyjson_obj_get(item, "expression"));
        CopyStr(line->boxState, sizeof(line->boxState), yyjson_obj_get(item, "boxState"));
        CopyStr(line->sound, sizeof(line->sound), yyjson_obj_get(item, "sound"));
        CopyStr(line->text, sizeof(line->text), yyjson_obj_get(item, "text"));
        line->speed = ReadNum(item, "speed", DIALOGUE_DEFAULT_SPEED);
        if (!(line->speed == line->speed))   /* NaN */
            line->speed = DIALOGUE_DEFAULT_SPEED;
    }

    yyjson_doc_free(doc);
    return true;
}

bool Dialogue_Save(const char *path, const DialogueFile *in, char *msg, size_t msgCap)
{
    char dir[512];
    snprintf(dir, sizeof(dir), "%s", GetDirectoryPath(path));
    if (dir[0] && !DirectoryExists(dir))
        MakeDirectory(dir);

    /* Existe mas nao le: nao sobrescreve (perderia o que o autor escreveu). */
    yyjson_mut_doc *doc = NULL;
    if (FileExists(path)) {
        yyjson_doc *source = yyjson_read_file(path, YYJSON_READ_ALLOW_TRAILING_COMMAS, NULL, NULL);
        if (!source) {
            snprintf(msg, msgCap, "invalid JSON, not saved: %s", path);
            return false;
        }
        doc = yyjson_doc_mut_copy(source, NULL);
        yyjson_doc_free(source);
    }
    if (!doc)
        doc = yyjson_mut_doc_new(NULL);
    if (!doc) {
        snprintf(msg, msgCap, "out of memory to save");
        return false;
    }

    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    if (!root || !yyjson_mut_is_obj(root)) {
        root = yyjson_mut_obj(doc);
        yyjson_mut_doc_set_root(doc, root);
    }

    yyjson_mut_obj_remove_key(root, "dialogue");
    yyjson_mut_val *list = yyjson_mut_obj_add_arr(doc, root, "dialogue");
    for (int i = 0; i < in->count; ++i) {
        const DialogueLine *line = &in->lines[i];
        yyjson_mut_val *item = yyjson_mut_arr_add_obj(doc, list);
        yyjson_mut_obj_add_strcpy(doc, item, "portrait", line->portrait);
        yyjson_mut_obj_add_strcpy(doc, item, "expression", line->expression);
        yyjson_mut_obj_add_strcpy(doc, item, "text", line->text);
        yyjson_mut_obj_add_strcpy(doc, item, "boxState", line->boxState[0] ? line->boxState : "normal");
        yyjson_mut_obj_add_real(doc, item, "speed", round((double)line->speed * 10000.0) / 10000.0);
        yyjson_mut_obj_add_strcpy(doc, item, "sound", line->sound);
    }

    const bool ok = yyjson_mut_write_file(path, doc, YYJSON_WRITE_PRETTY, NULL, NULL);
    yyjson_mut_doc_free(doc);
    snprintf(msg, msgCap, ok ? "saved: %s" : "ERROR saving: %s", path);
    return ok;
}

bool DialogueChar_Load(const char *path, DialogueCharDef *out)
{
    memset(out, 0, sizeof(*out));
    out->scale = 1.0f;
    out->pos = DIALOGUE_POS_LEFT;

    yyjson_doc *doc = path ? yyjson_read_file(path, YYJSON_READ_ALLOW_TRAILING_COMMAS, NULL, NULL) : NULL;
    yyjson_val *root = doc ? yyjson_doc_get_root(doc) : NULL;
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        return false;
    }

    CopyStr(out->image, sizeof(out->image), yyjson_obj_get(root, "image"));
    char pos[16] = "left";
    CopyStr(pos, sizeof(pos), yyjson_obj_get(root, "dialogue_pos"));
    if (strcmp(pos, "center") == 0)
        out->pos = DIALOGUE_POS_CENTER;
    else if (strcmp(pos, "right") == 0)
        out->pos = DIALOGUE_POS_RIGHT;
    out->scale = ReadNum(root, "scale", 1.0f);
    ReadPair(root, "position", out->position);
    out->noAntialiasing = yyjson_get_bool(yyjson_obj_get(root, "no_antialiasing"));

    yyjson_val *anims = yyjson_obj_get(root, "animations");
    size_t idx, max;
    yyjson_val *item;
    yyjson_arr_foreach(anims, idx, max, item) {
        if (out->animCount >= DIALOGUE_ANIM_MAX || !yyjson_is_obj(item))
            continue;
        DialogueCharAnim *a = &out->anims[out->animCount++];
        CopyStr(a->anim, sizeof(a->anim), yyjson_obj_get(item, "anim"));
        CopyStr(a->loopName, sizeof(a->loopName), yyjson_obj_get(item, "loop_name"));
        CopyStr(a->idleName, sizeof(a->idleName), yyjson_obj_get(item, "idle_name"));
        ReadPair(item, "loop_offsets", a->loopOffset);
        ReadPair(item, "idle_offsets", a->idleOffset);
    }

    yyjson_doc_free(doc);
    return true;
}

bool Dialogue_FindAsset(const char *modRoot, const char *relative, char *out, size_t cap)
{
    if (ModPath_FindIn(modRoot, relative, out, cap))
        return true;
    if ((!modRoot || strcmp(modRoot, "assets/mods") != 0) && ModPath_FindIn("assets/mods", relative, out, cap))
        return true;
    return ModPath_FindIn("assets", relative, out, cap);
}

bool Dialogue_FindForSong(const char *songDir, char *out, size_t cap)
{
    snprintf(out, cap, "%s/dialogue.json", songDir);
    if (FileExists(out))
        return true;

    /* Layout Psych: <mod>/data/<musica>/dialogue.json */
    char root[512];
    if (ModPath_Root(songDir, root, sizeof(root))) {
        char dir[512];
        snprintf(dir, sizeof(dir), "%s", songDir);
        for (size_t n = strlen(dir); n > 0 && (dir[n - 1] == '/' || dir[n - 1] == '\\'); --n)
            dir[n - 1] = '\0';
        char name[128];
        snprintf(name, sizeof(name), "%s", GetFileName(dir));
        char rel[192];
        snprintf(rel, sizeof(rel), "data/%s/dialogue.json", name);
        if (ModPath_FindIn(root, rel, out, cap))
            return true;
    }
    out[0] = '\0';
    return false;
}
