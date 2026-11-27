/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * ModdingMenuInput.c - entrada de texto/colar/apagar do caminho de origem e execucao do import.
 */
#include "scenes/modding/ModdingMenuInternal.h"
#include "data/modimport/ModImport.h"

#include <stdio.h>
#include <string.h>

void Imp_RefreshUsage(void)
{
    ModImport_ScanStart("assets/mods");
}

void Imp_PollUsage(void)
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

void Imp_RunImport(void)
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

void Imp_UpdateInput(void)
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
