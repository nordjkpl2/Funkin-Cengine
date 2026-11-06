/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * MenuState.c - menu principal: Story Mode, Freeplay, Options e Modding.
 * Os 3 primeiros sao botoes animados; "Modding" e um texto na parte de baixo.
 */
#include "scenes/AllScenes.h"
#include <math.h>

#define SCREEN_W 1280.0f
#define SCREEN_H 720.0f

#define BUTTON_COUNT 3          // botoes animados
#define MODDING_INDEX 3         // item de texto, depois dos botoes
#define BUTTON_FIRST_Y 200.0f
#define BUTTON_SPACING 175.0f
#define BUTTON_FPS 18

#define BG_SCALE 1.03f
#define BG_SHIFT_PER_ITEM 15.0f // o fundo desce um pouco a cada item
#define BG_LERP_SPEED 10.0f

#define MODDING_FONT_SIZE 40
#define MODDING_SPACING 4
#define MODDING_SHADOW 3
#define MODDING_Y 655.0f

#define VERSION_FONT_SIZE 18
#define VERSION_SHADOW 1.5f

typedef struct {
    RayAnimatedObject object;
    int idle, selected; // indices das animacoes
} MenuButton;

static RayGraphicObject bg;
static MenuButton buttons[BUTTON_COUNT];
static RayAnimationHandler buttonAnimations;
static int selectedItem; // mantido ao voltar pro menu, de proposito
static Camera2D cam;

static const Vector2 versionPos = {12.0f, 694.0f};
// # VERSION
static const char* const versionText = "v0030";
static const char* const moddingText = "Modding";
static Vector2 moddingPos; // calculado uma vez no Create

// Cores fixas (nao recria struct a cada frame).
static const Color MODDING_FILL_ON = {42, 209, 86, 255};
static const Color MODDING_FILL_OFF = {255, 255, 255, 150};
static const Color MODDING_SHADOW_OFF = {0, 0, 0, 100};
static const Color VERSION_FILL = {255, 255, 255, 180};

/* Centraliza o botao na horizontal; a altura depende da animacao atual. */
static void Button_Place(int i) {
    RayAnimatedObject* object = &buttons[i].object;
    const Vector2 size = AnimatedObject_Sizes(object);

    object->position = (Vector2) {
        .x = SCREEN_W / 2 - size.x / 2,
        .y = BUTTON_FIRST_Y + i * BUTTON_SPACING - size.y / 2
    };
}

static void Button_SetSelected(int i, char selected) {
    MenuButton* button = buttons + i;

    AnimatedObject_SetAnimation(&button->object, selected ? button->selected : button->idle);
    Button_Place(i);
}

static void Buttons_Load(void) {
    buttonAnimations = AnimationSet_LoadAnimations(
        "assets/images/menu/menuOptions.animset",
        Render_LoadTexture("assets/images/menu/menuOptions.png")
    );

    for (int i = 0; i < buttonAnimations.animationCount; i++)
        AnimationSet_SetAnimationData(&buttonAnimations, i, BUTTON_FPS, 1);

    // Os nomes vem do .animset (alguns tem espaco no fim de proposito).
    buttons[0] = (MenuButton) {
        .idle = AnimationSet_FindAnimation(&buttonAnimations, "story mode idle"),
        .selected = AnimationSet_FindAnimation(&buttonAnimations, "story mode selected")
    };
    buttons[1] = (MenuButton) {
        .idle = AnimationSet_FindAnimation(&buttonAnimations, "freeplay idle "),
        .selected = AnimationSet_FindAnimation(&buttonAnimations, "freeplay selected ")
    };
    buttons[2] = (MenuButton) {
        .idle = AnimationSet_FindAnimation(&buttonAnimations, "options idle"),
        .selected = AnimationSet_FindAnimation(&buttonAnimations, "options selected ")
    };

    for (int i = 0; i < BUTTON_COUNT; i++) {
        Render_DefaultAnimated(&buttons[i].object);
        buttons[i].object.animationSet = buttonAnimations;
        Button_SetSelected(i, i == selectedItem);
    }
}

static void MenuState_Create([[maybe_unused]] RayScene* scene) {
    cam.target = (Vector2) {SCREEN_W / 2, SCREEN_H / 2};
    cam.zoom = 1;

    Render_DefaultRGT(&bg);
    bg.objType = RGT_IMAGE;
    bg.image = Render_LoadTexture("assets/images/menu/menuBG.png");
    bg.scaleX = bg.scaleY = BG_SCALE;
    bg.position.x = SCREEN_W / 2 - GraphicObject_Sizes(&bg).x / 2;

    Buttons_Load();

    const Vector2 size = MeasureTextEx(mainFont, moddingText, MODDING_FONT_SIZE, MODDING_SPACING);
    moddingPos = (Vector2) {(SCREEN_W - size.x) / 2.0f, MODDING_Y};
}

/* Abre a cena do item escolhido. */
static void OpenSelected(void) {
    switch (selectedItem) {
        case 0: StoryMode_SetScene(); break;
        case 1: Freeplay_SetScene(); break;
        case 2: OptionsMenu_SetScene(); break;
        case MODDING_INDEX:
            RayGame_ToggleMusic(0);
            ModdingMenu_SetScene();
            break;
    }
}

static void MenuState_Update([[maybe_unused]] RayScene* scene) {
    int newItem = selectedItem;

    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        if (newItem > 0) newItem--;
    }
    else if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        if (newItem < MODDING_INDEX) newItem++;
    }
    else if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        OpenSelected();
        return;
    }

    if (newItem != selectedItem) {
        if (selectedItem < BUTTON_COUNT) Button_SetSelected(selectedItem, 0);
        if (newItem < BUTTON_COUNT) Button_SetSelected(newItem, 1);
        selectedItem = newItem;
    }

    // Fundo suave; encosta no alvo quando chega perto (para de recalcular).
    const float bgTarget = -selectedItem * BG_SHIFT_PER_ITEM;
    if (bg.position.y != bgTarget) {
        bg.position.y = Lerp(bg.position.y, bgTarget, RayGame_DeltaTime() * BG_LERP_SPEED);
        if (fabsf(bg.position.y - bgTarget) < 0.01f)
            bg.position.y = bgTarget;
    }

    for (int i = 0; i < BUTTON_COUNT; i++)
        AnimatedObject_UpdateFrame(&buttons[i].object);
}

/* Texto com sombra deslocada. */
static void DrawShadowedText(const char* text, Vector2 pos, int fontSize, float spacing,
                             float shadow, Color fill, Color outline) {
    DrawTextEx(mainFont, text, (Vector2) {pos.x + shadow, pos.y + shadow}, fontSize, spacing, outline);
    DrawTextEx(mainFont, text, pos, fontSize, spacing, fill);
}

static void MenuState_Draw([[maybe_unused]] RayScene* scene) {
    Render_SetCamera(&cam);

    ClearBackground(BLACK);
    Render_DrawGraphicObject(&bg);

    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
    for (int i = 0; i < BUTTON_COUNT; i++)
        Render_DrawAnimatedObject(&buttons[i].object);
    EndBlendMode();

    // Modding: verde quando selecionado, apagado quando nao.
    if (selectedItem == MODDING_INDEX)
        DrawShadowedText(moddingText, moddingPos, MODDING_FONT_SIZE, MODDING_SPACING, MODDING_SHADOW,
                         MODDING_FILL_ON, BLACK);
    else
        DrawShadowedText(moddingText, moddingPos, MODDING_FONT_SIZE, MODDING_SPACING, MODDING_SHADOW,
                         MODDING_FILL_OFF, MODDING_SHADOW_OFF);

    DrawShadowedText(versionText, versionPos, VERSION_FONT_SIZE, 1, VERSION_SHADOW,
                     VERSION_FILL, BLACK);

    Render_StopCamera();
}

static void MenuState_Destroy([[maybe_unused]] RayScene* scene) {
    AnimationSet_FreeAll(&buttonAnimations);
    UnloadTexture(bg.image);
}

static RayScene menuState;
Scene_MakeSceneCode(
    menuState,
    MenuState_SetScene,
    MenuState_Create,
    MenuState_Update,
    MenuState_Draw,
    MenuState_Destroy
)
