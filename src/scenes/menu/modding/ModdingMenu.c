/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * ModdingMenu.c - Modding hub e cena. Update trata entrada e estado; o desenho e o import
 * ficam em ModdingMenuDraw.c e ModdingMenuInput.c.
 */

#include "scenes/modding/charactereditor/CharEditor.h"
#include "scenes/modding/charteditor/ChartEditor.h"
#include "scenes/modding/effectseditor/EffectsEditor.h"
#include "scenes/modding/notesplashseditor/NotesplashEditor.h"
#include "scenes/modding/stageeditor/StageEditor.h"
#include "scenes/modding/weekeditor/WeekEditor.h"
#include "scenes/menu/MenuBackground.h"
#include "scenes/menu/MenuState.h"
#include "scenes/menu/modding/ModdingMenu.h"
#include "scenes/menu/modding/ModdingMenuInternal.h"
#include "data/modimport/ModImport.h"

#include <stdio.h>
#include "Game.h"

/* ------------------------------------------------------------------------- */
/* State                                                                     */
/* ------------------------------------------------------------------------- */

const char *const tabs[TAB_COUNT] = {
    "Chart Editor",
    "Character Editor",
    "Stage Editor",
    "Week Editor",
    "Effects Editor",
    "Notesplash Editor",
    "Import Mod"
};

ModdingMode mode;
int selected;
float offset;
const char *stubTitle;

RayGraphicObject bg;
Camera2D cam;

char impPath[MODDING_PATH_CAP];
char impMsg[MODDING_MSG_CAP];
int impMods;
int impSongs;
long long impKB;
char impConfirmDel;
ModImportStats impLast;
int impLastDup;
char impLastValid;
ModImportProgress impProg;

Color outlineColor = {0, 0, 0, 0};
Color fontColor = {255, 255, 255, 255};
const Color selectedColor = {42, 209, 86, 255};

/* ------------------------------------------------------------------------- */
/* Update                                                                    */
/* ------------------------------------------------------------------------- */

static void ModdingMenu_Update([[maybe_unused]] RayScene *scene)
{
    if (mode == MOD_IMPORT) {
        Imp_PollUsage();

        /* import em andamento: sem digitar; ESC cancela */
        if (ModImport_Busy()) {
            char ok = 0;
            ModImportStats done = {0};

            while (GetCharPressed() > 0) {}
            if (IsKeyPressed(KEY_ESCAPE)) ModImport_Cancel();
            if (ModImport_Poll(&impProg, impMsg, sizeof(impMsg), &done, &ok)) {
                impMsg[sizeof(impMsg) - 1] = '\0';
                if (ok || done.songs || done.stages || done.images || done.characters) {
                    impLast = done;
                    impLastDup = impProg.duplicates;
                    impLastValid = 1;
                }
                Imp_RefreshUsage();
            }
            return;
        }

        Imp_UpdateInput();

        if (IsKeyPressed(KEY_ESCAPE)) {
            mode = MOD_LIST;
            impConfirmDel = 0;
            return;
        }

        if (IsKeyPressed(KEY_DELETE) && impPath[0] == '\0') {
            if (!impConfirmDel && impMods > 0) {
                impConfirmDel = 1;
                snprintf(impMsg, sizeof(impMsg),
                         "DELETE again to remove %d mods (%lld KB). ESC cancels.",
                         impMods, impKB);
            } else if (impConfirmDel) {
                int removed = 0;
                long long freed = 0;

                ModImport_DeleteAll("assets/mods", impMsg, sizeof(impMsg),
                                    &removed, &freed);
                impMsg[sizeof(impMsg) - 1] = '\0';
                impConfirmDel = 0;
                Imp_RefreshUsage();
            }
            return;
        }

        if (impConfirmDel &&
            (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))) {
            impConfirmDel = 0;
            return;
        }

        if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) &&
            impPath[0] != '\0') {
            Imp_RunImport();
            return;
        }

        return;
    }

    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) {
        if (mode == MOD_STUB) {
            mode = MOD_LIST;
            return;
        }

        RayGame_ResetMusic();
        RayGame_ToggleMusic(1);
        MenuState_SetScene();
        return;
    }

    if (mode == MOD_STUB)
        return;

    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        if (++selected >= TAB_COUNT)
            selected = 0;
    } else if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        if (--selected < 0)
            selected = TAB_COUNT - 1;
    }

    /* Roda do mouse: acumula o movimento (touchpad manda fracoes) e troca de aba a cada clique. */
    {
        static float wheelAcc;
        wheelAcc += GetMouseWheelMove();
        while (wheelAcc >= 1.0f) {
            wheelAcc -= 1.0f;
            selected = (selected + TAB_COUNT - 1) % TAB_COUNT;
        }
        while (wheelAcc <= -1.0f) {
            wheelAcc += 1.0f;
            selected = (selected + 1) % TAB_COUNT;
        }
    }

    /* Mouse: mover sobre uma aba seleciona; clique esquerdo seleciona e abre (igual ao Enter). */
    int clicked = 0;
    {
        const Vector2 m = RayGame_MouseWorld();
        const Vector2 delta = GetMouseDelta();
        int hovered = -1;

        for (int i = 0; i < TAB_COUNT && hovered < 0; ++i) {
            if (Modding_TabHit(i, m))
                hovered = i;
        }

        if (hovered >= 0) {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                selected = hovered;
                clicked = 1;
            } else if (delta.x != 0.0f || delta.y != 0.0f) {
                selected = hovered;
            }
        }
    }

    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) ||
        IsKeyPressed(KEY_KP_ENTER) || clicked) {
        switch (selected) {
            case 0:
                ChartEditor_SetScene();
                return;

            case 1:
                CharEditor_SetScene();
                return;

            case 2:
                StageEditor_SetScene();
                return;

            case 3:
                WeekEditor_SetScene();
                return;

            case 4:
                EffectsEditor_SetScene();
                return;

            case 5:
                NotesplashEditor_SetScene();
                return;

            case 6:
                mode = MOD_IMPORT;
                impConfirmDel = 0;
                Imp_RefreshUsage();
                return;

            default:
                break;
        }
    }

    /* Clamp interpolation to avoid overshoot after a long frame. */
    {
        float factor = RayGame_DeltaTime() * 10.0f;
        if (factor > 1.0f)
            factor = 1.0f;
        if (factor < 0.0f)
            factor = 0.0f;

        offset += ((float)selected - offset) * factor;
    }
}

/* ------------------------------------------------------------------------- */
/* Draw                                                                      */
/* ------------------------------------------------------------------------- */

static void ModdingMenu_Draw([[maybe_unused]] RayScene *scene)
{
    ClearBackground(BLACK);

    switch (mode) {
        case MOD_IMPORT:
            Modding_DrawImport();
            break;

        case MOD_STUB:
            Modding_DrawStub();
            break;

        case MOD_LIST:
        default:
            Modding_DrawList();
            break;
    }
}

/* ------------------------------------------------------------------------- */
/* Scene lifecycle                                                           */
/* ------------------------------------------------------------------------- */

static void ModdingMenu_Create([[maybe_unused]] RayScene *scene)
{
    Render_DefaultRGT(&bg);
    bg.objType = RGT_IMAGE;
    bg.image = MenuBackground_Get();

    cam = (Camera2D){
        .target = {640.0f, 360.0f},
        .offset = {0.0f, 0.0f},
        .rotation = 0.0f,
        .zoom = 1.0f
    };

    mode = MOD_LIST;
    selected = 0;
    offset = 0.0f;
    stubTitle = NULL;

    impPath[0] = '\0';
    impMsg[0] = '\0';
    impMods = 0;
    impSongs = 0;
    impKB = 0;
    impConfirmDel = 0;
}

static void ModdingMenu_Destroy([[maybe_unused]] RayScene *scene)
{
    bg.image = (Texture2D){0}; // compartilhada: nao descarrega aqui
}

static RayScene moddingScene;
Scene_MakeSceneCode(
    moddingScene,
    ModdingMenu_SetScene,
    ModdingMenu_Create,
    ModdingMenu_Update,
    ModdingMenu_Draw,
    ModdingMenu_Destroy
)
