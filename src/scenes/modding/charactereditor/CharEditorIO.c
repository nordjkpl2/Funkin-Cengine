/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Character Editor: IO. Usa a mesma busca do jogo (ModList + assets) e escreve o JSON com yyjson. */
#include "scenes/modding/charactereditor/CharEditorIO.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "raylib.h"
#include "characters/CharacterInternal.h"
#include "core/ModList.h"
#include "yyjson.h"

/* ------------------------------------------------------------------------- */
/* Lista                                                                     */
/* ------------------------------------------------------------------------- */

static bool Entry_Known(const CharEdEntry *out, int count, const char *name)
{
    for (int i = 0; i < count; ++i) {
        if (strcmp(out[i].name, name) == 0)
            return true;
    }
    return false;
}

static int Scan_Root(CharEdEntry *out, int count, int capacity, const char *root)
{
    char dir[CHARED_ROOT_MAX + 32];
    snprintf(dir, sizeof(dir), "%s/images/characters", root);
    if (!DirectoryExists(dir))
        return count;

    FilePathList list = LoadDirectoryFilesEx(dir, ".animset", false);
    for (unsigned int i = 0; i < list.count && count < capacity; ++i) {
        char stem[CHARED_NAME_MAX];
        Char_StemName(list.paths[i], stem, sizeof(stem));
        if (!stem[0] || stem[0] == '_' || !Char_IsSafeName(stem) || Entry_Known(out, count, stem))
            continue;

        char png[CHARED_ROOT_MAX + 64];
        snprintf(png, sizeof(png), "%s/%s.png", dir, stem);
        if (!FileExists(png))
            continue;

        CharEdEntry *entry = &out[count++];
        snprintf(entry->name, sizeof(entry->name), "%s", stem);
        snprintf(entry->root, sizeof(entry->root), "%s", root);
    }
    UnloadDirectoryFiles(list);
    return count;
}

int CharEd_ScanList(CharEdEntry *out, int capacity)
{
    int count = 0;
    if (!out || capacity <= 0)
        return 0;

    ModList_Rescan();
    for (int i = 0; i < ModList_Count(); ++i) {
        if (ModList_Enabled(i))
            count = Scan_Root(out, count, capacity, ModList_Root(i));
    }
    return Scan_Root(out, count, capacity, "assets");
}

/* ------------------------------------------------------------------------- */
/* Carregar / liberar                                                        */
/* ------------------------------------------------------------------------- */

bool CharEd_LoadCharacter(const CharEdEntry *entry, Character *character)
{
    if (!entry || !character || !Char_IsSafeName(entry->name))
        return false;

    /* Como oponente: o papel CHAR_BF inverte o flip_x do JSON e o save gravaria invertido.
       Exceto "bf", que so existe como embutido do jogador. */
    Character_Load(character, entry->name, strcmp(entry->name, "bf") == 0 ? CHAR_BF : CHAR_DAD);
    return true;
}

void CharEd_FreeCharacter(Character *character)
{
    if (!character)
        return;

    AnimationSet_FreeAll(&character->object.animationSet);
    if (character->icon.id)
        UnloadTexture(character->icon);
    character->icon = (Texture2D){0};
}

/* ------------------------------------------------------------------------- */
/* Salvar (JSON Psych, edicao in-place)                                      */
/* ------------------------------------------------------------------------- */

static void PutReal(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, double value)
{
    yyjson_mut_obj_remove_key(obj, key);
    yyjson_mut_obj_add_real(doc, obj, key, value);
}

static void PutBool(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, bool value)
{
    yyjson_mut_obj_remove_key(obj, key);
    yyjson_mut_obj_add_bool(doc, obj, key, value);
}

static void PutStr(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, const char *value)
{
    yyjson_mut_obj_remove_key(obj, key);
    yyjson_mut_obj_add_str(doc, obj, key, value);
}

static void PutInt(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, int value)
{
    yyjson_mut_obj_remove_key(obj, key);
    yyjson_mut_obj_add_int(doc, obj, key, value);
}

static void PutVec2(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, float x, float y)
{
    yyjson_mut_obj_remove_key(obj, key);
    yyjson_mut_val *arr = yyjson_mut_obj_add_arr(doc, obj, key);
    yyjson_mut_arr_add_real(doc, arr, x);
    yyjson_mut_arr_add_real(doc, arr, y);
}

/* Entrada de animacao do JSON cujo "name" (prefixo no atlas) e `name`. */
static yyjson_mut_val *FindAnimEntry(yyjson_mut_val *anims, const char *name)
{
    size_t index, max;
    yyjson_mut_val *entry;
    yyjson_mut_arr_foreach(anims, index, max, entry) {
        if (!yyjson_mut_is_obj(entry))
            continue;
        yyjson_mut_val *value = yyjson_mut_obj_get(entry, "name");
        if (value && yyjson_mut_is_str(value) && strcmp(yyjson_mut_get_str(value), name) == 0)
            return entry;
    }
    return NULL;
}

bool CharEd_Save(const CharEdEntry *entry, const Character *character, char *msg, size_t msgCap)
{
    if (msg && msgCap)
        msg[0] = '\0';
    if (!entry || !character || !Char_IsSafeName(entry->name))
        return false;

    char dir[CHARED_ROOT_MAX + 32];
    char path[CHARED_ROOT_MAX + 64];
    snprintf(dir, sizeof(dir), "%s/characters", entry->root);
    snprintf(path, sizeof(path), "%s/%s.json", dir, entry->name);
    MakeDirectory(dir);

    /* Se existe mas nao le, nao sobrescreve: perderia o que o autor escreveu. */
    yyjson_mut_doc *doc = NULL;
    if (FileExists(path)) {
        yyjson_doc *source = yyjson_read_file(path, 0, NULL, NULL);
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

    const RayAnimatedObject *object = &character->object;
    const float scale = fabsf(object->scaleY) > 0.0001f ? fabsf(object->scaleY) : 1.0f;
    const bool flip = object->scaleX < 0.0f;

    PutReal(doc, root, "scale", scale);
    PutBool(doc, root, "flip_x", flip);
    /* object.position ja tem o psychShift somado no load: o JSON guarda a posicao sem ele. */
    PutVec2(doc, root, "position", object->position.x - character->psychShift.x,
            object->position.y - character->psychShift.y);
    PutReal(doc, root, "sing_duration", character->singDuration);

    if (!yyjson_mut_obj_get(root, "healthicon"))
        PutStr(doc, root, "healthicon", entry->name);
    if (!yyjson_mut_obj_get(root, "image")) {
        char image[CHARED_NAME_MAX + 16];
        snprintf(image, sizeof(image), "characters/%s", entry->name);
        PutStr(doc, root, "image", image);
    }

    yyjson_mut_val *anims = yyjson_mut_obj_get(root, "animations");
    if (!anims || !yyjson_mut_is_arr(anims)) {
        yyjson_mut_obj_remove_key(root, "animations");
        anims = yyjson_mut_obj_add_arr(doc, root, "animations");
    }

    const RayAnimationHandler *set = &object->animationSet;
    for (int i = 0; i < set->animationCount; ++i) {
        const RayAnimation *anim = &set->animations[i];
        if (!anim->name[0])
            continue;

        yyjson_mut_val *animEntry = FindAnimEntry(anims, anim->name);
        if (!animEntry) {
            /* Animacao sem entrada no JSON: cria uma, com o nome do atlas como papel. */
            animEntry = yyjson_mut_obj(doc);
            yyjson_mut_arr_add_val(anims, animEntry);
            PutStr(doc, animEntry, "anim", anim->name);
            PutStr(doc, animEntry, "name", anim->name);
            yyjson_mut_obj_add_int(doc, animEntry, "fps", anim->fps > 0 ? anim->fps : 24);
        }
        if (anim->fps > 0)
            PutInt(doc, animEntry, "fps", anim->fps);
        PutBool(doc, animEntry, "loop", anim->looped != 0);
        PutVec2(doc, animEntry, "offsets", anim->animationOffset.x, anim->animationOffset.y);
    }

    const bool ok = yyjson_mut_write_file(path, doc, YYJSON_WRITE_PRETTY, NULL, NULL);
    yyjson_mut_doc_free(doc);

    if (msg && msgCap) {
        if (ok)
            snprintf(msg, msgCap, "salvo: %s", path);
        else
            snprintf(msg, msgCap, "ERRO ao salvar: %s", path);
    }
    return ok;
}
