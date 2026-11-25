/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * EditorStagePos.c - posicao de boyfriend / dad / gf no <stage>.json (mesmo arquivo que o jogo le).
 */
#include "scenes/modding/charteditor/model/EditorStagePos.h"

#include <stdio.h>
#include <string.h>

#include "raylib.h"
#include "yyjson.h"
#include "logic/stage/Stage.h"
#include "logic/stage/StageJson.h"
#include "scenes/modding/ModdingCommon.h"
#include "data/song/Song.h"
#include "data/song/SongList.h"
#include "scenes/api/FontLoader.h"
#include "scenes/modding/charteditor/lists/EditorCharactersAndLists.h"
#include "scenes/modding/charteditor/model/EditorChartModel.h"
#include "scenes/modding/charteditor/ui/EditorWidgets.h"

#define POS_STEP 10.0f

/* indices: 0 = boyfriend, 1 = opponent (dad), 2 = girlfriend, 3 = camera_boyfriend, 4 = camera_opponent */
#define POS_ROWS 5
static char cacheKey[400];
static char jsonPath[300];
static char hasJson = 0;
static char open = 0;     /* seta "positions" aberta */
static char dirty = 0;    /* ha mudanca nao salva */
static float pos[POS_ROWS][2];

/* Recarrega so quando trocam a stage ou a pasta da musica. */
static void Reload(const char* stage, const char* songDir) {
    char key[400];
    snprintf(key, sizeof(key), "%s|%s", stage, songDir);
    if(strcmp(key, cacheKey) == 0)
        return;
    snprintf(cacheKey, sizeof(cacheKey), "%s", key);
    hasJson = 0;
    jsonPath[0] = 0;
    if(stage[0] == 0 || strcmp(stage, "__not_have_a_stage") == 0)
        return;

    // mesma busca do jogo (Stage_FindJson): o arquivo editado e o que a gameplay le
    if(!Stage_FindJsonAllowStage(stage, songDir, jsonPath))
        return;

    StageMeta meta;
    if(!Stage_LoadJson(jsonPath, &meta))
        return;
    hasJson = 1;
    dirty = 0;
    memcpy(pos[0], meta.boyfriend, sizeof(pos[0]));
    memcpy(pos[1], meta.opponent, sizeof(pos[1]));
    memcpy(pos[2], meta.girlfriend, sizeof(pos[2]));
    memcpy(pos[3], meta.cameraBoyfriend, sizeof(pos[3]));
    memcpy(pos[4], meta.cameraOpponent, sizeof(pos[4]));
}

/* Regrava so as posicoes no json, mantendo os outros campos. */
static void Save(void) {
    yyjson_doc* src = yyjson_read_file(jsonPath, 0, NULL, NULL);
    if(src == NULL) {
        EdStatusMsg("stage.json invalido, nao salvei");
        return;
    }
    yyjson_mut_doc* doc = yyjson_doc_mut_copy(src, NULL);
    yyjson_doc_free(src);
    yyjson_mut_val* root = doc != NULL ? yyjson_mut_doc_get_root(doc) : NULL;
    if(root == NULL || !yyjson_mut_is_obj(root)) {
        if(doc != NULL) yyjson_mut_doc_free(doc);
        EdStatusMsg("stage.json invalido, nao salvei");
        return;
    }
    ModEd_PutRealArr2(doc, root, "boyfriend", pos[0][0], pos[0][1]);
    ModEd_PutRealArr2(doc, root, "opponent", pos[1][0], pos[1][1]);
    ModEd_PutRealArr2(doc, root, "girlfriend", pos[2][0], pos[2][1]);
    ModEd_PutRealArr2(doc, root, "camera_boyfriend", pos[3][0], pos[3][1]);
    ModEd_PutRealArr2(doc, root, "camera_opponent", pos[4][0], pos[4][1]);
    const bool ok = yyjson_mut_write_file(jsonPath, doc, YYJSON_WRITE_PRETTY, NULL, NULL);
    yyjson_mut_doc_free(doc);
    dirty = ok ? 0 : dirty;
    EdStatusMsg(ok ? "posicao salva no stage.json" : "ERRO ao salvar stage.json");
}

float StagePos_Draw(float x, float y, const char* stage, const char* songDir, char blocked) {
    const float top = y;
    Reload(stage, songDir);

    if(EdButton((Rectangle){x, y, 190, 30}, open ? "positions v" : "positions >", 20))
        open = !open;
    y += 38;
    if(!open)
        return y - top;
    if(!hasJson) {
        Ed_DrawText(mainFont, "sem stage.json (so .lua ou nenhum)", (Vector2){x, y + 4}, 18, 1, YELLOW);
        return y + 30 - top;
    }

    static const char* names[POS_ROWS] = {"BF:", "Dad:", "GF:", "cBF:", "cDad:"};
    for(int i = 0; i < POS_ROWS; i++) {
        const float ry = y + i * 34.0f;
        Ed_DrawText(mainFont, names[i], (Vector2){x, ry + 4}, 20, 1, WHITE);
        for(int a = 0; a < 2; a++) {
            const float bx = x + 60 + a * 130.0f;   // a = 0: x, a = 1: y
            Ed_DrawText(mainFont, a == 0 ? "x" : "y", (Vector2){bx, ry + 4}, 20, 1, WHITE);
            if(EdButton((Rectangle){bx + 18, ry, 26, 28}, "-", 20) && !blocked) {
                pos[i][a] -= POS_STEP;
                dirty = 1;
            }
            char tb[16];
            snprintf(tb, sizeof(tb), "%.0f", (double)pos[i][a]);
            Ed_DrawText(mainFont, tb, (Vector2){bx + 50, ry + 4}, 20, 1, WHITE);
            if(EdButton((Rectangle){bx + 100, ry, 26, 28}, "+", 20) && !blocked) {
                pos[i][a] += POS_STEP;
                dirty = 1;
            }
        }
    }
    const float by = y + POS_ROWS * 34.0f + 4;
    if(EdButton((Rectangle){x, by, 190, 32}, dirty ? "SALVAR POSICOES *" : "SALVAR POSICOES", 20) && !blocked && dirty)
        Save();
    return by + 32 - top + 4;
}
