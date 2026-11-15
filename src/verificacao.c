/*
 * verificacao.c - Verificacoes semanticas da gramatica (etapa 2).
 *
 *   - verif_indefinidos: nao terminais usados mas nao definidos (erro);
 *   - verif_terminais:   terminais nomeados sem declaracao (erro);
 *   - verif_inalcancaveis      (aviso)
 *   - verif_improdutivos       (erro)
 *   - verif_recursao_esquerda  (erro; direta e indireta)
 *
 * A duplicidade de definicoes ja e detectada durante a leitura
 * (ebnf_parser.c).
 */
#include <stdlib.h>
#include <string.h>
#include "verificacao.h"
#include "microc_mapa.h"
#include "util.h"

/* ------------------------------------------------------------------ */
/* Fornecido                                                          */
/* ------------------------------------------------------------------ */

static int verif_indefinidos(Gramatica *g)
{
    int i, erros = 0;
    for (i = 0; i < g->nnts; i++) {
        if (g->nts[i].regra == NULL) {
            diag_erro(g->nts[i].linha, g->nts[i].coluna,
                      "nao terminal %s usado mas nao definido", g->nts[i].nome);
            erros++;
        }
    }
    return erros;
}

static int verif_terminais(Gramatica *g, int modo_microc)
{
    int i, erros = 0, carregado = 0;
    Terminal *t;

    if (modo_microc)
        carregado = microc_carregar_tokens_h(g->tokens_externos) == 0;

    for (i = 1; i < g->nterms; i++) {
        t = &g->terms[i];
        if (modo_microc) {
            if (t->literal && microc_nome_literal(t->nome) == NULL) {
                diag_erro(t->linha, t->coluna, "literal %s nao corresponde a nenhum "
                          "token de Micro C", t->exibicao);
                erros++;
            } else if (!t->literal && carregado && !microc_token_existe(t->nome)) {
                diag_erro(t->linha, t->coluna, "terminal %s nao existe na enumeracao "
                          "TokenType de %s", t->nome, g->tokens_externos);
                erros++;
            }
        } else if (!t->declarado) {
            diag_erro(t->linha, t->coluna, "terminal %s nao declarado em TOKENS", t->nome);
            erros++;
        }
    }
    return erros;
}

/* ------------------------------------------------------------------ */
/* Inalcancaveis                                                      */
/* ------------------------------------------------------------------ */

/* Marca em ctx (Bitset de nao terminais) os nao terminais usados em e. */
static void marcar_usados(Expr *e, void *ctx)
{
    if (e->tipo == E_NAOTERM)
        bitset_add((Bitset *) ctx, e->simbolo);
}

/* Emite um AVISO para cada nao terminal definido que nao pode ser
 * alcancado a partir do simbolo inicial. Ponto fixo: o inicial e
 * alcancavel, e todo nao terminal usado na regra de um alcancavel
 * tambem e. Avisos nao contam como erro: devolve sempre 0. */
static int verif_inalcancaveis(Gramatica *g)
{
    Bitset alcancaveis, usados;
    int i, mudou;

    bitset_iniciar(&alcancaveis, g->nnts);
    bitset_iniciar(&usados, g->nnts);
    bitset_add(&alcancaveis, g->inicial);
    do {
        mudou = 0;
        for (i = 0; i < g->nnts; i++) {
            if (!bitset_tem(&alcancaveis, i))
                continue;
            bitset_limpar(&usados);
            expr_percorrer(g->nts[i].regra, marcar_usados, &usados);
            if (bitset_unir(&alcancaveis, &usados))
                mudou = 1;
        }
    } while (mudou);

    for (i = 0; i < g->nnts; i++)
        if (g->nts[i].regra != NULL && !bitset_tem(&alcancaveis, i))
            diag_aviso(g->nts[i].linha, g->nts[i].coluna,
                       "nao terminal %s inalcancavel a partir de %s",
                       g->nts[i].nome, g->nts[g->inicial].nome);
    bitset_liberar(&alcancaveis);
    bitset_liberar(&usados);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Improdutivos                                                       */
/* ------------------------------------------------------------------ */

/* 1 se e deriva alguma cadeia de terminais, supondo produtivos apenas
 * os nao terminais ja marcados em prod. */
static int expr_produtiva(const Expr *e, const Bitset *prod)
{
    int i;
    switch (e->tipo) {
    case E_TERM: case E_VAZIO: case E_OPC: case E_REP:
        return 1;                   /* [ ] e { } derivam a cadeia vazia */
    case E_NAOTERM:
        return bitset_tem(prod, e->simbolo);
    case E_SEQ:
        for (i = 0; i < e->nfilhos; i++)
            if (!expr_produtiva(e->filhos[i], prod))
                return 0;
        return 1;
    case E_ALT:
        for (i = 0; i < e->nfilhos; i++)
            if (expr_produtiva(e->filhos[i], prod))
                return 1;
        return 0;
    }
    return 0;
}

/* Emite um ERRO para cada nao terminal que nao deriva nenhuma cadeia de
 * terminais (ex.: a = "x" a ;). So e chamada com todos os nao terminais
 * definidos. Devolve o numero de erros. */
static int verif_improdutivos(Gramatica *g)
{
    Bitset prod;
    int i, mudou, erros = 0;

    bitset_iniciar(&prod, g->nnts);
    do {
        mudou = 0;
        for (i = 0; i < g->nnts; i++) {
            if (!bitset_tem(&prod, i) && expr_produtiva(g->nts[i].regra, &prod)) {
                bitset_add(&prod, i);
                mudou = 1;
            }
        }
    } while (mudou);

    for (i = 0; i < g->nnts; i++) {
        if (!bitset_tem(&prod, i)) {
            diag_erro(g->nts[i].linha_def, 0, "nao terminal %s improdutivo: "
                      "nao deriva nenhuma cadeia de terminais", g->nts[i].nome);
            erros++;
        }
    }
    bitset_liberar(&prod);
    return erros;
}

/* ------------------------------------------------------------------ */
/* Recursao a esquerda                                                */
/* ------------------------------------------------------------------ */

/* Acrescenta a esq os nao terminais que podem aparecer na ponta
 * esquerda de uma derivacao de e. Numa sequencia, olha-se cada filho
 * enquanto os anteriores forem anulaveis: em [ x ] a ..., o a tambem
 * esta na ponta esquerda. */
static void ponta_esquerda(const Expr *e, Bitset *esq)
{
    int i;
    switch (e->tipo) {
    case E_NAOTERM:
        bitset_add(esq, e->simbolo);
        break;
    case E_SEQ:
        for (i = 0; i < e->nfilhos; i++) {
            ponta_esquerda(e->filhos[i], esq);
            if (!e->filhos[i]->anulavel)
                break;
        }
        break;
    case E_ALT: case E_OPC: case E_REP:
        for (i = 0; i < e->nfilhos; i++)
            ponta_esquerda(e->filhos[i], esq);
        break;
    case E_TERM: case E_VAZIO:
        break;
    }
}

/* Detecta recursao a esquerda direta (a = a ...) e indireta
 * (a = b ... ; b = a ...). direto[i] guarda os nao terminais na ponta
 * esquerda da regra de i; fecho[i] e o fecho transitivo dessa relacao.
 * Ha recursao em i quando i pertence ao proprio fecho. */
int verif_recursao_esquerda(Gramatica *g)
{
    Bitset *direto, *fecho;
    int i, j, mudou, erros = 0, n = g->nnts;

    direto = (Bitset *) xcalloc((size_t) n, sizeof(Bitset));
    fecho = (Bitset *) xcalloc((size_t) n, sizeof(Bitset));
    for (i = 0; i < n; i++) {
        bitset_iniciar(&direto[i], n);
        bitset_iniciar(&fecho[i], n);
        ponta_esquerda(g->nts[i].regra, &direto[i]);
        bitset_copiar(&fecho[i], &direto[i]);
    }
    do {
        mudou = 0;
        for (i = 0; i < n; i++)
            for (j = bitset_proximo(&direto[i], 0); j >= 0;
                 j = bitset_proximo(&direto[i], j + 1))
                if (bitset_unir(&fecho[i], &fecho[j]))
                    mudou = 1;
    } while (mudou);

    for (i = 0; i < n; i++) {
        if (bitset_tem(&direto[i], i)) {
            diag_erro(g->nts[i].linha_def, 0,
                      "nao terminal %s tem recursao a esquerda direta", g->nts[i].nome);
            erros++;
        } else if (bitset_tem(&fecho[i], i)) {
            diag_erro(g->nts[i].linha_def, 0,
                      "nao terminal %s tem recursao a esquerda indireta", g->nts[i].nome);
            erros++;
        }
    }
    for (i = 0; i < n; i++) {
        bitset_liberar(&direto[i]);
        bitset_liberar(&fecho[i]);
    }
    free(direto);
    free(fecho);
    return erros;
}

int verif_basica(Gramatica *g, int modo_microc)
{
    int erros = 0;
    if (g->inicial < 0) {
        diag_erro(0, 0, "a gramatica nao tem nenhuma regra");
        return 1;
    }
    erros += verif_indefinidos(g);
    erros += verif_terminais(g, modo_microc);
    erros += verif_inalcancaveis(g);
    if (erros == 0)
        erros += verif_improdutivos(g);
    return erros;
}
