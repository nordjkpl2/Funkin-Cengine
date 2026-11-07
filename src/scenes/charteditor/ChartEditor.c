/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* // * Código do Chart Editor */
#include"scenes/AllScenes.h"
#include"data/Song.h"
#include"data/SongList.h"
#include "core/ModPath.h"
#include "logic/Character.h"
#include"logic/Stage.h"
#include"logic/Note.h"
#include "Game.h"
#include "GameData.h"
#include "raymath.h"
#include "EditorDataAndIO.h"
#include "EditorTimeline.h"
#include "EditorAudio.h"
#include "EditorWidgets.h"
#include "EditorGrid.h"
#include "EditorCharactersAndLists.h"
#include "EditorIcons.h"
#include "EditorPicker.h"
#include "EditorPanels.h"

// ---------- dialogs (por cima de tudo) ----------
static void Dialogs_Update(void) {
    if(ceDlg == DLG_NONE)
        return;
    if(IsKeyPressed(KEY_ESCAPE)) {
        ceDlg = DLG_NONE;
        focusBox = NULL;
        importPath[0] = 0;
        return;
    }
    if(IsKeyPressed(KEY_ENTER)) {
        if(ceDlg == DLG_LEAVE) {
            Editor_Save();
            Editor_LeaveToPicker();
            return;
        }
        if(ceDlg == DLG_OPEN) {
            if(openBox.buf[0] == 0) {
                EdStatusMsg("COLE O CAMINHO PRIMEIRO");
                return;
            }
            EdChart tmp;
            memset(&tmp, 0, sizeof(tmp));
            if(EdChart_LoadJSON(&tmp, openBox.buf, "", EdDirName(openBox.buf))) {
                EdChart_Free(&tmp);
                strncpy(importPath, openBox.buf, sizeof(importPath) - 1);
                saveAsBox.buf[0] = 0;
                ceDlg = DLG_SAVEAS;
                focusBox = &saveAsBox;
            } else {
                EdStatusMsg("NAO ABRIU: confira o caminho");
            }
        } else if(ceDlg == DLG_SAVEAS) {
            char ok = 0;
            if(importPath[0] != 0)
                ok = ImportJSON(importPath, saveAsBox.buf);
            else
                ok = Editor_SaveAs(saveAsBox.buf);
            if(ok) {
                ceDlg = DLG_NONE;
                focusBox = NULL;
                importPath[0] = 0;
            }
        }
    }
}

static void Dialogs_Draw(void) {
    if(ceDlg == DLG_NONE)
        return;
    Render_SetCamera(&camEd);
    DrawRectangle(0, 0, 1280, 720, (Color) {0, 0, 0, 180});
    if(ceDlg == DLG_LEAVE) {
        DrawTextEx(mainFont, "Salvar as alteracoes?", (Vector2) {340, 250}, 40, 3, WHITE);
        DrawTextEx(mainFont, "enter salva, esc cancela", (Vector2) {340, 310}, 20, 1, YELLOW);
        if(EdDlgButton((Rectangle) {340, 360, 170, 48}, "SALVAR", GREEN)) {
            Editor_Save();
            Editor_LeaveToPicker();
        } else if(EdDlgButton((Rectangle) {530, 360, 170, 48}, "DESCARTAR", RED)) {
            chart.dirty = 0;
            Editor_LeaveToPicker();
        } else if(EdDlgButton((Rectangle) {720, 360, 170, 48}, "CANCELAR", GRAY)) {
            ceDlg = DLG_NONE;
        }
    } else if(ceDlg == DLG_OPEN) {
        DrawTextEx(mainFont, ".json path", (Vector2) {340, 250}, 40, 3, WHITE);
        Rectangle r = {340, 320, 600, 40};
        TxtBox_Update(&openBox, r, 0);
        TxtBox_Draw(&openBox, r, "C:/.../musica.json");
        DrawTextEx(mainFont, "enter import, esc cancel", (Vector2) {340, 380}, 20, 1, YELLOW);
        DrawTextEx(mainFont, "You can copy and paste the path to the .json file.", (Vector2) {340, 410}, 20, 1, YELLOW);
    } else {
        DrawTextEx(mainFont, "Save (path name):", (Vector2) {140, 190}, 36, 3, WHITE);
        Rectangle r = {140, 260, 600, 40};
        TxtBox_Update(&saveAsBox, r, 0);
        TxtBox_Draw(&saveAsBox, r, "my-music-renameonexplorer");
        DrawTextEx(mainFont, "enter save, esc cancel", (Vector2) {140, 320}, 20, 1, YELLOW);
        DrawTextEx(mainFont, "renames the folder if it already exists", (Vector2) {140, 350}, 20, 1, YELLOW);
        {
            char clean[64];
            EdSanitizeName(clean, sizeof(clean), saveAsBox.buf);
            char where[192];
            char dstDir[128];
            if(clean[0] != 0) {
                snprintf(dstDir, sizeof(dstDir), "assets/mods/%s", clean);
                snprintf(where, sizeof(where), "saves to: %s/data.json", dstDir);
            } else {
                snprintf(dstDir, sizeof(dstDir), "assets/mods/<your-song>");
                snprintf(where, sizeof(where), "saves to: mods/<your-song>");
            }
            DrawTextEx(mainFont, where, (Vector2) {140, 380}, 20, 1, YELLOW);
            if(importPath[0] != 0) {
                char from[96];
                size_t L = strlen(importPath);
                const char* tail = (L > 52) ? importPath + L - 52 : importPath;
                snprintf(from, sizeof(from), "from: ...%s", (L > 52) ? tail : importPath);
                DrawTextEx(mainFont, from, (Vector2) {140, 410}, 18, 1, GRAY);
            }
            // mini arvore do destino (leve: so re-checa quando o nome muda)
            static char lastTree[64] = {0};
            static char tChart = 0, tInst = 0, tVox = 0;
            if(strcmp(lastTree, clean) != 0) {
                strncpy(lastTree, clean, sizeof(lastTree) - 1);
                lastTree[sizeof(lastTree) - 1] = 0;
                tChart = tInst = tVox = 0;
                if(clean[0] != 0) {
                    tChart = Song_HasChart(dstDir);
                    char p[176];
                    // se o destino ja existe mostra o destino, senao mostra o que sera copiado da origem
                    const char* srcDir = (tChart || chart.dir[0] == 0) ? dstDir : chart.dir;
                    snprintf(p, sizeof(p), "%s/Inst.ogg", srcDir);
                    FILE* f = fopen(p, "rb");
                    if(f != NULL) { fclose(f); tInst = 1; }
                    snprintf(p, sizeof(p), "%s/Voices.ogg", srcDir);
                    f = fopen(p, "rb");
                    if(f != NULL) { fclose(f); tVox = 1; }
                }
            }
            float tx = 780;
            float ty = 260;
            DrawTextEx(mainFont, "tree:", (Vector2) {tx, ty}, 20, 1, GREEN);
            ty += 30;
            DrawTextEx(mainFont, "assets/", (Vector2) {tx, ty}, 18, 1, GRAY);
            ty += 24;
            DrawTextEx(mainFont, "  mods/", (Vector2) {tx, ty}, 18, 1, GRAY);
            ty += 24;
            {
                char mod[80];
                if(clean[0] != 0) snprintf(mod, sizeof(mod), "    %.28s/", clean);
                else snprintf(mod, sizeof(mod), "    <your-song>/");
                DrawTextEx(mainFont, mod, (Vector2) {tx, ty}, 18, 1, WHITE);
                DrawTextEx(mainFont, tChart ? "[existe]" : "[novo]", (Vector2) {tx + 240, ty}, 18, 1, tChart ? YELLOW : GREEN);
            }
            ty += 24;
            DrawTextEx(mainFont, "      data.json", (Vector2) {tx, ty}, 18, 1, WHITE);
            ty += 24;
            DrawTextEx(mainFont, tInst ? "      Inst.ogg [ok]" : "      Inst.ogg [--]", (Vector2) {tx, ty}, 18, 1, tInst ? GREEN : GRAY);
            ty += 24;
            DrawTextEx(mainFont, tVox ? "      Voices.ogg [ok]" : "      Voices.ogg [--]", (Vector2) {tx, ty}, 18, 1, tVox ? GREEN : GRAY);
        }
    }
    Render_StopCamera();
}

static void Edit_UpdateKeys(void) {
    int ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    int shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

    if(ctrl && IsKeyPressed(KEY_S)) {
        Editor_Save();
        return;
    }
    if(ctrl && IsKeyPressed(KEY_Z) && focusBox == NULL && focusNum == NULL) {
        Undo_Pop();
        chart.dirty = 1;
        EdStatusMsg("actions reverted");
        return;
    }
    // digitando numa caixinha: nao dispara atalho
    if(menuOpen && IsKeyPressed(KEY_ESCAPE)) {
        menuOpen = 0;
        return;
    }
    if(activeDrop && IsKeyPressed(KEY_ESCAPE)) {
        activeDrop = 0;
        return;
    }
    if(focusNum != NULL) {
        if(IsKeyPressed(KEY_ENTER))
            NumBox_Commit();
        else if(IsKeyPressed(KEY_ESCAPE))
            focusNum = NULL;
        return;
    }
    if(focusBox != NULL) {
        if(IsKeyPressed(KEY_ESCAPE))
            focusBox = NULL;
        return;
    }
    if(IsKeyPressed(KEY_SPACE)) {
        Audio_Toggle();
        return;
    }
    if(IsKeyPressed(KEY_F5)) {
        Audio_StopPlay();
        Editor_Save();
        PlayState_SetSongDir(chart.dir);
        PlayState_SetReturnEditor(1);
        PlayState_SetScene();
        return;
    }
    if(IsKeyPressed(KEY_TAB)) {
        EdTouch();
        chart.secs[secIdx].mustHit = !chart.secs[secIdx].mustHit;
        EdStatusMsg(chart.secs[secIdx].mustHit ? "Turn: You" : "Turn: Opponent");
        return;
    }
    if(IsKeyPressed(KEY_T)) {
        snapDiv = (snapDiv == 4) ? 1 : (snapDiv == 1) ? 2 : 4;
        return;
    }    if(IsKeyPressed(KEY_N)) {
        EdTouch();
        EdSec* last = chart.secs + chart.nsec - 1;
        EdSec* g = realloc(chart.secs, sizeof(EdSec) * (size_t)(chart.nsec + 1));
        if(g != NULL) {
            chart.secs = g;
            EdSec* n = chart.secs + chart.nsec;
            memset(n, 0, sizeof(*n));
            n->mustHit = last->mustHit;
            n->bpm = last->bpm;
            n->lenSteps = 16;
            chart.nsec++;
            secIdx = chart.nsec - 1;
            cursorMs = SecStartMs(secIdx);
            viewTopMs = cursorMs - VIEW_MS * 0.3;
            if(viewTopMs < 0) viewTopMs = 0;
            EdStatusMsg("TRECHO NOVO NO FIM");
        }
        return;
    }
    if(IsKeyPressed(KEY_X)) {
        /* // * part deleter */
        if(chart.nsec > 1) {
            EdTouch();
            free(chart.secs[secIdx].notes);
            memmove(chart.secs + secIdx, chart.secs + secIdx + 1,
                sizeof(EdSec) * (size_t)(chart.nsec - secIdx - 1));
            chart.nsec--;
            if(secIdx >= chart.nsec) secIdx = chart.nsec - 1;
            EdStatusMsg("part deleted");
        } else {
            EdStatusMsg("NOT POSSIBLE TO DELETE HAVE ONLY 1 PART");
        }
        return;
    }
    if(IsKeyPressed(KEY_C)) {
        /* // * part copier */
        if(copyValid) free(copyBuf.notes);
        memset(&copyBuf, 0, sizeof(copyBuf));
        EdSec* s = chart.secs + secIdx;
        copyBuf.mustHit = s->mustHit;
        copyBuf.bpm = s->bpm;
        copyBuf.lenSteps = s->lenSteps;
        if(s->count > 0) {
            copyBuf.notes = malloc(sizeof(EdNote) * (size_t)s->count);
            copyBuf.count = s->count;
            copyBuf.cap = s->count;
            memcpy(copyBuf.notes, s->notes, sizeof(EdNote) * (size_t)s->count);
        }
        copyValid = 1;
        EdStatusMsg("part Copyed");
        return;
    }
    if(IsKeyPressed(KEY_V)) {
        /* // * if pressing control v without control c */
        if(!copyValid) {
            EdStatusMsg("V pressed missing Copy Before (C)");
            return;
        }
        EdTouch();
        double base = SongEndMs();
        double srcStart = SecStartMs(secIdx);
        EdSec* g = realloc(chart.secs, sizeof(EdSec) * (size_t)(chart.nsec + 1));
        if(g != NULL) {
            chart.secs = g;
            EdSec* n = chart.secs + chart.nsec;
            memset(n, 0, sizeof(*n));
            n->mustHit = copyBuf.mustHit;
            n->bpm = chart.secs[chart.nsec - 1].bpm;
            n->lenSteps = copyBuf.lenSteps;
            for(int i = 0; i < copyBuf.count; i++)
                EdSec_Add(n, (float)(base + (copyBuf.notes[i].time - srcStart)), copyBuf.notes[i].lane, copyBuf.notes[i].len);
            chart.nsec++;
            secIdx = chart.nsec - 1;
            EdStatusMsg("Copyed on End");
        }
        return;
    }
    if(IsKeyPressed(KEY_G)) {
        secIdx--;
        if(secIdx < 0) secIdx = 0;
        cursorMs = SecStartMs(secIdx);
        viewTopMs = cursorMs - VIEW_MS * 0.3;
        if(viewTopMs < 0) viewTopMs = 0;
        return;
    }
    if(IsKeyPressed(KEY_H)) {
        secIdx++;
        if(secIdx >= chart.nsec) secIdx = chart.nsec - 1;
        cursorMs = SecStartMs(secIdx);
        viewTopMs = cursorMs - VIEW_MS * 0.3;
        if(viewTopMs < 0) viewTopMs = 0;
        return;
    }
    if(IsKeyPressed(KEY_HOME)) {
        cursorMs = SecStartMs(secIdx);
        return;
    }
    if(IsKeyPressed(KEY_COMMA) || IsKeyPressed(KEY_PERIOD)) {
        EdTouch();
        float d = (IsKeyPressed(KEY_PERIOD) ? 1 : -1) * (shift ? 10 : 1);
        chart.secs[secIdx].bpm += d;
        if(chart.secs[secIdx].bpm < 10) chart.secs[secIdx].bpm = 10;
        if(chart.secs[secIdx].bpm > 300) chart.secs[secIdx].bpm = 300;
        return;
    }
    if(IsKeyPressed(KEY_ESCAPE)) {
        if(focusBox != NULL) {
            focusBox = NULL;
            return;
        }
        Editor_RequestLeave();
        return;
    }
}

static void Edit_Update(void) {
    if(ceDlg != DLG_NONE)
        return;
    // volume global (+/-) vale pro OUVIR tambem (so reaplica se mudou)
    {
        float v = RayGame_MusicVolumeLevel();
        if(v != edVolApplied) {
            edVolApplied = v;
            Audio_SetVolume(v);
        }
    }
    Edit_UpdateKeys();

    // roda a musica / relogio (tempo mestre = Inst, senao voz, senao silencio)
    if(playing) {
        Audio_Update();
        if(edHasAudio)
            cursorMs = GetMusicTimePlayed(edInst) * 1000.0;
        else if(edHasVoices)
            cursorMs = GetMusicTimePlayed(edVoices) * 1000.0;
        else if(edHasSplit)
            cursorMs = GetMusicTimePlayed(edSplitO) * 1000.0;
        else
            cursorMs += RayGame_DeltaTime() * 1000.0;
        if(cursorMs >= SongEndMs() + 2000)
            Audio_StopPlay();
        else if(playFollow)
            viewTopMs = cursorMs - VIEW_MS * 0.25; // trava no receptor, igual ao jogo
    }

    // setas e W/S sobem e descem a grade
    if(IsKeyDown(KEY_UP) || IsKeyDown(KEY_W))
        viewTopMs -= 2600 * RayGame_DeltaTime();
    if(IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S))
        viewTopMs += 2600 * RayGame_DeltaTime();
    if(viewTopMs < -500) viewTopMs = -500;
    {
        double maxTop = SongEndMs() + 1000;
        if(viewTopMs > maxTop) viewTopMs = maxTop;
    }

    // roda do mouse = sobe/desce
    float wheel = GetMouseWheelMove();
    if(wheel != 0) {
        Vector2 m = EdMouseWorld();
        int shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        int ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        if((shift || ctrl) && m.x >= GRID_X0 && m.x <= GRID_X0 + LANE_W * 8 && m.y >= GRID_Y0 && m.y <= GRID_Y1) {
            //na nota embaixo do mouse: muda a nota longa
            int lane = (int)((m.x - GRID_X0) / LANE_W);
            double t = viewTopMs + (m.y - GRID_Y0) / EdPxPerMs();
            int si = SecAtTime(t);
            EdSec* s = chart.secs + si;
            lane = EdDispLane(s, lane); // casa da tela -> casa do arquivo
            float beatMs = 60000.0f / s->bpm;
            for(int i = 0; i < s->count; i++) {
                if(s->notes[i].lane == lane && fabsf(s->notes[i].time - (float)t) < beatMs / 2) {
                    EdTouch();
                    s->notes[i].len += wheel * beatMs / 4;
                    if(s->notes[i].len < 0) s->notes[i].len = 0;
                    if(s->notes[i].len > 10000) s->notes[i].len = 10000;
                    break;
                }
            }
        } else {
            viewTopMs -= wheel * 150;
            if(viewTopMs < -500) viewTopMs = -500;
        }
    }
}

// (removido: migrado p/ abas Song/Section)

static void Edit_Draw(void) {
    Render_SetCamera(&camEd);
    Render_DrawGraphicObject(&bgEd);

    // tempo no canto + status
    char tbuf[32];
    EdTimeFmt(tbuf, sizeof(tbuf), cursorMs);
    DrawTextEx(mainFont, tbuf, (Vector2) {1110, 2}, 24, 2, WHITE);
    // status embaixo da grade (antes caia em cima do painel, lado direito)
    if(statusTimer > 0) {
        DrawTextEx(mainFont, statusText, (Vector2) {GRID_X0 + 2, 692}, 20, 1, (Color){0, 0, 0, 200});
        DrawTextEx(mainFont, statusText, (Vector2) {GRID_X0, 690}, 20, 1, YELLOW);
    }

    Edit_DrawGrid();
    Edit_DrawTabs();
    Edit_DrawPanel();
    Edit_DrawMenuBar();
    CharDrop_Draw();
    Render_StopCamera();
}

// ---------- cena ----------
static void ChartEditor_Create([[maybe_unused]] RayScene* scene) {
    Render_DefaultRGT(&bgEd);
    bgEd.objType = RGT_IMAGE;
    bgEd.image = Render_LoadTexture("assets/images/freeplay/menuBGBlue.png");

    camEd.target = (Vector2) {1280 / 2, 720 / 2};
    camEd.zoom = 1;
    camEd.rotation = 0;
    camEd.offset = (Vector2) {0, 0};

    EdArrows_Init();

    if(pendingDir[0] != 0) {
        char tmp[256];
        strncpy(tmp, pendingDir, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = 0;
        pendingDir[0] = 0;
        ceMode = CE_PICKER;
        Picker_Refresh();
        Editor_EnterDir(tmp);
    } else {
        ceMode = CE_PICKER;
        ceDlg = DLG_NONE;
        chartLoaded = 0;
        Picker_Refresh();
    }
    openBox.buf[0] = 0;
    saveAsBox.buf[0] = 0;
    importPath[0] = 0;
    focusBox = NULL;
    statusText[0] = 0;
    statusTimer = 0;
}

static void ChartEditor_Update([[maybe_unused]] RayScene* scene) {
    if(statusTimer > 0)
        statusTimer -= RayGame_DeltaTime();
    // dialog aberto neste mesmo frame (ex.: ESC) so recebe teclas no proximo,
    // senao o mesmo ESC que abriu ja fecharia
    CeDlg dlgAtStart = ceDlg;
    if(ceMode == CE_PICKER)
        Picker_Update();
    else
        Edit_Update();
    if(dlgAtStart != DLG_NONE)
        Dialogs_Update();
}

static void ChartEditor_Draw([[maybe_unused]] RayScene* scene) {
    if(ceMode == CE_PICKER)
        Picker_Draw();
    else
        Edit_Draw();
    Dialogs_Draw();
    if(statusTimer > 0 && ceMode == CE_PICKER)
        DrawTextEx(mainFont, statusText, (Vector2) {150, 640}, 24, 2, YELLOW);
}

static void ChartEditor_Destroy([[maybe_unused]] RayScene* scene) {
    UnloadTexture(bgEd.image);
    SecTimes_Drop();
    SongList_Free(pickerSongs);
    pickerSongs = NULL;
    pickerCount = 0;
    Audio_StopPlay();
    if(edHasAudio) {
        UnloadMusicStream(edInst);
        edHasAudio = 0;
    }
    if(edHasVoices) {
        UnloadMusicStream(edVoices);
        edHasVoices = 0;
    }
    if(edHasSplit) {
        UnloadMusicStream(edSplitO);
        UnloadMusicStream(edSplitP);
        edHasSplit = 0;
    }
    Editor_UnloadIcons();
    EdChart_Free(&chart);
    chartLoaded = 0;
    Undo_Clear();
    if(copyValid) {
        free(copyBuf.notes);
        memset(&copyBuf, 0, sizeof(copyBuf));
        copyValid = 0;
    }
    focusBox = NULL;
}

void ChartEditor_SetSongDir(const char* dir) {
    strncpy(pendingDir, dir, sizeof(pendingDir) - 1);
    pendingDir[sizeof(pendingDir) - 1] = 0;
}

static RayScene chartScene;

Scene_MakeSceneCode(
    chartScene,
    ChartEditor_SetScene,
    ChartEditor_Create,
    ChartEditor_Update,
    ChartEditor_Draw,
    ChartEditor_Destroy
)
