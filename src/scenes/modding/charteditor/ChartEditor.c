/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* // * Código do Chart Editor */
#include "scenes/api/FontLoader.h"
#include "scenes/modding/charteditor/ChartEditor.h"
#include "scenes/gameplay/PlayState.h"
#include"data/song/Song.h"
#include"data/song/SongList.h"
#include "core/ModPath.h"
#include "Game.h"
#include "raymath.h"
#include "scenes/modding/charteditor/io/EditorDataAndIO.h"
#include "scenes/modding/charteditor/grid/EditorTimeline.h"
#include "scenes/modding/charteditor/audio/EditorAudio.h"
#include "scenes/modding/charteditor/ui/EditorWidgets.h"
#include "scenes/modding/charteditor/grid/EditorGrid.h"
#include "scenes/modding/charteditor/lists/EditorCharactersAndLists.h"
#include "scenes/modding/charteditor/ui/EditorIcons.h"
#include "scenes/modding/charteditor/ui/EditorPicker.h"
#include "scenes/modding/charteditor/ui/EditorPanels.h"
#include "scenes/modding/charteditor/EditorActions.h"

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
                EdStatusMsg("PASTE THE PATH FIRST");
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
                EdStatusMsg("COULD NOT OPEN: check the path");
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
    const Rectangle card = (ceDlg == DLG_SAVEAS) ? (Rectangle){110, 165, 1040, 300}
                         : (ceDlg == DLG_OPEN)   ? (Rectangle){310, 220, 680, 230}
                                                 : (Rectangle){310, 220, 610, 220};
    DrawRectangleRounded(card, 0.06f, 8, ED_C_POPUP);
    DrawRectangleRoundedLinesEx(card, 0.06f, 8, 1.5f, ED_C_EDGE);
    DrawRectangleRounded((Rectangle){card.x + 12, card.y + 26, 5, 40}, 0.5f, 4, ED_C_ACCENT);
    if(ceDlg == DLG_LEAVE) {
        Ed_DrawText(mainFont, "Save changes?", (Vector2) {340, 250}, 40, 3, WHITE);
        Ed_DrawText(mainFont, "enter saves, esc cancels", (Vector2) {340, 310}, 20, 1, ED_C_HINT);
        if(EdDlgButton((Rectangle) {340, 360, 170, 48}, "SAVE", ED_C_OK)) {
            Editor_Save();
            Editor_LeaveToPicker();
        } else if(EdDlgButton((Rectangle) {530, 360, 170, 48}, "DISCARD", ED_C_BAD)) {
            chart.dirty = 0;
            Editor_LeaveToPicker();
        } else if(EdDlgButton((Rectangle) {720, 360, 170, 48}, "CANCEL", ED_C_EDGE)) {
            ceDlg = DLG_NONE;
        }
    } else if(ceDlg == DLG_OPEN) {
        Ed_DrawText(mainFont, ".json path", (Vector2) {340, 250}, 40, 3, WHITE);
        Rectangle r = {340, 320, 600, 40};
        TxtBox_Update(&openBox, r, 0);
        TxtBox_Draw(&openBox, r, "C:/.../song.json");
        Ed_DrawText(mainFont, "enter import, esc cancel", (Vector2) {340, 380}, 20, 1, ED_C_HINT);
        Ed_DrawText(mainFont, "You can copy and paste the path to the .json file.", (Vector2) {340, 410}, 20, 1, ED_C_DIM);
    } else {
        Ed_DrawText(mainFont, "Save (path name):", (Vector2) {140, 190}, 36, 3, WHITE);
        Rectangle r = {140, 260, 600, 40};
        TxtBox_Update(&saveAsBox, r, 0);
        TxtBox_Draw(&saveAsBox, r, "my-music-renameonexplorer");
        Ed_DrawText(mainFont, "enter save, esc cancel", (Vector2) {140, 320}, 20, 1, ED_C_HINT);
        Ed_DrawText(mainFont, "renames the folder if it already exists", (Vector2) {140, 350}, 20, 1, ED_C_DIM);
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
            Ed_DrawText(mainFont, where, (Vector2) {140, 380}, 20, 1, ED_C_DIM);
            if(importPath[0] != 0) {
                char from[96];
                size_t L = strlen(importPath);
                const char* tail = (L > 52) ? importPath + L - 52 : importPath;
                snprintf(from, sizeof(from), "from: ...%s", (L > 52) ? tail : importPath);
                Ed_DrawText(mainFont, from, (Vector2) {140, 410}, 18, 1, ED_C_DIM);
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
            Ed_DrawText(mainFont, "tree:", (Vector2) {tx, ty}, 20, 1, ED_C_ACCENT);
            ty += 30;
            Ed_DrawText(mainFont, "assets/", (Vector2) {tx, ty}, 18, 1, ED_C_DIM);
            ty += 24;
            Ed_DrawText(mainFont, "  mods/", (Vector2) {tx, ty}, 18, 1, ED_C_DIM);
            ty += 24;
            {
                char mod[80];
                if(clean[0] != 0) snprintf(mod, sizeof(mod), "    %.28s/", clean);
                else snprintf(mod, sizeof(mod), "    <your-song>/");
                Ed_DrawText(mainFont, mod, (Vector2) {tx, ty}, 18, 1, WHITE);
                Ed_DrawText(mainFont, tChart ? "[exists]" : "[new]", (Vector2) {tx + 240, ty}, 18, 1, tChart ? ED_C_HINT : ED_C_OK);
            }
            ty += 24;
            Ed_DrawText(mainFont, "      data.json", (Vector2) {tx, ty}, 18, 1, WHITE);
            ty += 24;
            Ed_DrawText(mainFont, tInst ? "      Inst.ogg [ok]" : "      Inst.ogg [--]", (Vector2) {tx, ty}, 18, 1, tInst ? ED_C_OK : ED_C_DIM);
            ty += 24;
            Ed_DrawText(mainFont, tVox ? "      Voices.ogg [ok]" : "      Voices.ogg [--]", (Vector2) {tx, ty}, 18, 1, tVox ? ED_C_OK : ED_C_DIM);
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
        EdAct_Undo();
        return;
    }
    // ajuda aberta: so TAB/ESC (fecham); o resto fica travado ate fechar
    if(EdHelpOpen()) {
        if(IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_ESCAPE))
            EdHelpToggle();
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
    if(activeDrop)   // lista aberta: o teclado e da pesquisa, sem atalhos
        return;
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
        Edit_StartTest(PLAYSTATE_TEST_FULL);
        return;
    }
    if(IsKeyPressed(KEY_F6)) {
        Edit_StartTest(PLAYSTATE_TEST_RECEPTORS);
        return;
    }
    if(IsKeyPressed(KEY_TAB)) {
        EdHelpToggle();
        return;
    }
    if(IsKeyPressed(KEY_ESCAPE)) {
        Editor_RequestLeave();
        return;
    }
    if(IsKeyPressed(KEY_Q)) EdAct_ToggleTurn();   // TAB virou atalhos; a troca de quem canta fica no Q
    else if(IsKeyPressed(KEY_T)) EdAct_CycleSnap();
    else if(IsKeyPressed(KEY_N)) EdAct_NewPart();
    else if(IsKeyPressed(KEY_X)) EdAct_DeletePart();
    else if(IsKeyPressed(KEY_C)) EdAct_CopyPart();
    else if(IsKeyPressed(KEY_V)) EdAct_PastePart();
    else if(IsKeyPressed(KEY_G)) EdAct_GoToPart(secIdx - 1);
    else if(IsKeyPressed(KEY_H)) EdAct_GoToPart(secIdx + 1);
    else if(IsKeyPressed(KEY_HOME)) cursorMs = SecStartMs(secIdx);
    else if(IsKeyPressed(KEY_COMMA)) EdAct_NudgeBpm(shift ? -10.0f : -1.0f);
    else if(IsKeyPressed(KEY_PERIOD)) EdAct_NudgeBpm(shift ? 10.0f : 1.0f);
}

static void Edit_Update(void) {
    if(ceDlg != DLG_NONE)
        return;
    // previa da skin segue a caixa Note Skin (so reaplica se mudou)
    {
        static char skinApplied[SONG_NAME_MAX];
        if(strcmp(skinApplied, skinBox.buf) != 0) {
            snprintf(skinApplied, sizeof(skinApplied), "%s", skinBox.buf);
            Editor_ApplyNoteSkin(skinApplied, chart.dir);
        }
    }
    // volume global (+/-) vale pro LISTEN tambem (so reaplica se mudou)
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
        // o raylib reporta a posicao antiga logo apos SeekMusicStream (buffer ainda nao trocou);
        // so aceita o relogio da musica se ele estiver perto do cursor, senao segue o dt
        double clkMs = -1.0;
        if(edHasAudio)
            clkMs = GetMusicTimePlayed(edInst) * 1000.0;
        else if(edHasVoices)
            clkMs = GetMusicTimePlayed(edVoices) * 1000.0;
        else if(edHasSplit)
            clkMs = GetMusicTimePlayed(edSplitO) * 1000.0;
        const double stepMs = RayGame_DeltaTime() * 1000.0;
        if(clkMs >= 0.0 && (clkMs - cursorMs < 300.0) && (cursorMs - clkMs < 300.0))
            cursorMs = clkMs;
        else
            cursorMs += stepMs;
        if(cursorMs >= SongEndMs() + 2000)
            Audio_StopPlay();
        else if(playFollow)
            viewTopMs = cursorMs - VIEW_MS * 0.25; // trava no receptor, igual ao jogo
    }

    // setas e W/S sobem e descem a grade (nao enquanto digita numa caixa nem no CTRL+S)
    const char typing = focusBox != NULL || focusNum != NULL || activeDrop;
    const char ctrlDown = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    if(!typing && !ctrlDown) {
        if(IsKeyDown(KEY_UP) || IsKeyDown(KEY_W))
            viewTopMs -= 2600 * RayGame_DeltaTime();
        if(IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S))
            viewTopMs += 2600 * RayGame_DeltaTime();
    }
    if(viewTopMs < -500) viewTopMs = -500;
    {
        double maxTop = SongEndMs() + 1000;
        if(viewTopMs > maxTop) viewTopMs = maxTop;
    }

    // roda do mouse = sobe/desce
    float wheel = GetMouseWheelMove();
    if(wheel != 0 && !Edit_EventPickerWheel(wheel) && !Edit_EventListWheel(wheel)) {
        Vector2 m = EdMouseWorld();
        int shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        int ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        const int lane = EdGrid_LaneAt(m);
        if((shift || ctrl) && lane >= 0) {
            // na nota embaixo do mouse: muda a nota longa (passo de 1/4 de tempo)
            const double t = fmax(EdGrid_TimeAt(m.y), 0.0);
            const int si = SecAtTime(t);
            EdSec* s = chart.secs + si;
            const float beatMs = 60000.0f / s->bpm;
            const int idx = EdSec_FindNear(s, (float)t, EdDispLane(s, lane), beatMs / 2);
            if(idx >= 0) {
                EdTouchSec(si);
                EdSec_SetLen(s, idx, s->notes[idx].len + wheel * beatMs / 4);
            }
        } else {
            viewTopMs -= wheel * 150;
            if(viewTopMs < -500) viewTopMs = -500;
        }
    }
}

/* Barra de status (rodape): trecho, grade, bpm e turno a esquerda; mensagem no meio; tempo a direita. */
static void Edit_DrawStatusBar(void) {
    const float y = 694;
    DrawRectangleRec((Rectangle){0, y, 1280, 720 - y}, ED_C_BAR);
    DrawRectangleRec((Rectangle){0, y, 1280, 1}, ED_C_EDGE);

    const EdSec* s = chart.secs + secIdx;
    char buf[128];
    snprintf(buf, sizeof(buf), "part %d/%d   snap 1/%d   %.0f bpm   %s",
        secIdx + 1, chart.nsec, snapDiv * 4, (double)s->bpm, s->mustHit ? "turn: you" : "turn: opponent");
    DrawTextEx(mainFont, buf, (Vector2){14, y + 5}, 18, 1, ED_C_DIM);

    if(statusTimer > 0)
        DrawTextEx(mainFont, statusText, (Vector2){PANEL_X, y + 4}, 20, 1, ED_C_HINT);
    else
        DrawTextEx(mainFont, "TAB = shortcuts", (Vector2){PANEL_X, y + 5}, 18, 1, ED_C_DIM);

    char now[16], end[16];
    EdTimeFmt(now, sizeof(now), cursorMs);
    EdTimeFmt(end, sizeof(end), SongEndMs());
    snprintf(buf, sizeof(buf), "%s / %s", now, end);
    DrawTextEx(mainFont, buf, (Vector2){1124, y + 4}, 20, 1, WHITE);
}


static void Edit_Draw(void) {
    EdArrows_Init();   // so faz algo na 1a vez (e depois de trocar a skin)
    Render_SetCamera(&camEd);
    Render_DrawGraphicObject(&bgEd);

    Edit_DrawGrid();
    Edit_DrawTabs();
    Edit_DrawPanel();
    Edit_DrawStatusBar();
    Edit_DrawMenuBar();
    CharDrop_Draw();
    Edit_DrawHelpOverlay();
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

    // EdArrows_Init() so quando entrar no modo edicao (o seletor nao usa a skin das notas)

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
    Edit_ShaderPreview();
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
        Ed_DrawText(mainFont, statusText, (Vector2) {150, 640}, 24, 2, ED_C_HINT);
}

static void ChartEditor_Destroy([[maybe_unused]] RayScene* scene) {
    ShaderFx_Reset(); // o preview nao pode vazar para os outros menus
    UnloadTexture(bgEd.image);
    SecTimes_Drop();
    Picker_Free();
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
    EdArrows_Free();   // atlas das cores e shader de matiz sao recursos da GPU
    EdChart_Free(&chart);
    chartLoaded = 0;
    Undo_Clear();
    if(copyValid) {
        free(copyBuf.notes);
        memset(&copyBuf, 0, sizeof(copyBuf));
        copyValid = 0;
    }
    focusBox = NULL;
    if(EdHelpOpen())
        EdHelpToggle();
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
