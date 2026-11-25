/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * JsonStream.h - leitor JSON por fluxo (pull parser). Le o arquivo em blocos de 64 KB e entrega
 * tokens sob demanda. Nao monta arvore: quem usa consome o que interessa e pula o resto com
 * JsonStream_SkipValue. Uso de memoria constante, independe do tamanho do arquivo.
 *
 * Convencao de erro: Peek devolve -1 e Fail marca o fluxo como invalido; depois disso todas as
 * chamadas viram no-op e JsonStream_Failed reporta o erro. Leituras que so casam um tipo
 * (ReadString, ReadNumber, ReadBool) devolvem 0 SEM erro quando o proximo valor e de outro tipo,
 * assim o chamador pode pular o valor com SkipValue.
 */
#pragma once
#ifndef JSONSTREAM_H
#define JSONSTREAM_H

#include <stddef.h>

typedef struct JsonStream JsonStream;

JsonStream *JsonStream_Open(const char *path);   /* NULL se nao abrir */
void JsonStream_Close(JsonStream *js);

int JsonStream_Failed(const JsonStream *js);
void JsonStream_Fail(JsonStream *js);

/* Proximo byte nao-espaco, sem consumir. -1 = fim do arquivo ou erro. */
int JsonStream_Peek(JsonStream *js);
/* Consome ch (depois de espacos). Se nao for ch, marca erro. */
int JsonStream_Eat(JsonStream *js, int ch);
/* Consome ch se for o proximo; sem erro quando nao for. */
int JsonStream_TryEat(JsonStream *js, int ch);

/* String JSON decodificada (escapes tratados; \u fora de ASCII vira '?') truncada em cap-1.
 * out pode ser NULL (so pula). 0 se o proximo valor nao for string. */
int JsonStream_ReadString(JsonStream *js, char *out, size_t cap);
/* Numero. 0 se o proximo valor nao for numero. */
int JsonStream_ReadNumber(JsonStream *js, double *out);
/* true/false. 0 se nao for booleano. */
int JsonStream_ReadBool(JsonStream *js, int *out);
/* Pula um valor qualquer (escalar, string, objeto ou array), inclusive aninhado. */
void JsonStream_SkipValue(JsonStream *js);

#endif /* JSONSTREAM_H */
