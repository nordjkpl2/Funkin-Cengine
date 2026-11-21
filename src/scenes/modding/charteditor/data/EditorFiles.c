/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: utilitarios de arquivo e nome. */

#include "scenes/modding/charteditor/data/EditorFiles.h"

#include <stdio.h>
#include <string.h>

const char* EdDirName(const char* path) {
    const char* a = strrchr(path, '/');
    const char* b = strrchr(path, '\\');
    const char* s = (a > b) ? a : b;
    return (s != NULL) ? s + 1 : path;
}

char EdCopyFile(const char* src, const char* dst) {
    FILE* in = fopen(src, "rb");
    if(in == NULL) return 0;
    FILE* out = fopen(dst, "wb");
    if(out == NULL) {
        fclose(in);
        return 0;
    }
    /* buffer de 64 KB: menos chamadas de sistema em audio grande. Erro de escrita conta como falha */
    char buf[65536];
    size_t n;
    char ok = 1;
    while((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if(fwrite(buf, 1, n, out) != n) {
            ok = 0;
            break;
        }
    }
    if(ferror(in)) ok = 0;
    if(fclose(in) != 0) ok = 0;
    if(fclose(out) != 0) ok = 0;
    if(!ok) remove(dst);
    return ok;
}

void EdSanitizeName(char* dst, size_t dstSize, const char* src) {
    size_t w = 0;
    for(size_t i = 0; src[i] != 0 && w + 1 < dstSize; i++) {
        char ch = src[i];
        if((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9') || ch == '_' || ch == '-')
            dst[w++] = ch;
    }
    dst[w] = 0;
}
