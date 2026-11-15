#ifndef GRAMATICA_H
#define GRAMATICA_H

#include <stdio.h>
#include "bitset.h"

#define TERM_FIM 0   /* indice do terminal especial "$" (fim de arquivo) */

typedef enum {
    E_TERM, E_NAOTERM, E_SEQ, E_ALT, E_OPC, E_REP, E_VAZIO
} TipoExpr;

typedef struct Expr {
    TipoExpr tipo;
    int simbolo;               /* E_TERM e E_NAOTERM */
    struct Expr **filhos;      /* E_SEQ, E_ALT: n filhos; E_OPC, E_REP: 1 */
    int nfilhos, capfilhos;
    int linha, coluna;         /* posicao no arquivo .ebnf */

    /* Atributos calculados em conjuntos.c */
    int anulavel;
    Bitset first;
    Bitset follow;
} Expr;

typedef struct {
    char *nome;
    int literal;
    char *regex;
    char *exibicao;
    char *nome_c;
    int declarado;
    int linha, coluna;
} Terminal;

typedef struct {
    char *nome;
    Expr *regra;
    int linha, coluna;
    int linha_def;

    /* Atributos calculados em conjuntos.c */
    int anulavel;
    Bitset first, follow;
} NaoTerminal;

typedef struct {
    char *arquivo;
    Terminal *terms;     int nterms, capterms;
    NaoTerminal *nts;    int nnts, capnts;
    char **skips;        int nskips, capskips;
    int inicial;
    char *tokens_externos; 
} Gramatica;

/* --- Gramatica --- */
Gramatica *gram_criar(const char *arquivo);
void gram_liberar(Gramatica *g);

/* Busca o terminal; se nao existir, cria. Retorna o indice. */
int gram_terminal(Gramatica *g, const char *nome, int literal,
                  int linha, int coluna);
/* Busca o terminal; retorna -1 se nao existir. */
int gram_busca_terminal(const Gramatica *g, const char *nome, int literal);

/* Busca o nao terminal; se nao existir, cria (ainda sem regra). */
int gram_naoterminal(Gramatica *g, const char *nome, int linha, int coluna);
int gram_busca_naoterminal(const Gramatica *g, const char *nome);

void gram_add_skip(Gramatica *g, const char *regex);

/* Expressoes*/
Expr *expr_criar(TipoExpr tipo, int linha, int coluna);
Expr *expr_simbolo(TipoExpr tipo, int simbolo, int linha, int coluna);
Expr *expr_unario(TipoExpr tipo, Expr *filho, int linha, int coluna);
void  expr_adicionar(Expr *pai, Expr *filho);
void  expr_liberar(Expr *e);

/* Percorre a arvore em pre-ordem chamando f(e, ctx) para cada no. */
void  expr_percorrer(Expr *e, void (*f)(Expr *e, void *ctx), void *ctx);

/* Imprime a expressao de volta em EBNF (usado em mensagens e comentarios). */
void  expr_imprimir(FILE *f, const Gramatica *g, const Expr *e);

/* Imprime a gramatica inteira em EBNF (opcao --debug). */
void  gram_imprimir(FILE *f, const Gramatica *g);

/* Imprime um conjunto de terminais como {A, B, 'c'} */
void  gram_imprimir_conjunto(FILE *f, const Gramatica *g, const Bitset *b);

#endif
