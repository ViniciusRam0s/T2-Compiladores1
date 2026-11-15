/*
 * strtab.c - Tabela de strings: tabela de dispersao (hash) com
 * encadeamento externo (ver strtab.h).
 *
 * A funcao de dispersao e a djb2 (h = h * 33 + c). Colisoes sao
 * resolvidas com uma lista ligada por balde. O numero de baldes e fixo:
 * programas de Micro C tem poucos lexemas distintos.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "strtab.h"

#define NUM_BALDES 211

typedef struct Entrada {
    char *texto;
    struct Entrada *prox;
} Entrada;

static Entrada *baldes[NUM_BALDES];
static int quantidade = 0;

static unsigned long dispersao(const char *s)
{
    unsigned long h = 5381;
    for (; *s != '\0'; s++)
        h = h * 33 + (unsigned char) *s;
    return h;
}

const char *strtab_inserir(const char *s)
{
    unsigned long b = dispersao(s) % NUM_BALDES;
    Entrada *e;

    for (e = baldes[b]; e != NULL; e = e->prox)
        if (strcmp(e->texto, s) == 0)
            return e->texto;

    e = (Entrada *) malloc(sizeof(Entrada));
    if (e != NULL)
        e->texto = (char *) malloc(strlen(s) + 1);
    if (e == NULL || e->texto == NULL) {
        fprintf(stderr, "memoria insuficiente\n");
        exit(EXIT_FAILURE);
    }
    strcpy(e->texto, s);
    e->prox = baldes[b];
    baldes[b] = e;
    quantidade++;
    return e->texto;
}

int strtab_tamanho(void)
{
    return quantidade;
}

void strtab_liberar(void)
{
    int b;
    Entrada *e, *prox;

    for (b = 0; b < NUM_BALDES; b++) {
        for (e = baldes[b]; e != NULL; e = prox) {
            prox = e->prox;
            free(e->texto);
            free(e);
        }
        baldes[b] = NULL;
    }
    quantidade = 0;
}
