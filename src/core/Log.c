/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
#include "Log.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <unistd.h>
#include <sys/stat.h>

#if defined(_WIN32) || defined(_WIN64)
  #include <windows.h>
#endif

// Trava simples (spin) para o log poder ser usado por threads de fundo.
// Todas as funcoes publicas travam; as internas (Emit, EmitF, WriteToFile) nao.
static atomic_flag logBusy;

static void LogLock(void) {
    while(atomic_flag_test_and_set_explicit(&logBusy, memory_order_acquire)) {
#if defined(_WIN32) || defined(_WIN64)
        SwitchToThread();
#else
        sched_yield();
#endif
    }
}

static void LogUnlock(void) {
    atomic_flag_clear_explicit(&logBusy, memory_order_release);
}

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

void Log_Info(const char* msg)  { LogLock(); Emit(STDOUT_FILENO, msg); LogUnlock(); }
void Log_Error(const char* msg) { LogLock(); Emit(STDERR_FILENO, msg); LogUnlock(); }

void Log_Fatal(const char* msg) { 
    LogLock(); // nao destrava: o processo vai sair
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
    LogLock();
    EmitF(STDERR_FILENO, 0, fmt, ap);
    LogUnlock();
    va_end(ap);
}

static char warnText[LOG_WARN_MAX][LOG_WARN_LEN];
static int warnTotal = 0;

void Log_WarnF(const char* fmt, ...) {
    char text[LOG_WARN_LEN * 2];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);

    char line[LOG_WARN_LEN * 2 + 8];
    snprintf(line, sizeof(line), "[WARN] %s", text);
    LogLock();
    Emit(STDERR_FILENO, line);

    if (warnTotal < LOG_WARN_MAX) {
        memcpy(warnText[warnTotal], text, LOG_WARN_LEN - 1);
        warnText[warnTotal][LOG_WARN_LEN - 1] = '\0';
    }
    warnTotal++;
    LogUnlock();
}

void Log_WarnClear(void) { LogLock(); warnTotal = 0; LogUnlock(); }
int Log_WarnCount(void) { LogLock(); int n = warnTotal; LogUnlock(); return n; }
const char* Log_WarnAt(int index) {
    if (index < 0 || index >= warnTotal || index >= LOG_WARN_MAX) return "";
    return warnText[index];
}

void Log_FatalF(const char* fmt, ...) {
    va_list ap; 
    va_start(ap, fmt);
    LogLock(); // nao destrava: o processo vai sair
    EmitF(STDERR_FILENO, 1, fmt, ap);
    va_end(ap);
}