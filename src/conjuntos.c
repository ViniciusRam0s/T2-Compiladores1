#include <stdio.h>
#include <string.h>
#include "conjuntos.h"
#include "util.h"

/* Fornecido */

static void iniciar_expr(Expr *e, void *ctx)
{
    int n = *(int *) ctx;
    bitset_liberar(&e->first);
    bitset_liberar(&e->follow);
    bitset_iniciar(&e->first, n);
    bitset_iniciar(&e->follow, n);
    e->anulavel = 0;
}

void conj_iniciar(Gramatica *g)
{
    int i, n = g->nterms;
    for (i = 0; i < g->nnts; i++) {
        bitset_liberar(&g->nts[i].first);
        bitset_liberar(&g->nts[i].follow);
        bitset_iniciar(&g->nts[i].first, n);
        bitset_iniciar(&g->nts[i].follow, n);
        g->nts[i].anulavel = 0;
        expr_percorrer(g->nts[i].regra, iniciar_expr, &n);
    }
}

void conj_imprimir(FILE *f, const Gramatica *g)
{
    int i;
    size_t largura = strlen("NAO TERMINAL");
    for (i = 0; i < g->nnts; i++)
        if (strlen(g->nts[i].nome) > largura)
            largura = strlen(g->nts[i].nome);
    fprintf(f, "%-*s  ANULAVEL  FIRST / FOLLOW\n", (int) largura, "NAO TERMINAL");
    for (i = 0; i < g->nnts; i++) {
        fprintf(f, "%-*s  %-8s  FIRST  = ", (int) largura, g->nts[i].nome,
                g->nts[i].anulavel ? "sim" : "nao");
        gram_imprimir_conjunto(f, g, &g->nts[i].first);
        fprintf(f, "\n%-*s            FOLLOW = ", (int) largura, "");
        gram_imprimir_conjunto(f, g, &g->nts[i].follow);
        fputc('\n', f);
    }
}

/* Anulavel */

/* Calcula e grava e->anulavel (e o dos descendentes) com os valores*/
static int calc_anulavel(const Gramatica *g, Expr *e)
{
    int i, r = 0;
    switch (e->tipo) {
    case E_TERM:
        r = 0;
        break;
    case E_VAZIO:
        r = 1;
        break;
    case E_NAOTERM:
        r = g->nts[e->simbolo].anulavel;
        break;
    case E_SEQ:                         /* todos os filhos anulaveis */
        r = 1;
        for (i = 0; i < e->nfilhos; i++)
            if (!calc_anulavel(g, e->filhos[i]))
                r = 0;
        break;
    case E_ALT:                         /* algum filho anulavel */
        r = 0;
        for (i = 0; i < e->nfilhos; i++)
            if (calc_anulavel(g, e->filhos[i]))
                r = 1;
        break;
    case E_OPC: case E_REP:             /* sempre; o filho e so calculado */
        calc_anulavel(g, e->filhos[0]);
        r = 1;
        break;
    }
    e->anulavel = r;
    return r;
}

/* Ponto fixo: comeca com todos "nao anulavel" (conj_iniciar) e repete
 * enquanto algum nao terminal passar a anulavel.*/
void conj_anulavel(Gramatica *g)
{
    int i, mudou;
    do {
        mudou = 0;
        for (i = 0; i < g->nnts; i++) {
            if (g->nts[i].regra == NULL)
                continue;
            if (calc_anulavel(g, g->nts[i].regra) && !g->nts[i].anulavel) {
                g->nts[i].anulavel = 1;
                mudou = 1;
            }
        }
    } while (mudou);
}

/* FIRST*/
/* Acrescenta a e->first (e ao dos descendentes) o que os FIRST atuais
 * permitem concluir. Devolve 1 se algum conjunto cresceu. */
static int calc_first(const Gramatica *g, Expr *e)
{
    int i, mudou = 0, prefixo_anulavel = 1;
    switch (e->tipo) {
    case E_TERM:
        if (!bitset_tem(&e->first, e->simbolo)) {
            bitset_add(&e->first, e->simbolo);
            mudou = 1;
        }
        break;
    case E_NAOTERM:
        mudou = bitset_unir(&e->first, &g->nts[e->simbolo].first);
        break;
    case E_VAZIO:
        break;
    case E_SEQ:
        for (i = 0; i < e->nfilhos; i++) {
            mudou |= calc_first(g, e->filhos[i]);
            if (prefixo_anulavel) {
                mudou |= bitset_unir(&e->first, &e->filhos[i]->first);
                if (!e->filhos[i]->anulavel)
                    prefixo_anulavel = 0;
            }
        }
        break;
    case E_ALT: case E_OPC: case E_REP:
        for (i = 0; i < e->nfilhos; i++) {
            mudou |= calc_first(g, e->filhos[i]);
            mudou |= bitset_unir(&e->first, &e->filhos[i]->first);
        }
        break;
    }
    return mudou;
}

void conj_first(Gramatica *g)
{
    int i, mudou;
    do {
        mudou = 0;
        for (i = 0; i < g->nnts; i++) {
            if (g->nts[i].regra == NULL)
                continue;
            mudou |= calc_first(g, g->nts[i].regra);
            mudou |= bitset_unir(&g->nts[i].first, &g->nts[i].regra->first);
        }
    } while (mudou);
}

/* FOLLOW*/
/* Propaga e->follow para os descendentes de e (e para o FOLLOW dos
 * nao terminais usados). Devolve 1 se algum conjunto cresceu. */
static int calc_follow(Gramatica *g, Expr *e)
{
    int i, mudou = 0;
    Bitset depois;

    switch (e->tipo) {
    case E_TERM: case E_VAZIO:
        break;
    case E_NAOTERM:
        mudou = bitset_unir(&g->nts[e->simbolo].follow, &e->follow);
        break;
    case E_SEQ:
        bitset_iniciar(&depois, g->nterms);
        bitset_copiar(&depois, &e->follow);
        for (i = e->nfilhos - 1; i >= 0; i--) {
            mudou |= bitset_unir(&e->filhos[i]->follow, &depois);
            if (!e->filhos[i]->anulavel)
                bitset_limpar(&depois);
            bitset_unir(&depois, &e->filhos[i]->first);
        }
        bitset_liberar(&depois);
        for (i = 0; i < e->nfilhos; i++)
            mudou |= calc_follow(g, e->filhos[i]);
        break;
    case E_ALT: case E_OPC: case E_REP:
        /* em { a }, apos uma volta do laco pode vir outra: FIRST(a) segue a */
        if (e->tipo == E_REP)
            mudou |= bitset_unir(&e->filhos[0]->follow, &e->filhos[0]->first);
        for (i = 0; i < e->nfilhos; i++) {
            mudou |= bitset_unir(&e->filhos[i]->follow, &e->follow);
            mudou |= calc_follow(g, e->filhos[i]);
        }
        break;
    }
    return mudou;
}

void conj_follow(Gramatica *g)
{
    int i, mudou;
    if (g->inicial >= 0)
        bitset_add(&g->nts[g->inicial].follow, TERM_FIM);
    do {
        mudou = 0;
        for (i = 0; i < g->nnts; i++) {
            if (g->nts[i].regra == NULL)
                continue;
            mudou |= bitset_unir(&g->nts[i].regra->follow, &g->nts[i].follow);
            mudou |= calc_follow(g, g->nts[i].regra);
        }
    } while (mudou);
}
