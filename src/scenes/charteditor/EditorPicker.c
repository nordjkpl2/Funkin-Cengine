/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: lista de musicas, abrir/salvar/importar e o estado da sessao de edicao. */
#include"scenes/AllScenes.h"
#include"scenes/Song.h"
#include"scenes/SongList.h"
#include "scenes/ModPath.h"
#include "scenes/Character.h"
#include"scenes/Stage.h"
#include"scenes/Note.h"
#include "Game.h"
#include "GameData.h"
#include "raymath.h"
#include "EditorDataAndIO.h"
#include "EditorTimeline.h"
#include "EditorAudio.h"
#include "EditorWidgets.h"
#include "EditorGrid.h"
#include "EditorPicker.h"
#include "EditorCharactersAndLists.h"
#include "EditorIcons.h"

const char* ChartTab_Name(int t) {
    switch(t) {
        case TAB_CHARTING: return "Charting";
        case TAB_DATA: return "Data";
        case TAB_EVENTS: return "Events";
        case TAB_NOTE: return "Note";
        case TAB_SECTION: return "Section";
        case TAB_SONG: return "Song";
        default: return "?";
    }
}

CeMode ceMode;

int activeTab = TAB_SECTION;

static int pickerSel = 0; // 0 = open file, 1..N = songs

static float pickerOffset = 0;

TxtBox openBox;

TxtBox saveAsBox;

char pendingDir[256]; // entrar direto no editor (volta do teste)

EdSec copyBuf;

char copyValid = 0;

RayGraphicObject bgEd;

void Picker_Refresh(void) {
    SongList_Free(pickerSongs);
    pickerSongs = NULL;
    pickerCount = SongList_Scan(&pickerSongs);
    pickerSel = 0;
    pickerOffset = 0;
}

void Editor_EnterDir(const char* dir) {
    if(!EdChart_OpenDir(&chart, dir)) {
        EdStatusMsg("NAO ABRIU ESSA MUSICA");
        return;
    }
    chartLoaded = 1;
    chart.dirty = 0;
    Undo_Clear();
    if(copyValid) {
        free(copyBuf.notes);
        copyValid = 0;
    }
    secIdx = 0;
    cursorMs = 0;
    viewTopMs = 0;
    playing = 0;
    snapDiv = 4;
    strncpy(p1Box.buf, chart.p1, sizeof(p1Box.buf) - 1);
    strncpy(p2Box.buf, chart.p2, sizeof(p2Box.buf) - 1);
    strncpy(gfBox.buf, chart.gf, sizeof(gfBox.buf) - 1);
    strncpy(stageBox.buf, chart.stage, sizeof(stageBox.buf) - 1);
    strncpy(skinBox.buf, chart.noteSkin, sizeof(skinBox.buf) - 1);
    CharList_Scan();
    StageList_Scan();
    NoteSkinList_Scan();
    activeDrop = 0;
    menuOpen = 0;
    activeTab = TAB_SECTION;
    Editor_LoadIcons();
    // audio pra ouvir (se nao tiver, anda no silencio)
    // voz: Voices.ogg, ou o par separado Voices-Opponent/Voices-Player
    char p[300];
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
    FILE* f;
    snprintf(p, sizeof(p), "%s/Inst.ogg", dir);
    f = fopen(p, "rb");
    if(f != NULL) {
        fclose(f);
        edInst = LoadMusicStream(p);
        SetMusicVolume(edInst, RayGame_MusicVolumeLevel());
        edHasAudio = 1;
    }
    snprintf(p, sizeof(p), "%s/Voices.ogg", dir);
    f = fopen(p, "rb");
    if(f != NULL) {
        fclose(f);
        edVoices = LoadMusicStream(p);
        SetMusicVolume(edVoices, RayGame_MusicVolumeLevel());
        edHasVoices = 1;
    } else {
        char po[300], pp[300];
        snprintf(po, sizeof(po), "%s/Voices-Opponent.ogg", dir);
        snprintf(pp, sizeof(pp), "%s/Voices-Player.ogg", dir);
        FILE* fo = fopen(po, "rb");
        FILE* fp = fopen(pp, "rb");
        if(fo != NULL && fp != NULL) {
            fclose(fo);
            fclose(fp);
            edSplitO = LoadMusicStream(po);
            edSplitP = LoadMusicStream(pp);
            SetMusicVolume(edSplitO, RayGame_MusicVolumeLevel());
            SetMusicVolume(edSplitP, RayGame_MusicVolumeLevel());
            edHasSplit = 1;
        } else {
            if(fo != NULL) fclose(fo);
            if(fp != NULL) fclose(fp);
        }
    }
    ceMode = CE_EDIT;
    ceDlg = DLG_NONE;
    focusBox = NULL;

    // set custom note skin for editor preview
    if(chart.noteSkin[0] != 0) {
        RayAnimationHandler skin = Cache_GetNoteSkin(chart.noteSkin, dir);
        Note_SetCurrentSkin(&skin);
    } else {
        Note_SetCurrentSkin(NULL);
    }

    EdStatusMsg("opened :)");
}

void Editor_Save(void) {
    strncpy(chart.p1, (p1Box.buf[0] != 0) ? p1Box.buf : "bf", sizeof(chart.p1) - 1);
    // vazio = sem oponente (tutorial): preserva "" no save (sem virar "dad")
    if(p2Box.buf[0] == 0) {
        strncpy(chart.p2, "", sizeof(chart.p2) - 1);
    } else {
        strncpy(chart.p2, p2Box.buf, sizeof(chart.p2) - 1);
    }
    chart.p2[sizeof(chart.p2) - 1] = 0;
    strncpy(chart.stage, (stageBox.buf[0] != 0) ? stageBox.buf : "stage", sizeof(chart.stage) - 1);
    chart.p1[sizeof(chart.p1) - 1] = 0;
    chart.stage[sizeof(chart.stage) - 1] = 0;
    // sem fantasma no save: so entra quem tem png+animset (mod ou global), senao bf/dad/stage
    // p2 vazio e valido (tutorial, so bf) e nao cai na validacao de asset
    if(!CharacterAssetForSong_Exists(chart.p1, chart.dir)) {
        strncpy(chart.p1, "bf", sizeof(chart.p1) - 1);
        strncpy(p1Box.buf, "bf", sizeof(p1Box.buf) - 1);
    }
    if(chart.p2[0] != 0 && !CharacterAssetForSong_Exists(chart.p2, chart.dir)) {
        strncpy(chart.p2, "dad", sizeof(chart.p2) - 1);
        strncpy(p2Box.buf, "dad", sizeof(p2Box.buf) - 1);
    }
    // girlfriend vazia ou inexistente volta para "gf"
    strncpy(chart.gf, (gfBox.buf[0] != 0) ? gfBox.buf : "gf", sizeof(chart.gf) - 1);
    chart.gf[sizeof(chart.gf) - 1] = 0;
    if(!GirlfriendAsset_Exists(chart.gf, chart.dir)) {
        strncpy(chart.gf, "gf", sizeof(chart.gf) - 1);
        strncpy(gfBox.buf, "gf", sizeof(gfBox.buf) - 1);
    }
    if(!StageAsset_Exists(chart.stage) && !StageFileForSong_Exists(chart.stage, chart.dir)) {
        strncpy(chart.stage, "stage", sizeof(chart.stage) - 1);
        strncpy(stageBox.buf, "stage", sizeof(stageBox.buf) - 1);
    }
    strncpy(chart.noteSkin, skinBox.buf, sizeof(chart.noteSkin) - 1);
    chart.noteSkin[sizeof(chart.noteSkin) - 1] = 0;
    if(EdChart_Save(&chart)) {
        chart.dirty = 0;
        Editor_LoadIcons();
        EdStatusMsg("SALVO!");
    } else {
        EdStatusMsg("ERRO AO SALVAR");
    }
}

// importa json solto -> vira pasta de mod -> abre (1 = ok)
char ImportJSON(const char* jsonPath, const char* modName) {
    char clean[64];
    EdSanitizeName(clean, sizeof(clean), modName);
    if(clean[0] == 0) {
        EdStatusMsg("NOME INVALIDO");
        return 0;
    }
    char dir[256];
    snprintf(dir, sizeof(dir), "assets/mods/%s", clean);
    if(Song_HasChart(dir)) {
        EdStatusMsg("JA EXISTE ESSE NOME");
        return 0;
    }
    MakeDirectory(dir);
    char dst[300];
    snprintf(dst, sizeof(dst), "%s/data.json", dir);
    if(!EdCopyFile(jsonPath, dst)) {
        EdStatusMsg("NAO DEU PRA COPIAR");
        return 0;
    }
    Editor_EnterDir(dir);
    return 1;
}

// SALVAR COMO: copia audio + salva json com outro nome (1 = ok)
char Editor_SaveAs(const char* modName) {
    char clean[64];
    EdSanitizeName(clean, sizeof(clean), modName);
    if(clean[0] == 0) {
        EdStatusMsg("NOME INVALIDO");
        return 0;
    }
    char dir[256];
    snprintf(dir, sizeof(dir), "assets/mods/%s", clean);
    if(strcmp(dir, chart.dir) == 0) {
        Editor_Save();
        return 1;
    }
    if(Song_HasChart(dir)) {
        EdStatusMsg("JA EXISTE ESSE NOME");
        return 0;
    }
    // mesma musica, outro nome: RENOMEIA a pasta ( so vale pros mods )
    if(strncmp(chart.dir, "assets/mods/", 12) == 0 && Song_HasChart(chart.dir)) {
        if(rename(chart.dir, dir) == 0) {
            strncpy(chart.dir, dir, sizeof(chart.dir) - 1);
            chart.dir[sizeof(chart.dir) - 1] = 0;
            strncpy(chart.name, clean, sizeof(chart.name) - 1);
            chart.name[sizeof(chart.name) - 1] = 0;
            Editor_Save();
            Picker_Refresh();
            EdStatusMsg("PASTA RENOMEADA");
            return 1;
        }
        // rename falhou: cai pro copia normal abaixo
    }
    MakeDirectory(dir);
    char src[300], dst[300];
    snprintf(src, sizeof(src), "%s/Inst.ogg", chart.dir);
    snprintf(dst, sizeof(dst), "%s/Inst.ogg", dir);
    EdCopyFile(src, dst);
    snprintf(src, sizeof(src), "%s/Voices.ogg", chart.dir);
    snprintf(dst, sizeof(dst), "%s/Voices.ogg", dir);
    EdCopyFile(src, dst);
    snprintf(src, sizeof(src), "%s/Voices-Opponent.ogg", chart.dir);
    snprintf(dst, sizeof(dst), "%s/Voices-Opponent.ogg", dir);
    EdCopyFile(src, dst);
    snprintf(src, sizeof(src), "%s/Voices-Player.ogg", chart.dir);
    snprintf(dst, sizeof(dst), "%s/Voices-Player.ogg", dir);
    EdCopyFile(src, dst);
    strncpy(chart.dir, dir, sizeof(chart.dir) - 1);
    strncpy(chart.name, clean, sizeof(chart.name) - 1);
    Editor_Save();
    Picker_Refresh();
    return 1;
}

void CharDrop_Draw(void) {
    if(!activeDrop || dropBox == NULL || dropNames == NULL)
        return;
    float bx = dropX, by = dropY, bw = dropW;
    TxtBox* box = dropBox;
    int n = dropCount;
    float ih = 30;
    int rows = n < DROP_ROWS_MAX ? n : DROP_ROWS_MAX;
    float h = (rows > 0 ? rows : 1) * ih + 8;
    if(by + h > 700) by = 700 - h;
    Rectangle r = {bx - bw + 30, by, bw, h};
    if(r.x + r.width > 1270) r.x = 1270 - r.width;
    DrawRectangleRec(r, (Color) {15, 15, 15, 245});
    DrawRectangleLinesEx(r, 1, GREEN);
    Vector2 m = EdMouseWorld();
    const char inside = (m.x >= r.x && m.x <= r.x + r.width && m.y >= r.y && m.y <= r.y + r.height);

    // roda: rola a lista (so com o mouse em cima dela)
    if(dropJustOpened) dropScroll = 0;
    if(inside && n > rows) {
        dropScroll -= (int)GetMouseWheelMove();
        if(dropScroll > n - rows) dropScroll = n - rows;
        if(dropScroll < 0) dropScroll = 0;
    }

    if(n == 0) {
        DrawTextEx(mainFont, "(vazio)", (Vector2) {r.x + 8, r.y + 8}, 20, 1, YELLOW);
    }
    // o clique que abriu a lista nao pode escolher o item que ficou embaixo do mouse
    const char canPick = !dropJustOpened;
    for(int row = 0; row < rows; row++) {
        const int i = dropScroll + row;
        Rectangle ir = {r.x + 4, r.y + 4 + row * ih, r.width - 14, ih - 2};
        char hov = (m.x >= ir.x && m.x <= ir.x + ir.width && m.y >= ir.y && m.y <= ir.y + ir.height);
        if(hov)
            DrawRectangleRec(ir, (Color) {50, 90, 50, 255});
        DrawTextEx(mainFont, dropNames[i], (Vector2) {ir.x + 6, ir.y + 4}, 20, 1, WHITE);
        if(canPick && hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            strncpy(box->buf, dropNames[i], sizeof(box->buf) - 1);
            box->buf[sizeof(box->buf) - 1] = 0;
            chart.dirty = 1;
            if(activeDrop == DROP_BOYFRIEND || activeDrop == DROP_OPPONENT) {
                // icones do topo da grade seguem o personagem escolhido
                strncpy(activeDrop == DROP_BOYFRIEND ? chart.p1 : chart.p2, box->buf, sizeof(chart.p1) - 1);
                Editor_LoadIcons();
            }
            activeDrop = 0;
            return;
        }
    }
    // barra de rolagem
    if(n > rows) {
        const float trackH = h - 8;
        const float thumbH = trackH * rows / n;
        const float thumbY = r.y + 4 + (trackH - thumbH) * dropScroll / (float)(n - rows);
        DrawRectangle((int)(r.x + r.width - 8), (int)thumbY, 4, (int)thumbH, GREEN);
    }
    // clicou fora: fecha (pula o frame que abriu)
    if(dropJustOpened) {
        dropJustOpened = 0;
    } else if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if(!inside) {
            activeDrop = DROP_NONE;
        }
    }
}

// ---------- PICKER ----------
char importPath[512];

static void Picker_OpenSelected(void) {
    if(pickerSel == 0) {
        openBox.buf[0] = 0;
        ceDlg = DLG_OPEN;
        focusBox = &openBox;
    } else if(pickerSel >= 1 && pickerSel <= pickerCount) {
        Editor_EnterDir(pickerSongs[pickerSel - 1].dir);
    }
}

static void Picker_NewSong(void) {
    EdChart_Default(&chart, "new-song");
    chart.dir[0] = 0;
    chartLoaded = 1;
    chart.dirty = 1;
    Undo_Clear();
    if(copyValid) { free(copyBuf.notes); memset(&copyBuf, 0, sizeof(copyBuf)); copyValid = 0; }
    secIdx = 0;
    cursorMs = 0;
    viewTopMs = 0;
    playing = 0;
    snapDiv = 4;
    strncpy(p1Box.buf, "bf", sizeof(p1Box.buf) - 1);
    strncpy(p2Box.buf, "dad", sizeof(p2Box.buf) - 1);
    strncpy(gfBox.buf, "gf", sizeof(gfBox.buf) - 1);
    strncpy(stageBox.buf, "stage", sizeof(stageBox.buf) - 1);
    CharList_Scan();
    StageList_Scan();
    activeDrop = 0;
    menuOpen = 0;
    activeTab = TAB_SONG;
    Editor_LoadIcons();
    if(edHasAudio) { UnloadMusicStream(edInst); edHasAudio = 0; }
    if(edHasVoices) { UnloadMusicStream(edVoices); edHasVoices = 0; }
    if(edHasSplit) { UnloadMusicStream(edSplitO); UnloadMusicStream(edSplitP); edHasSplit = 0; }
    ceMode = CE_EDIT;
    ceDlg = DLG_NONE;
    focusBox = NULL;
    focusNum = NULL;
    EdStatusMsg("NEW SONG - SALVAR COMO");
}

void Picker_Draw(void) {
    Render_SetCamera(&camEd);
    Render_DrawGraphicObject(&bgEd);

    DrawTextEx(mainFont, "song selector", (Vector2) {150, 60}, 48, 4, WHITE);

    int rows = 1 + pickerCount;
    for(int i = 0; i < rows; i++) {
        const int fontSize = (i == 0) ? 40 : 56;
        float y = 720 / 2 + ((float)i - pickerOffset) * (10 + fontSize);
        if(y < -60 || y > 780)
            continue;
        char name[128];
        if(i == 0) {
            strncpy(name, "open file directory...", sizeof(name) - 1);
            name[sizeof(name) - 1] = 0;
        } else {
            strncpy(name, pickerSongs[i - 1].name, sizeof(name) - 1);
            name[sizeof(name) - 1] = 0;
            if(strstr(pickerSongs[i - 1].dir, "mods") != NULL)
                strncat(name, "  [mod]", sizeof(name) - strlen(name) - 1);
        }
        float d = fabsf((float)i - pickerOffset);
        Vector2 pos = {(float)(5 + 150 + d * -30), y + 5};
        Color fill = WHITE, out = BLACK;
        fill.a = (pickerSel == i) ? 255 : 150;
        out.a = (pickerSel == i) ? 255 : 100;
        if(i == 0) fill = (pickerSel == 0) ? GREEN : (Color) {150, 255, 150, 150};
        DrawTextEx(mainFont, name, pos, fontSize, 4, out);
        pos.x -= 5;
        pos.y -= 5;
        DrawTextEx(mainFont, name, pos, fontSize, 4, fill);
    }

    DrawTextEx(mainFont, "ENTER open   R update   N new   arraste um .json pra ca   ESC return",
        (Vector2) {150, 690}, 20, 1, (Color) {200, 200, 200, 255});
    if(EdButton((Rectangle) {150, 630, 200, 40}, "OPEN", 22)) Picker_OpenSelected();
    if(EdButton((Rectangle) {360, 630, 200, 40}, "NEW", 22)) Picker_NewSong();
    Render_StopCamera();
}

void Picker_Update(void) {
    // arrastou arquivo pra janela
    if(IsFileDropped()) {
        FilePathList drop = LoadDroppedFiles();
        for(unsigned int i = 0; i < drop.count; i++) {
            const char* p = drop.paths[i];
            size_t L = strlen(p);
            if(L > 5 && (strcmp(p + L - 5, ".json") == 0 || strcmp(p + L - 5, ".JSON") == 0)) {
                EdChart tmp;
                memset(&tmp, 0, sizeof(tmp));
                if(EdChart_LoadJSON(&tmp, p, "", EdDirName(p))) {
                    EdChart_Free(&tmp);
                    strncpy(importPath, p, sizeof(importPath) - 1);
                    saveAsBox.buf[0] = 0;
                    ceDlg = DLG_SAVEAS;
                    focusBox = &saveAsBox;
                } else {
                    EdStatusMsg("ESSE JSON NAO E MUSICA");
                }
                break;
            }
        }
        UnloadDroppedFiles(drop);
    }

    if(ceDlg != DLG_NONE)
        return;

    int rows = 1 + pickerCount;
    if(IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        pickerSel++;
        if(pickerSel >= rows) pickerSel = 0;
    }
    if(IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        pickerSel--;
        if(pickerSel < 0) pickerSel = rows - 1;
    }
    if(IsKeyPressed(KEY_R))
        Picker_Refresh();
    if(IsKeyPressed(KEY_N))
        Picker_NewSong();
    if(IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) {
        RayGame_ResetMusic();
        RayGame_ToggleMusic(1);
        MenuState_SetScene();
        return;
    }
    if(IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
        Picker_OpenSelected();
        return;
    }
    // clique: 1o seleciona, 2o abre (OPEN ao lado da musica)
    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && ceDlg == DLG_NONE) {
        Vector2 m = EdMouseWorld();
        for(int i = 0; i < rows; i++) {
            int fs = (i == 0) ? 40 : 56;
            float y = 720 / 2 + ((float)i - pickerOffset) * (10 + fs);
            if(m.x >= 150 && m.x <= 1100 && m.y >= y - 5 && m.y <= y + fs) {
                if(pickerSel == i)
                    Picker_OpenSelected();
                else
                    pickerSel = i;
                break;
            }
        }
    }
    pickerOffset = Lerp(pickerOffset, (float)pickerSel, RayGame_DeltaTime() * 10.0f);
}

// volta pra lista de musicas, soltando audio e icones
void Editor_LeaveToPicker(void) {
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
    ceDlg = DLG_NONE;
    ceMode = CE_PICKER;
    Picker_Refresh();
}

// com alteracoes pendentes pergunta antes (salvar / descartar / cancelar)
void Editor_RequestLeave(void) {
    if(chart.dirty) {
        ceDlg = DLG_LEAVE;
        focusBox = NULL;
        focusNum = NULL;
    } else {
        Editor_LeaveToPicker();
    }
}
