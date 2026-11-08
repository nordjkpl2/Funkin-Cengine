/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * ModdingMenu.c - Modding hub and mod importer UI.
 * Input/state work is handled in Update; Draw only renders.
 */

#include "scenes/AllScenes.h"
#include "data/ModImport.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "Game.h"

/* ------------------------------------------------------------------------- */
/* State                                                                     */
/* ------------------------------------------------------------------------- */

typedef enum {
    MOD_LIST = 0,
    MOD_IMPORT,
    MOD_STUB
} ModdingMode;

static const char *const tabs[] = {
    "Chart Editor",
    "Character Editor",
    "Import Mod"
};

enum { TAB_COUNT = (int)(sizeof(tabs) / sizeof(tabs[0])) };

static ModdingMode mode;
static int selected;
static float offset;
static const char *stubTitle;

static RayGraphicObject bg;
static Camera2D cam;

/* Importer: origem do mod -> assets/mods/<nome>. */
#define MODDING_PATH_CAP 512
#define MODDING_MSG_CAP  256

static char impPath[MODDING_PATH_CAP];
static char impMsg[MODDING_MSG_CAP];
static int impMods;
static int impSongs;
static long long impKB;
static char impConfirmDel;
static ModImportStats impLast;
static int impLastDup;
static char impLastValid;
static ModImportProgress impProg;

static Color outlineColor = {0, 0, 0, 0};
static Color fontColor = {255, 255, 255, 255};
static const Color selectedColor = {42, 209, 86, 255};

/* ------------------------------------------------------------------------- */
/* Small utilities                                                           */
/* ------------------------------------------------------------------------- */

/* Varredura em thread de fundo: a tela nao trava; o resultado chega em Imp_PollUsage. */
static void Imp_RefreshUsage(void)
{
    ModImport_ScanStart("assets/mods");
}

static void Imp_PollUsage(void)
{
    ModImportUsage usage = {0};

    if (ModImport_ScanPoll(&usage)) {
        impMods = usage.mods;
        impSongs = usage.songs;
        impKB = usage.bytes / 1024;
    }
}

static void Imp_SetMessage(const char *message)
{
    if (!message) {
        impMsg[0] = '\0';
        return;
    }

    snprintf(impMsg, sizeof(impMsg), "%s", message);
}

/* Append an input codepoint as UTF-8. GetCharPressed returns Unicode codepoints. */
static void Imp_AppendCodepoint(int codepoint)
{
    size_t length = strlen(impPath);
    unsigned char bytes[4];
    size_t count;

    if (codepoint < 32 || codepoint == 127 || codepoint > 0x10FFFF ||
        (codepoint >= 0xD800 && codepoint <= 0xDFFF))
        return;

    if (codepoint <= 0x7F) {
        bytes[0] = (unsigned char)codepoint;
        count = 1;
    } else if (codepoint <= 0x7FF) {
        bytes[0] = (unsigned char)(0xC0 | (codepoint >> 6));
        bytes[1] = (unsigned char)(0x80 | (codepoint & 0x3F));
        count = 2;
    } else if (codepoint <= 0xFFFF) {
        bytes[0] = (unsigned char)(0xE0 | (codepoint >> 12));
        bytes[1] = (unsigned char)(0x80 | ((codepoint >> 6) & 0x3F));
        bytes[2] = (unsigned char)(0x80 | (codepoint & 0x3F));
        count = 3;
    } else {
        bytes[0] = (unsigned char)(0xF0 | (codepoint >> 18));
        bytes[1] = (unsigned char)(0x80 | ((codepoint >> 12) & 0x3F));
        bytes[2] = (unsigned char)(0x80 | ((codepoint >> 6) & 0x3F));
        bytes[3] = (unsigned char)(0x80 | (codepoint & 0x3F));
        count = 4;
    }

    if (length + count >= sizeof(impPath))
        return;

    memcpy(impPath + length, bytes, count);
    impPath[length + count] = '\0';
}

static void Imp_PasteClipboard(void)
{
    const char *clipboard = GetClipboardText();
    size_t length = strlen(impPath);

    if (!clipboard)
        return;

    while (*clipboard && length + 1 < sizeof(impPath)) {
        unsigned char c = (unsigned char)*clipboard++;

        /* Keep UTF-8 bytes; discard newlines and other control characters. */
        if (c < 32 || c == 127)
            continue;

        impPath[length++] = (char)c;
    }

    impPath[length] = '\0';
}

static void Imp_Backspace(void)
{
    size_t length = strlen(impPath);

    if (length == 0)
        return;

    /* Remove one UTF-8 codepoint, not just one trailing byte. */
    do {
        length--;
    } while (length > 0 && (((unsigned char)impPath[length] & 0xC0) == 0x80));

    impPath[length] = '\0';
}

/* Remove surrounding quotes and trailing separators; return the last folder. */
static const char *Imp_PrepareSourcePath(char path[MODDING_PATH_CAP])
{
    size_t length = strlen(path);
    char *base = path;

    while (length > 0 && (path[length - 1] == '\\' || path[length - 1] == '/'))
        path[--length] = '\0';

    if (length >= 2 && path[0] == '"' && path[length - 1] == '"') {
        memmove(path, path + 1, length - 2);
        length -= 2;
        path[length] = '\0';
    }

    while (length > 0 && (path[length - 1] == '\\' || path[length - 1] == '/'))
        path[--length] = '\0';

    if (length == 0)
        return NULL;

    for (char *p = path; *p; ++p) {
        if (*p == '/' || *p == '\\')
            base = p + 1;
    }

    return *base ? base : NULL;
}

static void Imp_RunImport(void)
{
    char sourcePath[MODDING_PATH_CAP];
    const char *modName;

    memcpy(sourcePath, impPath, sizeof(sourcePath));
    sourcePath[sizeof(sourcePath) - 1] = '\0';

    modName = Imp_PrepareSourcePath(sourcePath);
    if (!modName) {
        Imp_SetMessage("import: caminho invalido");
        return;
    }

    impConfirmDel = 0;
    impMsg[0] = '\0';
    memset(&impProg, 0, sizeof(impProg));
    if (!ModImport_Start(sourcePath, "assets/mods", modName))
        Imp_SetMessage("import: could not start");
}

static void Imp_UpdateInput(void)
{
    int codepoint;
    const int ctrlDown = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

    while ((codepoint = GetCharPressed()) > 0)
        Imp_AppendCodepoint(codepoint);

    if (IsKeyPressed(KEY_BACKSPACE))
        Imp_Backspace();

    if (ctrlDown && IsKeyPressed(KEY_V))
        Imp_PasteClipboard();
}

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

    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) ||
        IsKeyPressed(KEY_KP_ENTER)) {
        switch (selected) {
            case 0:
                ChartEditor_SetScene();
                return;

            case 1:
                stubTitle = tabs[selected];
                mode = MOD_STUB;
                return;

            case 2:
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

static void Modding_DrawBackground(void)
{
    Render_SetCamera(&cam);
    Render_DrawGraphicObject(&bg);
}

/* Icones desenhados mais abaixo. */
static void Modding_DrawMusicNote(float x, float y, float size, Color c);
static void Modding_DrawPerson(float x, float y, float size, Color c);
static void Modding_DrawDownload(float x, float y, float size, Color c);

static void Imp_FormatSize(long long bytes, char *out, size_t cap)
{
    const double b = (double)bytes;
    if (bytes >= (1LL << 30))      snprintf(out, cap, "%.2f GB", b / 1073741824.0);
    else if (bytes >= (1LL << 20)) snprintf(out, cap, "%.1f MB", b / 1048576.0);
    else if (bytes >= (1LL << 10)) snprintf(out, cap, "%.0f KB", b / 1024.0);
    else                           snprintf(out, cap, "%lld B", bytes);
}

/* Fim do texto que cabe em maxW (caminhos longos mostram o final). */
static const char *Imp_FitTail(const char *text, float maxW, float size)
{
    const char *p = text;
    while (*p && MeasureTextEx(mainFont, p, size, 1).x > maxW) {
        ++p;
        while ((*p & 0xC0) == 0x80) ++p;
    }
    return p;
}

/* Quebra por palavras; devolve o y logo abaixo do texto. */
static float Imp_DrawWrapped(const char *text, float x, float y, float maxW,
                             float size, Color c)
{
    char line[MODDING_MSG_CAP] = "";
    char test[MODDING_MSG_CAP];
    size_t len = 0;
    const char *p = text;

    while (*p) {
        const char *e = p;
        size_t wl, tl;
        while (*e && *e != ' ' && *e != '\n') ++e;
        wl = (size_t)(e - p);
        if (wl + len + 2 >= sizeof(test)) break;
        memcpy(test, line, len);
        tl = len;
        if (len) test[tl++] = ' ';
        memcpy(test + tl, p, wl);
        tl += wl;
        test[tl] = '\0';
        if (len && MeasureTextEx(mainFont, test, size, 1).x > maxW) {
            DrawTextEx(mainFont, line, (Vector2){x, y}, size, 1, c);
            y += size + 6;
            memcpy(line, p, wl);
            line[wl] = '\0';
            len = wl;
        } else {
            memcpy(line, test, tl + 1);
            len = tl;
        }
        p = e;
        while (*p == ' ' || *p == '\n') ++p;
    }
    if (len) {
        DrawTextEx(mainFont, line, (Vector2){x, y}, size, 1, c);
        y += size + 6;
    }
    return y;
}

static void Imp_Panel(Rectangle r, const char *title)
{
    DrawRectangleRec(r, (Color){10, 10, 16, 215});
    DrawRectangleLinesEx(r, 2, (Color){42, 209, 86, 150});
    DrawTextEx(mainFont, title, (Vector2){r.x + 20, r.y + 14}, 22, 2, selectedColor);
    DrawRectangle((int)r.x + 20, (int)r.y + 46, (int)r.width - 40, 1, (Color){255, 255, 255, 40});
}

/* Tecla + rotulo; devolve o x onde termina. */
static float Imp_Chip(float x, float y, const char *key, const char *label,
                      Color accent, char active)
{
    const float kw = MeasureTextEx(mainFont, key, 17, 1).x + 20;
    const Color text = active ? fontColor : (Color){130, 130, 140, 255};

    DrawRectangle((int)x, (int)y, (int)kw, 30, active ? (Color){accent.r, accent.g, accent.b, 70} : (Color){40, 40, 48, 255});
    DrawRectangleLinesEx((Rectangle){x, y, kw, 30}, 1, active ? accent : (Color){70, 70, 80, 255});
    DrawTextEx(mainFont, key, (Vector2){x + 10, y + 6}, 17, 1, text);
    DrawTextEx(mainFont, label, (Vector2){x + kw + 8, y + 5}, 19, 1, text);
    return x + kw + 8 + MeasureTextEx(mainFont, label, 19, 1).x + 22;
}

static void Imp_StatCard(float x, float y, int kind, const char *value, const char *label)
{
    const Rectangle card = {x, y, 410, 68};
    const Color icon = selectedColor;

    DrawRectangleRec(card, (Color){255, 255, 255, 14});
    DrawRectangle((int)x + 10, (int)y + 10, 48, 48, (Color){42, 209, 86, 45});
    if (kind == 0)      Modding_DrawPerson(x + 22, y + 18, 24, icon);
    else if (kind == 1) Modding_DrawMusicNote(x + 22, y + 20, 24, icon);
    else                Modding_DrawDownload(x + 22, y + 20, 24, icon);
    DrawTextEx(mainFont, value, (Vector2){x + 72, y + 6}, 30, 2, fontColor);
    DrawTextEx(mainFont, label, (Vector2){x + 72, y + 40}, 19, 1, (Color){150, 150, 160, 255});
}

static void Modding_DrawImport(void)
{
    const Color muted = {150, 150, 160, 255};
    const Color warning = impConfirmDel ? RED : YELLOW;
    const Rectangle left = {50, 130, 700, 430};
    const Rectangle right = {780, 130, 450, 430};
    const char busy = ModImport_ScanBusy();
    const char importing = ModImport_Busy();
    char a[96], b[96], msg[MODDING_MSG_CAP];
    float y, x;

    Modding_DrawBackground();
    DrawRectangle(0, 0, 1280, 720, (Color){0, 0, 0, 120});

    /* topo: seta de voltar + titulo */
    DrawTriangle((Vector2){76, 52}, (Vector2){76, 88}, (Vector2){50, 70}, fontColor);
    DrawRectangle(76, 64, 24, 12, fontColor);
    DrawTextEx(mainFont, "IMPORT MODS", (Vector2){120, 46}, 44, 4, fontColor);

    /* esquerda: origem */
    Imp_Panel(left, "SOURCE FOLDER");
    DrawTextEx(mainFont, "Directory containing songs/, data/ or weeks/",
               (Vector2){left.x + 20, left.y + 60}, 19, 1, muted);
    {
        const Rectangle box = {left.x + 20, left.y + 92, left.width - 40, 46};
        const float textW = box.width - 24;
        const char *shown = Imp_FitTail(impPath, textW - 24, 20);
        float tx = box.x + 12;

        DrawRectangleRec(box, (Color){20, 20, 20, 255});
        DrawRectangleLinesEx(box, 2, GREEN);
        if (impPath[0]) {
            if (shown != impPath) {
                DrawTextEx(mainFont, "...", (Vector2){tx, box.y + 12}, 20, 1, muted);
                tx += MeasureTextEx(mainFont, "...", 20, 1).x + 4;
            }
            DrawTextEx(mainFont, shown, (Vector2){tx, box.y + 12}, 20, 1, WHITE);
            tx += MeasureTextEx(mainFont, shown, 20, 1).x + 3;
        } else {
            DrawTextEx(mainFont, "C:/.../mod-folder", (Vector2){tx, box.y + 12}, 20, 1, (Color){110, 110, 120, 255});
        }
        if (((int)(GetTime() * 2.0)) & 1)
            DrawRectangle((int)tx, (int)box.y + 10, 2, 26, fontColor);
    }
    x = left.x + 20;
    y = left.y + 156;
    x = Imp_Chip(x, y, "ENTER", "Import", selectedColor, impPath[0] != '\0' && !importing);
    x = Imp_Chip(x, y, "CTRL+V", "Paste path", selectedColor, 1);
    (void)Imp_Chip(x, y, "BKSP", "Erase", selectedColor, impPath[0] != '\0');

    DrawRectangle((int)left.x + 20, (int)left.y + 206, (int)left.width - 40, 1, (Color){255, 255, 255, 30});
    y = Imp_DrawWrapped("Imports charts, music, weeks, stages, images, characters and icons.",
                        left.x + 20, left.y + 222, left.width - 40, 19, fontColor);
    y = Imp_DrawWrapped("Destination: assets/mods/<mod-name>/",
                        left.x + 20, y + 2, left.width - 40, 19, muted);

    if (importing) {
        const float top = left.y + 322;
        char c[96];
        Imp_FormatSize(impProg.bytesDone, c, sizeof(c));
        snprintf(msg, sizeof(msg), "%s... %d / %d files, %s copied%s",
                 impProg.cancelling ? "Cancelling" : "Importing",
                 impProg.filesDone, impProg.filesTotal, c,
                 impProg.failed ? " (some files failed)" : "");
        DrawRectangle((int)left.x + 20, (int)top, 4, 90, selectedColor);
        (void)Imp_DrawWrapped(msg, left.x + 36, top + 2, left.width - 60, 19, selectedColor);
    } else if (impMsg[0]) {
        const float top = left.y + 322;
        snprintf(msg, sizeof(msg), "%s", impMsg);
        DrawRectangle((int)left.x + 20, (int)top, 4, 90, warning);
        (void)Imp_DrawWrapped(msg, left.x + 36, top + 2, left.width - 60, 19, warning);
    }

    /* direita: dados que ja existem */
    Imp_Panel(right, "INSTALLED");
    if (busy) {
        snprintf(a, sizeof(a), "...");
        snprintf(b, sizeof(b), "...");
        snprintf(msg, sizeof(msg), "...");
    } else {
        snprintf(a, sizeof(a), "%d", impMods);
        snprintf(b, sizeof(b), "%d", impSongs);
        Imp_FormatSize(impKB * 1024, msg, sizeof(msg));
    }
    Imp_StatCard(right.x + 20, right.y + 60, 0, a, "Mods");
    Imp_StatCard(right.x + 20, right.y + 138, 1, b, "Songs");
    Imp_StatCard(right.x + 20, right.y + 216, 2, msg, "Disk usage");

    DrawTextEx(mainFont, "LAST IMPORT", (Vector2){right.x + 20, right.y + 298}, 19, 2, selectedColor);
    if (impLastValid) {
        char c[96];
        y = right.y + 326;
        snprintf(a, sizeof(a), "Songs %d   Charts %d   Weeks %d   Stages %d",
                 impLast.songs, impLast.charts, impLast.weeks, impLast.stages);
        snprintf(b, sizeof(b), "Images %d   Characters %d   Icons %d",
                 impLast.images, impLast.characters, impLast.icons);
        Imp_FormatSize(impLast.bytes, msg, sizeof(msg));
        snprintf(c, sizeof(c), "Copied %s, %d same files skipped", msg, impLastDup);
        DrawTextEx(mainFont, a, (Vector2){right.x + 20, y}, 19, 1, fontColor);
        DrawTextEx(mainFont, b, (Vector2){right.x + 20, y + 28}, 19, 1, fontColor);
        DrawTextEx(mainFont, c, (Vector2){right.x + 20, y + 62}, 19, 1, muted);
    } else {
        DrawTextEx(mainFont, "Nothing imported in this session.",
                   (Vector2){right.x + 20, right.y + 326}, 19, 1, muted);
    }

    /* rodape: barra de uso de disco (parte do ultimo import) + atalhos */
    Imp_FormatSize(impKB * 1024, a, sizeof(a));
    if (importing) snprintf(msg, sizeof(msg), "Importing: %d / %d files", impProg.filesDone, impProg.filesTotal);
    else if (busy) snprintf(msg, sizeof(msg), "Scanning mods...");
    else           snprintf(msg, sizeof(msg), "Disk usage: %s", a);
    DrawTextEx(mainFont, msg, (Vector2){50, 586}, 19, 1, fontColor);
    if (importing) {
        Imp_FormatSize(impProg.bytesDone, a, sizeof(a));
        snprintf(msg, sizeof(msg), "%s copied", a);
        DrawTextEx(mainFont, msg,
                   (Vector2){1230 - MeasureTextEx(mainFont, msg, 19, 1).x, 586}, 19, 1, muted);
    } else if (impLastValid && !busy) {
        Imp_FormatSize(impLast.bytes, a, sizeof(a));
        snprintf(msg, sizeof(msg), "Last import: %s", a);
        DrawTextEx(mainFont, msg,
                   (Vector2){1230 - MeasureTextEx(mainFont, msg, 19, 1).x, 586}, 19, 1, muted);
    }
    {
        const Rectangle bar = {50, 616, 1180, 14};
        DrawRectangleRec(bar, (Color){255, 255, 255, 30});
        if (importing && impProg.filesTotal > 0) {
            float f = (float)impProg.filesDone / (float)impProg.filesTotal;
            if (f > 0.99f) f = 0.99f;
            DrawRectangle((int)bar.x, (int)bar.y, (int)(bar.width * f) + 3, 14, selectedColor);
        } else if (busy || importing) {
            const float t = (float)fmod(GetTime() * 0.8, 1.0);
            const float segW = 220;
            const float sx = bar.x + t * (bar.width + segW) - segW;
            const float x0 = sx < bar.x ? bar.x : sx;
            const float x1 = sx + segW > bar.x + bar.width ? bar.x + bar.width : sx + segW;
            if (x1 > x0) DrawRectangle((int)x0, (int)bar.y, (int)(x1 - x0), 14, selectedColor);
        } else if (impKB > 0 && impLastValid && impLast.bytes > 0) {
            float f = (float)((double)impLast.bytes / ((double)impKB * 1024.0));
            if (f > 1.0f) f = 1.0f;
            DrawRectangle((int)bar.x, (int)bar.y, (int)(bar.width * f) + 3, 14, selectedColor);
        }
        DrawRectangleLinesEx(bar, 1, (Color){255, 255, 255, 60});
    }
    x = Imp_Chip(50, 650, "ESC", importing ? "Cancel import" : "Back", selectedColor, 1);
    (void)Imp_Chip(x, 650, "DELETE x2", impConfirmDel ? "Press again to remove all mods" : "Remove all mods",
                   warning, impMods > 0 && impPath[0] == '\0' && !importing);

    Render_StopCamera();
}

static void Modding_DrawStub(void)
{
    const char *message = "COMING SOON";
    const Vector2 center = {640.0f, 360.0f};
    const Vector2 size = MeasureTextEx(mainFont, message, 80, 5);

    Modding_DrawBackground();
    DrawTextEx(mainFont, message,
               (Vector2){center.x - size.x * 0.5f,
                         center.y - size.y * 0.5f},
               80, 5, fontColor);

    if (stubTitle) {
        const Vector2 sub = MeasureTextEx(mainFont, stubTitle, 32, 4);
        DrawTextEx(mainFont, stubTitle,
                   (Vector2){center.x - sub.x * 0.5f, center.y + 80.0f},
                   32, 4, fontColor);
    }

    Render_StopCamera();
}

/* Nota musical (colcheia) feita de elipse + retangulo + triangulo; (x,y) = canto superior esquerdo. */
static void Modding_DrawMusicNote(float x, float y, float size, Color c)
{
    const float headW = size * 0.30f;
    const float headH = size * 0.22f;
    const float stemW = size * 0.08f;
    const float stemX = x + headW * 1.8f;
    const float headY = y + size - headH;

    DrawEllipse((int)(x + headW), (int)headY, headW, headH, c);
    DrawRectangle((int)(stemX - stemW), (int)y, (int)stemW + 1, (int)(headY - y), c);
    DrawTriangle((Vector2){stemX, y},
                 (Vector2){stemX + size * 0.30f, y + size * 0.30f},
                 (Vector2){stemX, y + size * 0.45f}, c);
}

/* Silhueta de pessoa (avatar sem foto): cabeca + ombros. */
static void Modding_DrawPerson(float x, float y, float size, Color c)
{
    const float cx = x + size * 0.5f;
    DrawCircle((int)cx, (int)(y + size * 0.28f), size * 0.22f, c);
    DrawCircleSector((Vector2){cx, y + size * 1.02f}, size * 0.46f, 180.0f, 360.0f, 24, c);
}

/* Icone de download: seta pra baixo + bandeja. */
static void Modding_DrawDownload(float x, float y, float size, Color c)
{
    const float cx = x + size * 0.5f;
    const float stemW = size * 0.18f;
    const float trayY = y + size * 0.86f;

    DrawRectangle((int)(cx - stemW * 0.5f), (int)y, (int)stemW, (int)(size * 0.40f), c);
    DrawTriangle((Vector2){cx - size * 0.30f, y + size * 0.38f},
                 (Vector2){cx, y + size * 0.72f},
                 (Vector2){cx + size * 0.30f, y + size * 0.38f}, c);
    DrawRectangle((int)x, (int)trayY, (int)size, (int)(size * 0.14f), c);
    DrawRectangle((int)x, (int)(trayY - size * 0.16f), (int)(size * 0.14f), (int)(size * 0.16f), c);
    DrawRectangle((int)(x + size * 0.86f), (int)(trayY - size * 0.16f), (int)(size * 0.14f), (int)(size * 0.16f), c);
}

/* Icone de cada aba, na ordem de `tabs`. */
static void Modding_DrawTabIcon(int tab, float x, float y, float size, Color c)
{
    switch (tab) {
        case 0: Modding_DrawMusicNote(x, y, size, c); break;
        case 1: Modding_DrawPerson(x, y, size, c);    break;
        default: Modding_DrawDownload(x, y, size, c); break;
    }
}

static void Modding_DrawList(void)
{
    Modding_DrawBackground();

    for (int i = 0; i < TAB_COUNT; ++i) {
        const float distance = (float)i - offset;
        const float absDistance = (distance < 0.0f) ? -distance : distance;
        const int selectedItem = (selected == i);
        Color textColor = fontColor;
        Color shadowColor = outlineColor;
        const float fontSize = 80.0f;
        const float shadow = 5.0f;
        Vector2 position = {
            shadow + 150.0f - absDistance * 30.0f,
            shadow + 360.0f + distance * 90.0f
        };

        // Selecionado em verde, igual ao menu principal.
        if (selectedItem)
            textColor = selectedColor;
        else
            textColor.a = 150;
        shadowColor.a = selectedItem ? 255 : 100;

        const float iconX = position.x - shadow + MeasureTextEx(mainFont, tabs[i], fontSize, 5).x + 24.0f;
        DrawTextEx(mainFont, tabs[i], position, fontSize, 5, shadowColor);
        Modding_DrawTabIcon(i, iconX + shadow, position.y + 6.0f, fontSize * 0.8f, shadowColor);
        position.x -= shadow;
        position.y -= shadow;
        DrawTextEx(mainFont, tabs[i], position, fontSize, 5, textColor);
        Modding_DrawTabIcon(i, iconX, position.y + 6.0f, fontSize * 0.8f, textColor);
    }

    Render_StopCamera();
}

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
    bg.image = Render_LoadTexture("assets/images/freeplay/menuBGBlue.png");

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
    if (bg.image.id != 0)
        UnloadTexture(bg.image);

    bg.image = (Texture2D){0};
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
