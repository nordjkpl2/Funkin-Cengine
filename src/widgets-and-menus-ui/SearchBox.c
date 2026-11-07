/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * SearchBox.c - campo de pesquisa com lupa (circulo + cabo desenhados, sem textura).
 * So trabalha quando ha entrada; o filtro compara texto curto, sem alocar.
 */

#include "widgets-and-menus-ui/SearchBox.h"
#include "Game.h"

#include <string.h>

#define BUTTON_RADIUS 22.0f
#define TEXT_SIZE 24.0f
#define TEXT_SPACING 2.0f

static const Color FIELD_BG = {14, 12, 30, 235};
static const Color FIELD_BORDER = {110, 105, 155, 220};
static const Color FIELD_BORDER_FOCUS = {255, 215, 90, 255};
static const Color FIELD_TEXT = {255, 255, 255, 255};
static const Color FIELD_HINT = {150, 145, 170, 255};
static const Color BUTTON_BG = {60, 52, 110, 255};
static const Color BUTTON_BG_HOVER = {96, 84, 170, 255};
static const Color BUTTON_ICON = {255, 255, 255, 255};

/* a-z minusculo; - _ espaco viram espaco. */
static char Fold(char c) {
    if (c >= 'A' && c <= 'Z') return (char)(c - 'A' + 'a');
    if (c == '-' || c == '_') return ' ';
    return c;
}

static Vector2 ButtonCenter(const SearchBox* box) {
    return (Vector2) {box->bounds.x + box->bounds.width - BUTTON_RADIUS,
                      box->bounds.y + box->bounds.height * 0.5f};
}

static Rectangle FieldRect(const SearchBox* box) {
    Rectangle field = box->bounds;
    field.width -= BUTTON_RADIUS * 2.0f + 8.0f;
    return field;
}

void SearchBox_Init(SearchBox* box, Rectangle bounds) {
    memset(box, 0, sizeof(*box));
    box->bounds = bounds;
}

static void Apply(SearchBox* box) {
    int n = 0;
    for (int i = 0; i < box->length && n < SEARCH_TEXT_MAX - 1; ++i)
        box->applied[n++] = Fold(box->text[i]);
    while (n > 0 && box->applied[n - 1] == ' ') --n;   /* sem espacos no fim */
    box->applied[n] = '\0';
}

bool SearchBox_Update(SearchBox* box) {
    bool apply = false;

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const Vector2 mouse = RayGame_MouseWorld();
        const Vector2 center = ButtonCenter(box);
        const float dx = mouse.x - center.x, dy = mouse.y - center.y;

        if (dx * dx + dy * dy <= BUTTON_RADIUS * BUTTON_RADIUS) {
            box->focused = false;
            apply = true;                                   /* clique na lupa */
        } else {
            box->focused = CheckCollisionPointRec(mouse, FieldRect(box));
        }
    }

    if (!box->focused) {
        if (apply) Apply(box);
        return apply;
    }

    int codepoint;
    while ((codepoint = GetCharPressed()) > 0) {
        if (codepoint >= 32 && codepoint <= 126 && box->length < SEARCH_TEXT_MAX - 1) {
            box->text[box->length++] = (char)codepoint;
            box->text[box->length] = '\0';
        }
    }

    if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && box->length > 0)
        box->text[--box->length] = '\0';

    if (IsKeyPressed(KEY_ESCAPE)) box->focused = false;

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        box->focused = false;
        apply = true;
    }

    if (apply) Apply(box);
    return apply;
}

bool SearchBox_Focused(const SearchBox* box) {
    return box->focused;
}

bool SearchBox_Match(const SearchBox* box, const char* name) {
    const char* needle = box->applied;
    if (needle[0] == '\0') return true;
    if (name == NULL) return false;

    for (const char* start = name; *start != '\0'; ++start) {
        const char* a = start;
        const char* b = needle;
        while (*b != '\0' && *a != '\0' && Fold(*a) == *b) { ++a; ++b; }
        if (*b == '\0') return true;
    }
    return false;
}

void SearchBox_Draw(const SearchBox* box, Font font) {
    const Rectangle field = FieldRect(box);
    DrawRectangleRec(field, FIELD_BG);
    DrawRectangleLinesEx(field, 2.0f, box->focused ? FIELD_BORDER_FOCUS : FIELD_BORDER);

    const float textY = field.y + (field.height - TEXT_SIZE) * 0.5f;
    if (box->length == 0) {
        DrawTextEx(font, "SEARCH", (Vector2) {field.x + 12.0f, textY}, TEXT_SIZE, TEXT_SPACING, FIELD_HINT);
    } else {
        DrawTextEx(font, box->text, (Vector2) {field.x + 12.0f, textY}, TEXT_SIZE, TEXT_SPACING, FIELD_TEXT);
    }

    if (box->focused && ((int)(GetTime() * 2.0) & 1) == 0) {
        const float textW = box->length > 0 ? MeasureTextEx(font, box->text, TEXT_SIZE, TEXT_SPACING).x : 0.0f;
        DrawRectangle((int)(field.x + 14.0f + textW), (int)(field.y + 8.0f), 2, (int)(field.height - 16.0f), FIELD_TEXT);
    }

    /* Lupa: botao redondo com um circulo (aro) e um cabo inclinado. */
    const Vector2 center = ButtonCenter(box);
    const Vector2 mouse = RayGame_MouseWorld();
    const float dx = mouse.x - center.x, dy = mouse.y - center.y;
    const bool hover = dx * dx + dy * dy <= BUTTON_RADIUS * BUTTON_RADIUS;

    DrawCircleV(center, BUTTON_RADIUS, hover ? BUTTON_BG_HOVER : BUTTON_BG);
    const Vector2 lens = {center.x - 3.0f, center.y - 3.0f};
    DrawRing(lens, 6.0f, 9.0f, 0.0f, 360.0f, 24, BUTTON_ICON);
    DrawLineEx((Vector2) {lens.x + 6.5f, lens.y + 6.5f}, (Vector2) {center.x + 11.0f, center.y + 11.0f},
               4.0f, BUTTON_ICON);
}
