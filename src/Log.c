/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
#include "Log.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/stat.h>

#if defined(_WIN32) || defined(_WIN64)
  #include <windows.h>
#endif

static FILE* logFile = NULL;
static int fileInitAttempted = 0;

// Garante que a pasta assets exista e abre assets/logs.txt
static void EnsureLogFileOpen(void) {
    if (fileInitAttempted) return;
    fileInitAttempted = 1;

#if defined(_WIN32) || defined(_WIN64)
    CreateDirectoryA("assets", NULL);
#else
    mkdir("assets", 0755);
#endif

    logFile = fopen("assets/logs.txt", "a");
    if (logFile) {
        fprintf(logFile, "\n--- SESSION LOG START ---\n");
        fflush(logFile);
    }
}

static void WriteToFile(const char* buf, size_t len) {
    EnsureLogFileOpen();
    if (logFile) {
        fwrite(buf, 1, len, logFile);
        fflush(logFile);
    }
}

static void RawWrite(int fd, const char* s, size_t n) {
    while(n > 0) {
        long w = (long)write(fd, s, n);
        if(w <= 0) break;
        s += (size_t)w;
        n -= (size_t)w;
    }
}

static void Emit(int fd, const char* msg) {
    if(msg == NULL) { 
        RawWrite(fd, "\n", 1); 
        WriteToFile("\n", 1);
        return; 
    }
    size_t len = strlen(msg);
    if(len + 1 < 512) {
        char tmp[512];
        memcpy(tmp, msg, len);
        tmp[len] = '\n';
        RawWrite(fd, tmp, len + 1);
        WriteToFile(tmp, len + 1);
    } else {
        RawWrite(fd, msg, len);
        RawWrite(fd, "\n", 1);
        WriteToFile(msg, len);
        WriteToFile("\n", 1);
    }
}

static void ShowFatalPopup(const char* msg) {
#if defined(_WIN32) || defined(_WIN64)
    MessageBoxA(NULL, msg != NULL ? msg : "Erro desconhecido fatal.", "FNF C Engine - Erro Fatal", MB_OK | MB_ICONERROR);
#else
    if (system("which zenity > /dev/null 2>&1") == 0) {
        char cmd[1024];
        (void)snprintf(cmd, sizeof(cmd), "zenity --error --text=\"%s\" --title=\"FNF C Engine - Erro Fatal\"", msg ? msg : "Erro fatal");
        (void)system(cmd);
    }
#endif
}

static void EmitF(int fd, int fatal, const char* fmt, va_list ap) {
    char buf[512];
    int len = vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
    if(len < 0) len = 0;
    if(len > (int)sizeof(buf) - 2) len = (int)sizeof(buf) - 2;
    buf[len] = '\n';
    
    RawWrite(fd, buf, (size_t)len + 1);
    WriteToFile(buf, (size_t)len + 1);
    
    if(fatal) {
        buf[len] = '\0';
        ShowFatalPopup(buf);
        if (logFile) {
            fclose(logFile);
            logFile = NULL;
        }
        exit(1);
    }
}

void Log_Info(const char* msg)  { Emit(STDOUT_FILENO, msg); }
void Log_Error(const char* msg) { Emit(STDERR_FILENO, msg); }

void Log_Fatal(const char* msg) { 
    Emit(STDERR_FILENO, msg); 
    ShowFatalPopup(msg);
    if (logFile) {
        fclose(logFile);
        logFile = NULL;
    }
    exit(1); 
}

void Log_ErrorF(const char* fmt, ...) {
    va_list ap; 
    va_start(ap, fmt);
    EmitF(STDERR_FILENO, 0, fmt, ap);
    va_end(ap);
}

void Log_FatalF(const char* fmt, ...) {
    va_list ap; 
    va_start(ap, fmt);
    EmitF(STDERR_FILENO, 1, fmt, ap);
    va_end(ap);
}