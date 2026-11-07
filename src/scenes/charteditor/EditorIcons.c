/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: carrega e solta os icones de vida do oponente e do jogador. */
#include"scenes/AllScenes.h"
#include"data/Song.h"
#include"data/SongList.h"
#include "core/ModPath.h"
#include "logic/Character.h"
#include"logic/Stage.h"
#include"logic/Note.h"
#include"yyjson.h"
#include "Game.h"
#include "GameData.h"
#include "raymath.h"
#include "EditorDataAndIO.h"
#include "EditorTimeline.h"
#include "EditorAudio.h"
#include "EditorWidgets.h"
#include "EditorGrid.h"
#include "EditorIcons.h"
#include "EditorCharactersAndLists.h"

void Editor_UnloadIcons(void) {
    if(edIconOOk) {
        UnloadTexture(edIconO);
        edIconOOk = 0;
    }
    if(edIconPOk) {
        UnloadTexture(edIconP);
        edIconPOk = 0;
    }
}

// "healthicon" do characters/<nome>.json (mod da musica primeiro, depois global); "" = nao tem
static void Editor_HealthIcon(const char* name, const char* modRoot, char out[SONG_NAME_MAX]) {
    out[0] = 0;
    char p[300];
    for(int pass = 0; pass < 2; pass++) {
        if(pass == 0 && modRoot[0] == 0)
            continue;
        if(pass == 0)
            snprintf(p, sizeof(p), "%s/characters/%s.json", modRoot, name);
        else
            snprintf(p, sizeof(p), "assets/characters/%s.json", name);
        yyjson_doc* doc = yyjson_read_file(p, 0, NULL, NULL);
        if(doc == NULL)
            continue;
        yyjson_val* v = yyjson_obj_get(yyjson_doc_get_root(doc), "healthicon");
        if(yyjson_is_str(v) && strchr(yyjson_get_str(v), '/') == NULL && strchr(yyjson_get_str(v), '\\') == NULL
            && strstr(yyjson_get_str(v), "..") == NULL)
            snprintf(out, SONG_NAME_MAX, "%s", yyjson_get_str(v));
        yyjson_doc_free(doc);
        return;
    }
}

// acha icons/<nome>.png no mod da musica e depois no global
static char Editor_FindIconFile(const char* name, const char* modRoot, char out[300]) {
    if(name[0] == 0 || strchr(name, '/') != NULL || strchr(name, '\\') != NULL || strstr(name, "..") != NULL)
        return 0;
    if(modRoot[0] != 0) {
        snprintf(out, 300, "%s/images/icons/%s.png", modRoot, name);
        if(FileExists(out))
            return 1;
    }
    snprintf(out, 300, "assets/images/icons/%s.png", name);
    return FileExists(out);
}

// mesma ordem do jogo: healthicon do json, nome do personagem, fallback (bf/dad)
static Texture2D Editor_TryIcon(const char* name, const char* fallback, char* ok) {
    const char* want = (name != NULL && name[0] != 0) ? name : fallback;
    char modRoot[256];
    EdModRoot(chart.dir, modRoot);
    char health[SONG_NAME_MAX];
    char p[300];
    Editor_HealthIcon(want, modRoot, health);
    if((health[0] != 0 && Editor_FindIconFile(health, modRoot, p)) || Editor_FindIconFile(want, modRoot, p)
        || (fallback != NULL && Editor_FindIconFile(fallback, "", p))) {
        *ok = 1;
        return Render_LoadTexture(p);
    }
    *ok = 0;
    return (Texture2D) {0};
}

void Editor_LoadIcons(void) {
    Editor_UnloadIcons();
    // p2 vazio (tutorial): sem icone de oponente
    if(chart.p2[0] == 0) {
        edIconOOk = 0;
    } else {
        edIconO = Editor_TryIcon(chart.p2, "dad", &edIconOOk);
    }
    edIconP = Editor_TryIcon(chart.p1, "bf", &edIconPOk);
}
