/*
 * ll1.c - Verificacao da condicao LL(1) (etapa 4).
 *
 * Condicoes (secao 3 do enunciado):
 *   1. Em a1 | a2 | ... | an, os FIRST das alternativas sao disjuntos dois
 *      a dois e no maximo uma alternativa e anulavel.
 *   2. Se uma alternativa ai e anulavel, FOLLOW(alternativa) e disjunto
 *      dos FIRST das demais.
 *   3. Em [ a ] e { a }, a nao e anulavel e FIRST(a) e disjunto do
 *      FOLLOW da construcao.
 */
#include <stdarg.h>
#include <stdio.h>
#include "ll1.h"
#include "util.h"

/* Reporta um conflito na regra do nao terminal nt, na posicao de e.
 * comum (pode ser NULL) e o conjunto de tokens em conflito. */
static void reportar(const Gramatica *g, int nt, const Expr *e, int eh_aviso,
                     const Bitset *comum, const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "%s:%d:%d: %s: conflito LL(1) em %s: ", g->arquivo, e->linha,
            e->coluna, eh_aviso ? "aviso" : "erro", g->nts[nt].nome);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    if (comum != NULL) {
        fprintf(stderr, " ");
        gram_imprimir_conjunto(stderr, g, comum);
    }
    fputc('\n', stderr);
}

static const char *delimitadores(const Expr *e)
{
    return e->tipo == E_OPC ? "[ ]" : "{ }";
}

/* Condicoes 1 e 2 para o no E_ALT e. Devolve o numero de erros. */
static int verificar_alt(const Gramatica *g, int nt, const Expr *e)
{
    int i, j, erros = 0;
    Bitset comum;

    bitset_iniciar(&comum, g->nterms);
    for (i = 0; i < e->nfilhos; i++) {
        for (j = i + 1; j < e->nfilhos; j++) {
            bitset_intersecao(&comum, &e->filhos[i]->first, &e->filhos[j]->first);
            if (!bitset_vazio(&comum)) {
                reportar(g, nt, e, 0, &comum, "alternativas %d e %d comecam com",
                         i + 1, j + 1);
                erros++;
            }
            if (e->filhos[i]->anulavel && e->filhos[j]->anulavel) {
                reportar(g, nt, e, 0, NULL, "alternativas %d e %d sao ambas anulaveis",
                         i + 1, j + 1);
                erros++;
            }
        }
    }
    for (i = 0; i < e->nfilhos; i++) {
        if (!e->filhos[i]->anulavel)
            continue;
        for (j = 0; j < e->nfilhos; j++) {
            if (j == i || e->filhos[j]->anulavel)
                continue;
            bitset_intersecao(&comum, &e->follow, &e->filhos[j]->first);
            if (!bitset_vazio(&comum)) {
                reportar(g, nt, e, 0, &comum, "a alternativa %d e anulavel e o FOLLOW "
                         "intersecta o FIRST da alternativa %d em", i + 1, j + 1);
                erros++;
            }
        }
    }
    bitset_liberar(&comum);
    return erros;
}

/* Condicao 3 para o no E_OPC ou E_REP e. Devolve o numero de erros. */
static int verificar_opc_rep(const Gramatica *g, int nt, const Expr *e)
{
    const Expr *a = e->filhos[0];
    Bitset comum;

    if (a->anulavel) {
        reportar(g, nt, e, 0, NULL, "o conteudo de %s e anulavel", delimitadores(e));
        return 1;
    }
    bitset_iniciar(&comum, g->nterms);
    bitset_intersecao(&comum, &a->first, &e->follow);
    if (!bitset_vazio(&comum))
        reportar(g, nt, e, 1, &comum, "FIRST e FOLLOW de %s se intersectam "
                 "(resolvido de forma gulosa) em", delimitadores(e));
    bitset_liberar(&comum);
    return 0;
}

/* Verifica e e todos os seus descendentes (pre-ordem). */
static int verificar_expr(const Gramatica *g, int nt, const Expr *e)
{
    int i, erros = 0;
    if (e->tipo == E_ALT)
        erros += verificar_alt(g, nt, e);
    else if (e->tipo == E_OPC || e->tipo == E_REP)
        erros += verificar_opc_rep(g, nt, e);
    for (i = 0; i < e->nfilhos; i++)
        erros += verificar_expr(g, nt, e->filhos[i]);
    return erros;
}

/* Devolve o numero de ERROS (avisos nao contam). */
int ll1_verificar(const Gramatica *g)
{
    int i, erros = 0;
    for (i = 0; i < g->nnts; i++)
        if (g->nts[i].regra != NULL)
            erros += verificar_expr(g, i, g->nts[i].regra);
    return erros;
}
