/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
#pragma once
#ifndef LOG_H
#define LOG_H

// ~ O Código ta no log C
// Escrita baixo nivel, sem stdio bufferizado:
// Windows -> WriteFile, resto -> write(fileno).
void Log_Info(const char* msg);
void Log_Error(const char* msg);
void Log_Fatal(const char* msg); // stderr + exit(1), nao retorna
void Log_ErrorF(const char* fmt, ...);
void Log_FatalF(const char* fmt, ...); // nao retorna

// Avisos: vao pro log ("[WARN] ...") e ficam guardados para a tela de avisos do PlayState.
// Guarda ate LOG_WARN_MAX mensagens de LOG_WARN_LEN caracteres; as extras so vao pro log.
#define LOG_WARN_MAX 12
#define LOG_WARN_LEN 120
void Log_WarnF(const char* fmt, ...);
void Log_WarnClear(void);
int Log_WarnCount(void);                 // total de avisos desde o ultimo Clear (pode passar de LOG_WARN_MAX)
const char* Log_WarnAt(int index);       // 0 .. min(count, LOG_WARN_MAX)-1

#endif
