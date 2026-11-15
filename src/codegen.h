/*
 * codegen.h - Geracao do analisador sintatico descendente recursivo (etapa 5).
 */
#ifndef CODEGEN_H
#define CODEGEN_H

#include "gramatica.h"

typedef struct {
    int microc;            /* --scanner microc */
    int arvore;            /* --tree: gera suporte a impressao da arvore */
    const char *prefixo;   /* -o: caminho sem extensao */
    const char *origem;    /* nome do arquivo .ebnf */
} OpcoesGeracao;

/* Retorna 0 em caso de sucesso. */
int codegen_gerar(Gramatica *g, const OpcoesGeracao *op);

#endif
