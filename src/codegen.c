/*
 * codegen.c - Geracao do analisador sintatico (etapa 5).
 *
 * Estrutura do arquivo .c gerado:
 *
 *   1. includes e conjuntos constantes (conj_0, conj_1, ...)
 *   2. estado e primitivas: avancar, match, erro_sintatico, arvore
 *   3. modo proprio: analisador lexico gerado a partir de TOKENS
 *      modo Micro C: tokens obtidos de proximo_token() (scanner do TP1)
 *   4. leitura de um arquivo de tokens (opcao --tokens do analisador)
 *   5. um procedimento parse_X() por nao terminal
 *   6. main
 *
 * Pontos centrais:
 *   - gen_expr: traducao de cada construcao da EBNF em codigo C
 *     (esquema da secao 4 do enunciado);
 *   - gen_casamento_regex: converte uma expressao regular de TOKENS em
 *     codigo C (automato finito) que reconhece o maior prefixo;
 *   - gen_token_de_arquivo: leitura do arquivo de tokens do TP1.
 */
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen.h"
#include "microc_mapa.h"
#include "util.h"

/* ------------------------------------------------------------------ */
/* Emissao de texto                                                   */
/* ------------------------------------------------------------------ */

typedef struct {
    FILE *f;
    int nivel;          /* nivel de indentacao (4 espacos por nivel) */
} Emissor;

/* Emite uma linha indentada; fmt segue printf. */
static void emit(Emissor *em, const char *fmt, ...)
{
    int i;
    va_list ap;
    for (i = 0; i < em->nivel; i++)
        fputs("    ", em->f);
    va_start(ap, fmt);
    vfprintf(em->f, fmt, ap);
    va_end(ap);
    fputc('\n', em->f);
}

/* Emite linhas fixas, sem interpretar '%'. A lista termina em NULL. */
static void emit_linhas(FILE *f, const char *const *linhas)
{
    for (; *linhas != NULL; linhas++) {
        fputs(*linhas, f);
        fputc('\n', f);
    }
}

/* Texto dentro de comentario C: evita fechar o comentario por engano. */
static void emitir_comentario(FILE *f, const char *s)
{
    for (; *s != '\0'; s++) {
        if (s[0] == '*' && s[1] == '/') {
            fputs("* /", f);
            s++;
        } else if (*s == '\n') {
            fputc(' ', f);
        } else {
            fputc(*s, f);
        }
    }
}

/* String C entre aspas, com escapes. */
static void emitir_string_c(FILE *f, const char *s)
{
    fputc('"', f);
    for (; *s != '\0'; s++) {
        unsigned char c = (unsigned char) *s;
        if (c == '"' || c == '\\')
            fprintf(f, "\\%c", c);
        else if (c == '\n')
            fputs("\\n", f);
        else if (c == '\t')
            fputs("\\t", f);
        else if (c < 32 || c > 126)
            fprintf(f, "\\%03o", c);
        else
            fputc(c, f);
    }
    fputc('"', f);
}

/* ------------------------------------------------------------------ */
/* Nomes dos tokens no codigo gerado                                  */
/* ------------------------------------------------------------------ */

static const char *const nomes_simbolos[][2] = {
    { "+", "MAIS" }, { "-", "MENOS" }, { "*", "VEZES" }, { "/", "BARRA" },
    { "%", "PORCENTO" }, { "(", "ABRE_PAR" }, { ")", "FECHA_PAR" },
    { "[", "ABRE_COL" }, { "]", "FECHA_COL" }, { "{", "ABRE_CHAVE" },
    { "}", "FECHA_CHAVE" }, { ";", "PV" }, { ",", "VIRGULA" },
    { ".", "PONTO" }, { ":", "DOIS_PONTOS" }, { "=", "IGUAL" },
    { "==", "IGUAL_IGUAL" }, { "!=", "DIFERENTE" }, { "<", "MENOR" },
    { ">", "MAIOR" }, { "<=", "MENOR_IGUAL" }, { ">=", "MAIOR_IGUAL" },
    { "&&", "E_LOGICO" }, { "||", "OU_LOGICO" }, { "!", "NAO" },
    { ":=", "ATRIBUICAO" }, { "->", "SETA" }
};

static int eh_palavra(const char *s)
{
    if (!isalpha((unsigned char) *s) && *s != '_')
        return 0;
    for (; *s != '\0'; s++)
        if (!isalnum((unsigned char) *s) && *s != '_')
            return 0;
    return 1;
}

static int nome_em_uso(const Gramatica *g, int ate, const char *nome)
{
    int i;
    for (i = 0; i < ate; i++)
        if (strcmp(g->terms[i].nome_c, nome) == 0)
            return 1;
    return 0;
}

/* Define g->terms[i].nome_c para todos os terminais. */
static void nomear_terminais(Gramatica *g, int microc)
{
    int i;
    size_t k;
    char buf[256];
    Terminal *t;

    for (i = 0; i < g->nterms; i++) {
        t = &g->terms[i];
        free(t->nome_c);
        if (i == TERM_FIM) {
            t->nome_c = xstrdup(microc ? "END_OF_FILE" : "T_FIM");
            continue;
        }
        if (microc) {
            t->nome_c = xstrdup(t->literal ? microc_nome_literal(t->nome) : t->nome);
            continue;
        }
        buf[0] = '\0';
        if (!t->literal) {
            sprintf(buf, "T_%.200s", t->nome);
        } else if (eh_palavra(t->nome) && strlen(t->nome) < 200) {
            sprintf(buf, "T_%s", t->nome);
            for (k = 2; buf[k] != '\0'; k++)
                buf[k] = (char) toupper((unsigned char) buf[k]);
        } else {
            for (k = 0; k < sizeof(nomes_simbolos) / sizeof(nomes_simbolos[0]); k++)
                if (strcmp(nomes_simbolos[k][0], t->nome) == 0)
                    sprintf(buf, "T_%s", nomes_simbolos[k][1]);
            if (buf[0] == '\0')
                sprintf(buf, "T_LIT_%d", i);
        }
        if (nome_em_uso(g, i, buf))
            sprintf(buf + strlen(buf), "_%d", i);
        t->nome_c = xstrdup(buf);
    }
}

/* ------------------------------------------------------------------ */
/* Conjuntos constantes                                               */
/* ------------------------------------------------------------------ */

#define MAX_COMPARACOES 3   /* ate 3 tokens: tok.tipo == A || ...; acima: pertence */

static Bitset *conjuntos = NULL;
static int nconj = 0, capconj = 0;

static int bitset_igual(const Bitset *a, const Bitset *b)
{
    int i;
    for (i = 0; i < a->n; i++)
        if (bitset_tem(a, i) != bitset_tem(b, i))
            return 0;
    return 1;
}

/* Registra um conjunto e devolve seu numero N (emitido como conj_N).
 * Conjuntos iguais compartilham a mesma constante. */
static int registrar_conjunto(const Bitset *b)
{
    int i;
    for (i = 0; i < nconj; i++)
        if (bitset_igual(&conjuntos[i], b))
            return i;
    if (nconj == capconj) {
        capconj = capconj ? 2 * capconj : 16;
        conjuntos = (Bitset *) xrealloc(conjuntos, (size_t) capconj * sizeof(Bitset));
    }
    bitset_iniciar(&conjuntos[nconj], b->n);
    bitset_copiar(&conjuntos[nconj], b);
    return nconj++;
}

static void liberar_conjuntos(void)
{
    int i;
    for (i = 0; i < nconj; i++)
        bitset_liberar(&conjuntos[i]);
    free(conjuntos);
    conjuntos = NULL;
    nconj = capconj = 0;
}

static void emitir_conjuntos(FILE *f, const Gramatica *g)
{
    int i, t;
    for (i = 0; i < nconj; i++) {
        fprintf(f, "static const int conj_%d[] = { ", i);
        for (t = bitset_proximo(&conjuntos[i], 0); t >= 0;
             t = bitset_proximo(&conjuntos[i], t + 1))
            fprintf(f, "%s, ", g->terms[t].nome_c);
        fputs("-1 };\n", f);
    }
}

/* Escreve a condicao "o token corrente pertence a b":
 *   tok.tipo == T_A                     (um token)
 *   tok.tipo == T_A || tok.tipo == T_B  (ate MAX_COMPARACOES)
 *   pertence(tok.tipo, conj_N)          (mais tokens) */
static void emitir_teste(FILE *f, const Gramatica *g, const Bitset *b)
{
    int t, n = bitset_tamanho(b), primeiro = 1;
    if (n == 0) {
        fputs("0", f);
    } else if (n <= MAX_COMPARACOES) {
        for (t = bitset_proximo(b, 0); t >= 0; t = bitset_proximo(b, t + 1)) {
            fprintf(f, "%stok.tipo == %s", primeiro ? "" : " || ", g->terms[t].nome_c);
            primeiro = 0;
        }
    } else {
        fprintf(f, "pertence(tok.tipo, conj_%d)", registrar_conjunto(b));
    }
}

/* Linha "<antes><teste><depois>", ex.: emit_teste(em, "while (", g, b, ") {") */
static void emit_teste(Emissor *em, const char *antes, const Gramatica *g,
                       const Bitset *b, const char *depois)
{
    int i;
    for (i = 0; i < em->nivel; i++)
        fputs("    ", em->f);
    fputs(antes, em->f);
    emitir_teste(em->f, g, b);
    fputs(depois, em->f);
    fputc('\n', em->f);
}

/* Linha "erro_sintatico(conj_N);" com N = conjunto dos tokens esperados. */
static void emit_erro(Emissor *em, const Bitset *esperados)
{
    emit(em, "erro_sintatico(conj_%d);", registrar_conjunto(esperados));
}

/* ------------------------------------------------------------------ */
/* Traducao das expressoes (esquema da secao 4 do enunciado)          */
/* ------------------------------------------------------------------ */

static void gen_expr(Emissor *em, const Gramatica *g, const Expr *e)
{
    int i, primeiro = 1;
    const Expr *anulavel = NULL;

    switch (e->tipo) {
    case E_TERM:
        emit(em, "match(%s);", g->terms[e->simbolo].nome_c);
        break;
    case E_NAOTERM:
        emit(em, "parse_%s();", g->nts[e->simbolo].nome);
        break;
    case E_VAZIO:
        break;
    case E_SEQ:
        /* a1 a2 ... an: o codigo de cada filho, em ordem */
        for (i = 0; i < e->nfilhos; i++)
            gen_expr(em, g, e->filhos[i]);
        break;
    case E_ALT:
        /* a1 | ... | an: escolhe a alternativa pelo token corrente.
         * A alternativa anulavel (a condicao LL(1) garante no maximo
         * uma) nao tem teste: fica no else final, no lugar do erro. */
        for (i = 0; i < e->nfilhos; i++) {
            if (e->filhos[i]->anulavel) {
                anulavel = e->filhos[i];
                continue;
            }
            emit_teste(em, primeiro ? "if (" : "} else if (", g, &e->filhos[i]->first,
                       ") {");
            primeiro = 0;
            em->nivel++;
            gen_expr(em, g, e->filhos[i]);
            em->nivel--;
        }
        if (primeiro) {
            /* so ha a alternativa anulavel (ex.: "a = | ;") */
            if (anulavel != NULL)
                gen_expr(em, g, anulavel);
            break;
        }
        if (anulavel == NULL) {
            emit(em, "} else {");
            em->nivel++;
            emit_erro(em, &e->first);
            em->nivel--;
        } else if (anulavel->tipo != E_VAZIO) {
            emit(em, "} else {");
            em->nivel++;
            gen_expr(em, g, anulavel);
            em->nivel--;
        }
        emit(em, "}");
        break;
    case E_OPC:
        /* [ a ]: entra se o token corrente pode iniciar a */
        emit_teste(em, "if (", g, &e->filhos[0]->first, ") {");
        em->nivel++;
        gen_expr(em, g, e->filhos[0]);
        em->nivel--;
        emit(em, "}");
        break;
    case E_REP:
        /* { a }: repete enquanto o token corrente puder iniciar a */
        emit_teste(em, "while (", g, &e->filhos[0]->first, ") {");
        em->nivel++;
        gen_expr(em, g, e->filhos[0]);
        em->nivel--;
        emit(em, "}");
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Expressoes regulares de TOKENS (modo proprio)                      */
/* ------------------------------------------------------------------ */

/*
 * Cada expressao regular passa por tres fases:
 *
 *   1. analise + AFN: um analisador descendente recursivo le a
 *      expressao e monta, ao mesmo tempo, o automato finito nao
 *      deterministico pela construcao de Thompson;
 *   2. AFD: construcao de subconjuntos (cada estado do AFD e um
 *      conjunto de estados do AFN);
 *   3. emissao: o AFD vira uma funcao C com um switch sobre o estado.
 *
 * Gramatica aceita:
 *
 *   alt    = concat { "|" concat } ;
 *   concat = { repet } ;
 *   repet  = atomo { "*" | "+" | "?" } ;
 *   atomo  = "(" alt ")" | "[" classe "]" | "." | "\" c | c ;
 *
 * Classes: [abc], intervalos [a-z], negacao [^...]. Escapes: \n \t \r
 * \f \v, \d (digito), \w (letra, digito ou _), \s (espaco em branco) e
 * \c para qualquer outro caractere c, que vale por ele mesmo (\/ \\ \.).
 * O ponto casa com qualquer caractere menos a quebra de linha. O
 * caractere '\0' marca o fim da entrada e nunca e casado.
 */

#define NCHARS 256

/* Estado do AFN de Thompson: ou tem uma transicao por caractere
 * (classe != NULL, para o estado prox) ou ate duas transicoes vazias. */
typedef struct {
    unsigned char *classe;      /* classe[c] != 0 se c dispara a transicao */
    int prox;
    int eps[2];
    int neps;
} EstadoAfn;

typedef struct {
    const char *re;             /* texto da expressao */
    size_t pos;                 /* posicao de leitura */
    EstadoAfn *est;
    int n, cap;
    const char *erro;           /* primeira mensagem de erro, ou NULL */
} Regex;

/* Fragmento de AFN: um estado inicial e um final. O estado final de um
 * fragmento nunca tem transicoes de saida. */
typedef struct {
    int ini, fim;
} Frag;

/* AFD: trans[e * NCHARS + c] e o destino de e com c, ou -1. O estado
 * inicial e o 0. */
typedef struct {
    int n;
    int *trans;
    char *finais;
} Afd;

/* Posicao no .ebnf da expressao em analise, para as mensagens. */
static int regex_linha = 0, regex_coluna = 0;

static int afn_estado(Regex *r)
{
    EstadoAfn *e;
    if (r->n == r->cap) {
        r->cap = r->cap ? 2 * r->cap : 32;
        r->est = (EstadoAfn *) xrealloc(r->est, (size_t) r->cap * sizeof(EstadoAfn));
    }
    e = &r->est[r->n];
    e->classe = NULL;
    e->prox = -1;
    e->neps = 0;
    return r->n++;
}

static void afn_eps(Regex *r, int de, int para)
{
    r->est[de].eps[r->est[de].neps++] = para;
}

static void regex_erro(Regex *r, const char *msg)
{
    if (r->erro == NULL)
        r->erro = msg;
}

static int re_atual(const Regex *r)
{
    return (unsigned char) r->re[r->pos];
}

/* Le o que vem depois de uma barra invertida e acrescenta a conj os
 * caracteres correspondentes. Devolve o caractere, ou -1 se o escape
 * representar varios (\d, \w, \s). */
static int re_escape(Regex *r, unsigned char *conj)
{
    int c = re_atual(r), i;
    if (c == '\0') {
        regex_erro(r, "'\\' no fim da expressao");
        return -1;
    }
    r->pos++;
    switch (c) {
    case 'n': c = '\n'; break;
    case 't': c = '\t'; break;
    case 'r': c = '\r'; break;
    case 'f': c = '\f'; break;
    case 'v': c = '\v'; break;
    case 'd':
        for (i = '0'; i <= '9'; i++)
            conj[i] = 1;
        return -1;
    case 'w':
        for (i = 1; i < NCHARS; i++)
            if (isalnum(i) || i == '_')
                conj[i] = 1;
        return -1;
    case 's':
        conj[' '] = conj['\t'] = conj['\n'] = conj['\r'] = conj['\f'] = conj['\v'] = 1;
        return -1;
    default:
        break;
    }
    conj[c] = 1;
    return c;
}

/* Le um elemento de classe (caractere ou escape). Devolve o caractere,
 * ou -1 para escapes de varios caracteres. */
static int re_item_classe(Regex *r, unsigned char *conj)
{
    int c = re_atual(r);
    r->pos++;
    if (c == '\\')
        return re_escape(r, conj);
    conj[c] = 1;
    return c;
}

/* classe = [ "^" ] item { item } ;  item = c | c "-" c ;
 * Chamada depois do '['. Um ']' logo no inicio vale por ele mesmo. */
static void re_classe(Regex *r, unsigned char *conj)
{
    int negada = 0, primeiro = 1, lo, hi, i;
    unsigned char descarte[NCHARS];

    if (re_atual(r) == '^') {
        negada = 1;
        r->pos++;
    }
    while (r->erro == NULL) {
        if (re_atual(r) == '\0') {
            regex_erro(r, "falta ']' para fechar a classe");
            return;
        }
        if (re_atual(r) == ']' && !primeiro)
            break;
        primeiro = 0;
        lo = re_item_classe(r, conj);
        if (re_atual(r) == '-' && r->re[r->pos + 1] != ']' && r->re[r->pos + 1] != '\0') {
            r->pos++;
            memset(descarte, 0, sizeof descarte);
            hi = re_item_classe(r, descarte);
            if (lo < 0 || hi < 0) {
                regex_erro(r, "intervalo invalido em classe");
                return;
            }
            if (hi < lo) {
                regex_erro(r, "intervalo invertido em classe (ex.: [z-a])");
                return;
            }
            for (i = lo; i <= hi; i++)
                conj[i] = 1;
        }
    }
    if (r->erro != NULL)
        return;
    r->pos++;                           /* ']' */
    if (negada)
        for (i = 1; i < NCHARS; i++)
            conj[i] = (unsigned char) !conj[i];
    conj[0] = 0;
}

static Frag re_alt(Regex *r);

/* atomo = "(" alt ")" | "[" classe "]" | "." | "\" c | c ; */
static Frag re_atomo(Regex *r)
{
    Frag f;
    unsigned char *conj;
    int c = re_atual(r), i;

    if (c == '(') {
        r->pos++;
        f = re_alt(r);
        if (re_atual(r) == ')')
            r->pos++;
        else
            regex_erro(r, "falta ')'");
        return f;
    }
    /* os demais atomos sao uma transicao por um conjunto de caracteres */
    conj = (unsigned char *) xcalloc(NCHARS, 1);
    r->pos++;
    if (c == '[') {
        re_classe(r, conj);
    } else if (c == '.') {
        for (i = 1; i < NCHARS; i++)
            conj[i] = (unsigned char) (i != '\n');
    } else if (c == '\\') {
        re_escape(r, conj);
    } else if (c == '*' || c == '+' || c == '?') {
        regex_erro(r, "operador de repeticao sem operando");
    } else {
        conj[c] = 1;
    }
    f.ini = afn_estado(r);
    f.fim = afn_estado(r);
    r->est[f.ini].classe = conj;
    r->est[f.ini].prox = f.fim;
    return f;
}

/* repet = atomo { "*" | "+" | "?" } ; */
static Frag re_repet(Regex *r)
{
    Frag a = re_atomo(r), f;
    int op;

    while (r->erro == NULL && ((op = re_atual(r)) == '*' || op == '+' || op == '?')) {
        r->pos++;
        f.ini = afn_estado(r);
        f.fim = afn_estado(r);
        afn_eps(r, f.ini, a.ini);
        if (op != '+')
            afn_eps(r, f.ini, f.fim);   /* zero ocorrencias */
        if (op != '?')
            afn_eps(r, a.fim, a.ini);   /* repete */
        afn_eps(r, a.fim, f.fim);
        a = f;
    }
    return a;
}

/* concat = { repet } ; */
static Frag re_concat(Regex *r)
{
    Frag f, g;
    int c;

    f.ini = f.fim = afn_estado(r);      /* cadeia vazia */
    while (r->erro == NULL && (c = re_atual(r)) != '\0' && c != '|' && c != ')') {
        g = re_repet(r);
        afn_eps(r, f.fim, g.ini);
        f.fim = g.fim;
    }
    return f;
}

/* alt = concat { "|" concat } ; */
static Frag re_alt(Regex *r)
{
    Frag f = re_concat(r), g, u;

    while (r->erro == NULL && re_atual(r) == '|') {
        r->pos++;
        g = re_concat(r);
        u.ini = afn_estado(r);
        u.fim = afn_estado(r);
        afn_eps(r, u.ini, f.ini);
        afn_eps(r, u.ini, g.ini);
        afn_eps(r, f.fim, u.fim);
        afn_eps(r, g.fim, u.fim);
        f = u;
    }
    return f;
}

/* Fecho-epsilon: acrescenta a conj tudo o que e alcancavel por
 * transicoes vazias. pilha tem espaco para todos os estados do AFN. */
static void fecho_eps(const Regex *r, Bitset *conj, int *pilha)
{
    int topo = 0, s, k, d;
    for (s = bitset_proximo(conj, 0); s >= 0; s = bitset_proximo(conj, s + 1))
        pilha[topo++] = s;
    while (topo > 0) {
        s = pilha[--topo];
        for (k = 0; k < r->est[s].neps; k++) {
            d = r->est[s].eps[k];
            if (!bitset_tem(conj, d)) {
                bitset_add(conj, d);
                pilha[topo++] = d;
            }
        }
    }
}

/* Construcao de subconjuntos. aceita e o estado final do AFN. */
static Afd *afd_construir(const Regex *r, Frag afn)
{
    Afd *d = (Afd *) xmalloc(sizeof(Afd));
    Bitset *conjs = NULL, alvo;
    int cap = 0, n = 0, i, c, s, k, *pilha;

    pilha = (int *) xmalloc((size_t) r->n * sizeof(int));
    bitset_iniciar(&alvo, r->n);
    d->trans = NULL;
    d->finais = NULL;

    /* estado 0: fecho do estado inicial do AFN */
    cap = 16;
    conjs = (Bitset *) xmalloc((size_t) cap * sizeof(Bitset));
    bitset_iniciar(&conjs[0], r->n);
    bitset_add(&conjs[0], afn.ini);
    fecho_eps(r, &conjs[0], pilha);
    n = 1;

    for (i = 0; i < n; i++) {
        d->trans = (int *) xrealloc(d->trans, (size_t) (i + 1) * NCHARS * sizeof(int));
        d->trans[i * NCHARS] = -1;      /* '\0' nunca e casado */
        for (c = 1; c < NCHARS; c++) {
            bitset_limpar(&alvo);
            for (s = bitset_proximo(&conjs[i], 0); s >= 0;
                 s = bitset_proximo(&conjs[i], s + 1))
                if (r->est[s].classe != NULL && r->est[s].classe[c])
                    bitset_add(&alvo, r->est[s].prox);
            if (bitset_vazio(&alvo)) {
                d->trans[i * NCHARS + c] = -1;
                continue;
            }
            fecho_eps(r, &alvo, pilha);
            for (k = 0; k < n; k++)
                if (bitset_igual(&conjs[k], &alvo))
                    break;
            if (k == n) {               /* estado novo */
                if (n == cap) {
                    cap *= 2;
                    conjs = (Bitset *) xrealloc(conjs, (size_t) cap * sizeof(Bitset));
                }
                bitset_iniciar(&conjs[n], r->n);
                bitset_copiar(&conjs[n], &alvo);
                n++;
            }
            d->trans[i * NCHARS + c] = k;
        }
    }

    d->n = n;
    d->finais = (char *) xmalloc((size_t) n);
    for (i = 0; i < n; i++) {
        d->finais[i] = (char) bitset_tem(&conjs[i], afn.fim);
        bitset_liberar(&conjs[i]);
    }
    free(conjs);
    free(pilha);
    bitset_liberar(&alvo);
    return d;
}

static void afd_liberar(Afd *d)
{
    if (d == NULL)
        return;
    free(d->trans);
    free(d->finais);
    free(d);
}

/* Converte a expressao regular em AFD. Em caso de erro reporta-o e
 * devolve NULL. */
static Afd *regex_compilar(const char *regex)
{
    Regex r;
    Frag afn;
    Afd *d = NULL;
    int i;

    r.re = regex;
    r.pos = 0;
    r.est = NULL;
    r.n = r.cap = 0;
    r.erro = NULL;

    afn = re_alt(&r);
    if (r.erro == NULL && re_atual(&r) == ')')
        regex_erro(&r, "')' sem '(' correspondente");
    if (r.erro != NULL)
        diag_erro(regex_linha, regex_coluna, "expressao regular /%s/ invalida: %s",
                  regex, r.erro);
    else
        d = afd_construir(&r, afn);

    for (i = 0; i < r.n; i++)
        free(r.est[i].classe);
    free(r.est);
    return d;
}

/* Escreve c como constante C: 'a', '\n' ou o codigo numerico. */
static void emitir_char(FILE *f, int c)
{
    if (c == '\'' || c == '\\')
        fprintf(f, "'\\%c'", c);
    else if (c == '\n')
        fputs("'\\n'", f);
    else if (c == '\t')
        fputs("'\\t'", f);
    else if (c == '\r')
        fputs("'\\r'", f);
    else if (c >= 32 && c <= 126)
        fprintf(f, "'%c'", c);
    else
        fprintf(f, "%d", c);
}

/* Escreve o teste "c esta em [lo, hi]". c e unsigned char: comparar
 * com 255 seria sempre verdadeiro (e geraria aviso), entao e omitido. */
static void emitir_faixa(FILE *f, int lo, int hi)
{
    if (lo == hi) {
        fputs("c == ", f);
        emitir_char(f, lo);
    } else if (hi == NCHARS - 1) {
        fputs("c >= ", f);
        emitir_char(f, lo);
    } else {
        fputs("(c >= ", f);
        emitir_char(f, lo);
        fputs(" && c <= ", f);
        emitir_char(f, hi);
        fputc(')', f);
    }
}

/* Emite as transicoes do estado e: um if por estado de destino, com as
 * faixas de caracteres que levam a ele. */
static void emitir_transicoes(FILE *f, const Afd *d, int e)
{
    const int *t = d->trans + e * NCHARS;
    int destino, c, lo, primeiro_if = 1, primeira_faixa;

    for (destino = 0; destino < d->n; destino++) {
        primeira_faixa = 1;
        for (c = 1; c < NCHARS; c++) {
            if (t[c] != destino)
                continue;
            lo = c;
            while (c + 1 < NCHARS && t[c + 1] == destino)
                c++;
            if (primeira_faixa)
                fprintf(f, "            %sif (", primeiro_if ? "" : "else ");
            else
                fputs(" || ", f);
            emitir_faixa(f, lo, c);
            primeira_faixa = 0;
        }
        if (!primeira_faixa) {
            fprintf(f, ")\n                estado = %d;\n", destino);
            primeiro_if = 0;
        }
    }
    if (primeiro_if) {
        fputs("            return ultimo;\n", f);
    } else {
        fputs("            else\n                return ultimo;\n", f);
        fputs("            break;\n", f);
    }
}

/* Emite a funcao
 *     static size_t <nome>(const char *s)
 * que devolve o comprimento do MAIOR prefixo de s reconhecido pela
 * expressao regular, ou 0 se nenhum prefixo for reconhecido.
 * Retorna 0 se a expressao for valida (senao o erro e reportado).
 *
 * A funcao gerada simula o AFD: avanca enquanto houver transicao e
 * guarda em "ultimo" a posicao da ultima passagem por um estado final. */
static int gen_casamento_regex(FILE *f, const char *nome, const char *regex)
{
    Afd *d = regex_compilar(regex);
    int e, c, tem_transicao = 0;

    if (d == NULL)
        return -1;
    for (e = 0; e < d->n && !tem_transicao; e++)
        for (c = 1; c < NCHARS; c++)
            if (d->trans[e * NCHARS + c] >= 0) {
                tem_transicao = 1;
                break;
            }

    fprintf(f, "static size_t %s(const char *s)\n{\n", nome);
    if (!tem_transicao) {
        /* a expressao so reconhece a cadeia vazia */
        fputs("    (void) s;\n    return 0;\n}\n\n", f);
        afd_liberar(d);
        return 0;
    }
    fputs("    static const char finais[] = { ", f);
    for (e = 0; e < d->n; e++)
        fprintf(f, "%s%d", e > 0 ? ", " : "", d->finais[e]);
    fputs(" };\n", f);
    fputs("    size_t i = 0, ultimo = 0;\n    int estado = 0;\n    unsigned char c;\n\n", f);
    fputs("    for (;;) {\n        c = (unsigned char) s[i];\n        switch (estado) {\n", f);
    for (e = 0; e < d->n; e++) {
        fprintf(f, "        case %d:\n", e);
        emitir_transicoes(f, d, e);
    }
    fputs("        default:\n            return ultimo;\n        }\n", f);
    fputs("        i++;\n        if (finais[estado])\n            ultimo = i;\n    }\n}\n\n", f);
    afd_liberar(d);
    return 0;
}

/* Confere todas as expressoes regulares antes de qualquer arquivo ser
 * escrito, para que um erro nao deixe arquivos pela metade. Devolve o
 * numero de expressoes invalidas. */
static int validar_regexes(const Gramatica *g)
{
    int i, erros = 0;
    Afd *d;

    regex_linha = regex_coluna = 0;     /* SKIP nao guarda a posicao */
    for (i = 0; i < g->nskips; i++) {
        d = regex_compilar(g->skips[i]);
        if (d == NULL)
            erros++;
        afd_liberar(d);
    }
    for (i = 1; i < g->nterms; i++) {
        if (g->terms[i].literal || g->terms[i].regex == NULL)
            continue;
        regex_linha = g->terms[i].linha;
        regex_coluna = g->terms[i].coluna;
        d = regex_compilar(g->terms[i].regex);
        if (d == NULL)
            erros++;
        else if (d->finais[0])
            diag_aviso(regex_linha, regex_coluna, "a expressao regular de %s aceita a "
                       "cadeia vazia; o token so e reconhecido com ao menos um caractere",
                       g->terms[i].nome);
        afd_liberar(d);
    }
    regex_linha = regex_coluna = 0;
    return erros;
}

/* ------------------------------------------------------------------ */
/* Leitura do arquivo de tokens (opcao --tokens do analisador gerado) */
/* ------------------------------------------------------------------ */

/* Emite "static Token token_de_arquivo(void)", que le o proximo token
 * do arquivo aberto em arquivo_tokens, no formato gravado pelo lexer do
 * TP1 (livro-texto, secao 3.3.2):
 *
 *   Token: tipo = <numero>, lexema = '<texto>', linha = <numero>
 *
 * Ao chegar ao fim do arquivo devolve um token TOKEN_FIM.
 *
 * O lexema pode conter aspas, virgulas e ate quebras de linha (uma
 * string com \n ja convertido). Por isso o fim do lexema e procurado de
 * tras para a frente, no ultimo "linha =" da linha; se ele nao existir,
 * a linha seguinte do arquivo e emendada e a busca e repetida. */
static void gen_token_de_arquivo(FILE *f, const Gramatica *g, int microc)
{
    static const char *const inicio[] = {
        "/* ---- leitura do arquivo de tokens (--tokens) ---- */",
        "",
        "static void erro_arquivo_tokens(int linha_arq)",
        "{",
        "    fprintf(stderr, \"arquivo de tokens, linha %d: linha malformada (esperado: \"",
        "            \"Token: tipo = N, lexema = '...', linha = N)\\n\", linha_arq);",
        "    exit(EXIT_FAILURE);",
        "}",
        "",
        "static Token token_de_arquivo(void)",
        "{",
        "    static int linha_arq = 0;       /* linha corrente do arquivo de tokens */",
        "    static int ultima_linha = 1;    /* linha do ultimo token lido */",
        "    Token t;",
        "    char buf[2048], *lex, *fim, *p, *q;",
        "    int tipo, linha_tok = 0, n, inicio_tok;",
        "    size_t tam;",
        "",
        "    memset(&t, 0, sizeof t);",
        "    do {                            /* ignora linhas em branco */",
        "        if (fgets(buf, sizeof buf, arquivo_tokens) == NULL) {",
        "            t.tipo = TOKEN_FIM;",
        "            t.linha = ultima_linha;",
        "            return t;",
        "        }",
        "        linha_arq++;",
        "    } while (buf[strspn(buf, \" \\t\\r\\n\")] == '\\0');",
        "    inicio_tok = linha_arq;",
        "",
        "    n = 0;",
        "    if (sscanf(buf, \" Token: tipo = %d%n\", &tipo, &n) != 1 || n == 0)",
        "        erro_arquivo_tokens(inicio_tok);",
        "    lex = strstr(buf + n, \"lexema = '\");",
        "    if (lex == NULL)",
        "        erro_arquivo_tokens(inicio_tok);",
        "    lex += strlen(\"lexema = '\");",
        "",
        "    for (;;) {",
        "        /* p = ultimo \"linha =\" depois do inicio do lexema */",
        "        p = NULL;",
        "        for (q = strstr(lex, \"linha =\"); q != NULL; q = strstr(q + 1, \"linha =\"))",
        "            p = q;",
        "        fim = NULL;",
        "        if (p != NULL && sscanf(p, \"linha = %d\", &linha_tok) == 1) {",
        "            /* antes dele: aspa de fechamento, virgula opcional, espacos */",
        "            q = p;",
        "            while (q > lex && (q[-1] == ' ' || q[-1] == ','))",
        "                q--;",
        "            if (q > lex && q[-1] == '\\'')",
        "                fim = q - 1;",
        "        }",
        "        if (fim != NULL)",
        "            break;",
        "        /* lexema com quebra de linha: emenda a linha seguinte */",
        "        tam = strlen(buf);",
        "        if (tam + 1 >= sizeof buf",
        "            || fgets(buf + tam, (int) (sizeof buf - tam), arquivo_tokens) == NULL)",
        "            erro_arquivo_tokens(inicio_tok);",
        "        linha_arq++;",
        "    }",
        "",
        "    tam = (size_t) (fim - lex);",
        "    if (tam >= sizeof t.lexema)",
        "        tam = sizeof t.lexema - 1;",
        "    memcpy(t.lexema, lex, tam);",
        "    t.lexema[tam] = '\\0';",
        "    t.linha = linha_tok;",
        "    ultima_linha = linha_tok;",
        NULL
    };
    int i, ultimo;

    emit_linhas(f, inicio);
    if (microc) {
        /* O TP1 permite devolver tokens de um caractere pelo codigo ASCII.
         * Valores ate o ultimo item de TokenType sao sempre TokenType;
         * acima disso, o codigo ASCII e trocado pelo nome do token. */
        ultimo = microc_num_tokens() - 1;
        if (ultimo >= 0) {
            fprintf(f, "    if (tipo > %d) {    /* fora de TokenType: codigo ASCII */\n",
                    ultimo);
            fputs("        switch (tipo) {\n", f);
            for (i = 1; i < g->nterms; i++) {
                const Terminal *t = &g->terms[i];
                if (!t->literal || strlen(t->nome) != 1
                    || (int) (unsigned char) t->nome[0] <= ultimo)
                    continue;
                fputs("        case ", f);
                emitir_char(f, (unsigned char) t->nome[0]);
                fprintf(f, ": tipo = %s; break;\n", t->nome_c);
            }
            fputs("        default: break;\n        }\n    }\n", f);
        }
        fputs("    t.tipo = (TokenType) tipo;\n", f);
    } else {
        fputs("    t.tipo = tipo;\n", f);
        fputs("    t.coluna = 0;                   /* o arquivo nao guarda a coluna */\n", f);
    }
    fputs("    return t;\n}\n\n", f);
}

/* ------------------------------------------------------------------ */
/* Partes fixas do codigo gerado                                      */
/* ------------------------------------------------------------------ */

static const char *const rt_estado[] = {
    "/* ---- estado do analisador ---- */",
    "",
    "static Token tok;                  /* token corrente (lookahead) */",
    "static int imprimir_arvore = 0;    /* opcao --tree */",
    "static int nivel_arvore = 0;",
    "static FILE *arquivo_tokens = NULL; /* opcao --tokens */",
    "",
    "static Token token_de_arquivo(void);",
    "",
    NULL
};

static const char *const rt_primitivas[] = {
    "/* ---- primitivas ---- */",
    "",
    "int pertence(int tipo, const int *conjunto)",
    "{",
    "    for (; *conjunto != -1; conjunto++)",
    "        if (*conjunto == tipo)",
    "            return 1;",
    "    return 0;",
    "}",
    "",
    "/* Reporta o erro com os tokens esperados (lista terminada em -1) e encerra. */",
    "void erro_sintatico(const int *esperados)",
    "{",
    "    int i;",
    "    escrever_posicao();",
    "    if ((int) tok.tipo == TOKEN_FIM)",
    "        fprintf(stderr, \"encontrado fim de arquivo, \");",
    "    else",
    "        fprintf(stderr, \"encontrado '%s', \", tok.lexema);",
    "    if (esperados[0] != -1 && esperados[1] == -1) {",
    "        fprintf(stderr, \"esperado %s\\n\", nome_token(esperados[0]));",
    "    } else {",
    "        fprintf(stderr, \"esperado um de {\");",
    "        for (i = 0; esperados[i] != -1; i++)",
    "            fprintf(stderr, \"%s%s\", i > 0 ? \", \" : \"\", nome_token(esperados[i]));",
    "        fprintf(stderr, \"}\\n\");",
    "    }",
    "    exit(EXIT_FAILURE);",
    "}",
    "",
    "static void recuo(void)",
    "{",
    "    int i;",
    "    for (i = 0; i < nivel_arvore; i++)",
    "        fputs(\"  \", stdout);",
    "}",
    "",
    "void arvore_entra(const char *nao_terminal)",
    "{",
    "    if (imprimir_arvore) {",
    "        recuo();",
    "        printf(\"%s\\n\", nao_terminal);",
    "    }",
    "    nivel_arvore++;",
    "}",
    "",
    "void arvore_sai(void)",
    "{",
    "    nivel_arvore--;",
    "}",
    "",
    "/* Consome o token corrente se for do tipo esperado; senao, erro. */",
    "void match(int esperado)",
    "{",
    "    int lista[2];",
    "    if ((int) tok.tipo != esperado) {",
    "        lista[0] = esperado;",
    "        lista[1] = -1;",
    "        erro_sintatico(lista);",
    "    }",
    "    if (SUPORTE_ARVORE && imprimir_arvore) {",
    "        recuo();",
    "        if (nome_token(tok.tipo)[0] == '\\'')",
    "            printf(\"%s\\n\", nome_token(tok.tipo));",
    "        else",
    "            printf(\"%s '%s'\\n\", nome_token(tok.tipo), tok.lexema);",
    "    }",
    "    avancar();",
    "}",
    "",
    NULL
};

/* Lexico proprio: partes fixas antes e depois das funcoes de regex. */
static const char *const rt_lexico_estado[] = {
    "/* ---- analisador lexico gerado a partir de TOKENS ---- */",
    "",
    "static char *fonte = NULL;",
    "static size_t pos = 0;",
    "static int linha = 1, coluna = 1;",
    "",
    "static char *ler_tudo(FILE *f)",
    "{",
    "    size_t cap = 4096, n = 0, lidos;",
    "    char *buf = (char *) malloc(cap), *novo;",
    "    if (buf == NULL) {",
    "        fprintf(stderr, \"memoria insuficiente\\n\");",
    "        exit(EXIT_FAILURE);",
    "    }",
    "    while ((lidos = fread(buf + n, 1, cap - n - 1, f)) > 0) {",
    "        n += lidos;",
    "        if (cap - n - 1 == 0) {",
    "            cap *= 2;",
    "            novo = (char *) realloc(buf, cap);",
    "            if (novo == NULL) {",
    "                fprintf(stderr, \"memoria insuficiente\\n\");",
    "                exit(EXIT_FAILURE);",
    "            }",
    "            buf = novo;",
    "        }",
    "    }",
    "    buf[n] = '\\0';",
    "    return buf;",
    "}",
    "",
    "static void consumir(size_t n)",
    "{",
    "    for (; n > 0 && fonte[pos] != '\\0'; n--, pos++) {",
    "        if (fonte[pos] == '\\n') {",
    "            linha++;",
    "            coluna = 1;",
    "        } else {",
    "            coluna++;",
    "        }",
    "    }",
    "}",
    "",
    NULL
};

static void escrever_cabecalho_c(FILE *f, const OpcoesGeracao *op)
{
    fputs("/*\n * Analisador sintatico descendente recursivo gerado a partir de ", f);
    emitir_comentario(f, op->origem);
    fputs(".\n * Arquivo gerado automaticamente: nao edite.\n */\n", f);
}

/* ------------------------------------------------------------------ */
/* Arquivo .h                                                         */
/* ------------------------------------------------------------------ */

static int gerar_h(const Gramatica *g, const OpcoesGeracao *op)
{
    char *caminho = (char *) xmalloc(strlen(op->prefixo) + 3);
    FILE *f;
    int i;

    sprintf(caminho, "%s.h", op->prefixo);
    f = fopen(caminho, "w");
    if (f == NULL) {
        diag_erro(0, 0, "nao foi possivel criar %s", caminho);
        free(caminho);
        return -1;
    }
    escrever_cabecalho_c(f, op);
    fputs("#ifndef PARSER_GERADO_H\n#define PARSER_GERADO_H\n\n", f);
    if (op->microc) {
        fputs("#include \"scanner.h\"   /* scanner do TP1: Token, TokenType, proximo_token */\n\n", f);
        fputs("#define TOKEN_FIM END_OF_FILE\n\n", f);
    } else {
        fputs("typedef enum {\n", f);
        for (i = 0; i < g->nterms; i++) {
            fprintf(f, "    %s%s   /* ", g->terms[i].nome_c, i + 1 < g->nterms ? "," : "");
            emitir_comentario(f, g->terms[i].exibicao);
            fputs(" */\n", f);
        }
        fputs("} TipoToken;\n\n", f);
        fputs("typedef struct {\n    int tipo;\n    char lexema[100];\n"
              "    int linha;\n    int coluna;\n} Token;\n\n", f);
        fputs("#define TOKEN_FIM T_FIM\n\n", f);
    }
    fputs("const char *nome_token(int tipo);\n", f);
    fputs("int  pertence(int tipo, const int *conjunto);\n", f);
    fputs("void avancar(void);\n", f);
    fputs("void match(int esperado);\n", f);
    fputs("void erro_sintatico(const int *esperados);\n", f);
    fputs("void arvore_entra(const char *nao_terminal);\n", f);
    fputs("void arvore_sai(void);\n\n", f);
    for (i = 0; i < g->nnts; i++)
        if (g->nts[i].regra != NULL)
            fprintf(f, "void parse_%s(void);\n", g->nts[i].nome);
    fputs("\n#endif\n", f);
    fclose(f);
    free(caminho);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Arquivo .c                                                         */
/* ------------------------------------------------------------------ */

static void gerar_nome_token(FILE *f, const Gramatica *g)
{
    int i;
    fputs("const char *nome_token(int tipo)\n{\n    switch (tipo) {\n", f);
    for (i = 0; i < g->nterms; i++) {
        fprintf(f, "    case %s: return ", g->terms[i].nome_c);
        emitir_string_c(f, i == TERM_FIM ? "fim de arquivo" : g->terms[i].exibicao);
        fputs(";\n", f);
    }
    fputs("    default: return \"token desconhecido\";\n    }\n}\n\n", f);
}

static void gerar_posicao_e_avancar(FILE *f, int microc)
{
    fputs("static void escrever_posicao(void)\n{\n", f);
    if (microc)
        fputs("    fprintf(stderr, \"linha %d: \", tok.linha);\n", f);
    else
        fputs("    fprintf(stderr, \"linha %d, coluna %d: \", tok.linha, tok.coluna);\n", f);
    fputs("}\n\n", f);

    fputs("/* Le o proximo token para tok. */\nvoid avancar(void)\n{\n", f);
    fputs("    if (arquivo_tokens != NULL)\n        tok = token_de_arquivo();\n    else\n", f);
    if (microc) {
        fputs("        tok = proximo_token();\n", f);
        fputs("    if (tok.tipo == UNDEF) {\n", f);
        fputs("        fprintf(stderr, \"linha %d: erro lexico: %s\\n\", tok.linha, tok.lexema);\n", f);
        fputs("        exit(EXIT_FAILURE);\n    }\n", f);
    } else {
        fputs("        tok = proximo_token_gerado();\n", f);
    }
    fputs("}\n\n", f);
}

static int gerar_lexico(FILE *f, const Gramatica *g)
{
    int i, nlit = 0, erros = 0;
    char nome[300];

    emit_linhas(f, rt_lexico_estado);

    for (i = 0; i < g->nskips; i++) {
        sprintf(nome, "casa_skip_%d", i);
        fputs("/* SKIP: ", f);
        emitir_comentario(f, g->skips[i]);
        fputs(" */\n", f);
        if (gen_casamento_regex(f, nome, g->skips[i]) != 0)
            erros++;
    }
    for (i = 1; i < g->nterms; i++) {
        if (g->terms[i].literal) {
            nlit++;
            continue;
        }
        regex_linha = g->terms[i].linha;
        regex_coluna = g->terms[i].coluna;
        sprintf(nome, "casa_%.250s", g->terms[i].nome_c);
        fprintf(f, "/* %s: ", g->terms[i].nome);
        emitir_comentario(f, g->terms[i].regex ? g->terms[i].regex : "");
        fputs(" */\n", f);
        if (gen_casamento_regex(f, nome, g->terms[i].regex ? g->terms[i].regex : "")
            != 0)
            erros++;
    }

    /* literais: comparacao direta, com prioridade em caso de empate */
    fputs("static const char *const texto_literais[] = {\n", f);
    for (i = 1; i < g->nterms; i++)
        if (g->terms[i].literal) {
            fputs("    ", f);
            emitir_string_c(f, g->terms[i].nome);
            fputs(",\n", f);
        }
    fputs("    NULL\n};\n\nstatic const int tipo_literais[] = {\n", f);
    for (i = 1; i < g->nterms; i++)
        if (g->terms[i].literal)
            fprintf(f, "    %s,\n", g->terms[i].nome_c);
    fputs("    -1\n};\n\n", f);

    fputs("/* Reconhece o maior prefixo da entrada; em caso de empate vence\n"
          " * o literal (palavras reservadas tem prioridade sobre ID). */\n", f);
    fputs("static Token proximo_token_gerado(void)\n{\n", f);
    fputs("    Token t;\n    size_t n, melhor;\n    int i, tipo, pulou;\n\n", f);
    fputs("    do {\n        pulou = 0;\n", f);
    for (i = 0; i < g->nskips; i++)
        fprintf(f, "        if ((n = casa_skip_%d(fonte + pos)) > 0) {\n"
                   "            consumir(n);\n            pulou = 1;\n        }\n", i);
    fputs("    } while (pulou);\n\n", f);
    fputs("    memset(&t, 0, sizeof t);\n", f);
    fputs("    t.linha = linha;\n    t.coluna = coluna;\n", f);
    fputs("    if (fonte[pos] == '\\0') {\n        t.tipo = T_FIM;\n        return t;\n    }\n", f);
    fputs("    melhor = 0;\n    tipo = -1;\n", f);
    fputs("    for (i = 0; texto_literais[i] != NULL; i++) {\n", f);
    fputs("        n = strlen(texto_literais[i]);\n", f);
    fputs("        if (n > melhor && strncmp(fonte + pos, texto_literais[i], n) == 0) {\n", f);
    fputs("            melhor = n;\n            tipo = tipo_literais[i];\n        }\n    }\n", f);
    for (i = 1; i < g->nterms; i++)
        if (!g->terms[i].literal)
            fprintf(f, "    if ((n = casa_%s(fonte + pos)) > melhor) {\n"
                       "        melhor = n;\n        tipo = %s;\n    }\n",
                    g->terms[i].nome_c, g->terms[i].nome_c);
    fputs("    if (melhor == 0) {\n", f);
    fputs("        fprintf(stderr, \"linha %d, coluna %d: erro lexico: caractere "
          "invalido '%c'\\n\",\n                linha, coluna, fonte[pos]);\n", f);
    fputs("        exit(EXIT_FAILURE);\n    }\n", f);
    fputs("    n = melhor < sizeof t.lexema ? melhor : sizeof t.lexema - 1;\n", f);
    fputs("    memcpy(t.lexema, fonte + pos, n);\n    t.lexema[n] = '\\0';\n", f);
    fputs("    t.tipo = tipo;\n    consumir(melhor);\n    return t;\n}\n\n", f);
    (void) nlit;
    return erros;
}

static void gerar_procedimentos(FILE *f, const Gramatica *g, const OpcoesGeracao *op)
{
    int i;
    Emissor em;
    em.f = f;
    em.nivel = 0;

    fputs("/* ---- procedimentos (um por nao terminal) ---- */\n\n", f);
    for (i = 0; i < g->nnts; i++) {
        if (g->nts[i].regra == NULL)
            continue;
        fprintf(f, "/* %s = ", g->nts[i].nome);
        {
            /* a regra em EBNF, sem "* /" */
            FILE *tmp = tmpfile();
            int c;
            if (tmp != NULL) {
                char buf[2] = { 0, 0 };
                expr_imprimir(tmp, g, g->nts[i].regra);
                rewind(tmp);
                while ((c = fgetc(tmp)) != EOF) {
                    buf[0] = (char) c;
                    emitir_comentario(f, buf);
                }
                fclose(tmp);
            }
        }
        fputs(" ; */\n", f);
        fprintf(f, "void parse_%s(void)\n{\n", g->nts[i].nome);
        em.nivel = 1;
        if (op->arvore)
            emit(&em, "arvore_entra(\"%s\");", g->nts[i].nome);
        gen_expr(&em, g, g->nts[i].regra);
        if (op->arvore)
            emit(&em, "arvore_sai();");
        fputs("}\n\n", f);
    }
}

/* O analisador gerado encerra com exit() ao encontrar um erro. Para
 * que nenhuma memoria fique alocada nesse caminho, a liberacao dos
 * recursos e registrada com atexit(): ela roda tanto no retorno normal
 * de main quanto em qualquer exit(). */
static void gerar_main(FILE *f, const Gramatica *g, const OpcoesGeracao *op)
{
    fputs("/* ---- programa principal ---- */\n\n", f);
    fputs("static FILE *entrada = NULL;       /* programa a analisar */\n", f);
    if (op->microc)
        fputs("static int scanner_ativo = 0;\n", f);
    fputs("\n/* Registrada com atexit: roda no fim de main e em todo exit(). */\n", f);
    fputs("static void liberar_recursos(void)\n{\n", f);
    fputs("    if (arquivo_tokens != NULL)\n        fclose(arquivo_tokens);\n", f);
    if (op->microc)
        fputs("    if (scanner_ativo)\n        scanner_finalizar();\n", f);
    else
        fputs("    free(fonte);\n", f);
    fputs("    if (entrada != NULL && entrada != stdin)\n        fclose(entrada);\n", f);
    fputs("}\n\n", f);

    fputs("int main(int argc, char **argv)\n{\n", f);
    fputs("    int i, fim[2];\n    const char *arquivo = NULL, *tokens = NULL;\n\n", f);
    fputs("    for (i = 1; i < argc; i++) {\n", f);
    fputs("        if (strcmp(argv[i], \"--tree\") == 0) {\n", f);
    fputs("            imprimir_arvore = 1;\n", f);
    fputs("        } else if (strcmp(argv[i], \"--tokens\") == 0 && i + 1 < argc) {\n", f);
    fputs("            tokens = argv[++i];\n", f);
    fputs("        } else if (argv[i][0] == '-' && argv[i][1] != '\\0') {\n", f);
    fputs("            fprintf(stderr, \"uso: %s [--tree] [--tokens arquivo.tok | "
          "programa]\\n\", argv[0]);\n", f);
    fputs("            return 2;\n        } else {\n            arquivo = argv[i];\n"
          "        }\n    }\n", f);
    fputs("    if (imprimir_arvore && !SUPORTE_ARVORE)\n", f);
    fputs("        fprintf(stderr, \"aviso: gere o analisador com --tree para "
          "imprimir a arvore\\n\");\n\n", f);
    fputs("    atexit(liberar_recursos);\n", f);
    fputs("    if (tokens != NULL) {\n", f);
    fputs("        arquivo_tokens = fopen(tokens, \"r\");\n", f);
    fputs("        if (arquivo_tokens == NULL) {\n", f);
    fputs("            fprintf(stderr, \"nao foi possivel abrir %s\\n\", tokens);\n", f);
    fputs("            return 2;\n        }\n    } else {\n", f);
    fputs("        entrada = arquivo != NULL ? fopen(arquivo, \"r\") : stdin;\n", f);
    fputs("        if (entrada == NULL) {\n", f);
    fputs("            fprintf(stderr, \"nao foi possivel abrir %s\\n\", arquivo);\n", f);
    fputs("            return 2;\n        }\n", f);
    if (op->microc)
        fputs("        scanner_iniciar(entrada);\n        scanner_ativo = 1;\n", f);
    else
        fputs("        fonte = ler_tudo(entrada);\n", f);
    fputs("    }\n\n", f);
    fputs("    avancar();\n", f);
    fprintf(f, "    parse_%s();\n", g->nts[g->inicial].nome);
    fputs("    if (tok.tipo != TOKEN_FIM) {\n", f);
    fputs("        fim[0] = TOKEN_FIM;\n        fim[1] = -1;\n", f);
    fputs("        erro_sintatico(fim);\n    }\n", f);
    fputs("    printf(\"entrada aceita\\n\");\n", f);
    fputs("    return 0;\n}\n", f);
}

static const char *nome_base(const char *caminho)
{
    const char *b = strrchr(caminho, '/');
    return b != NULL ? b + 1 : caminho;
}

static int gerar_c(const Gramatica *g, const OpcoesGeracao *op)
{
    char *caminho = (char *) xmalloc(strlen(op->prefixo) + 3);
    FILE *f, *corpo;
    int c, erros = 0;

    /* Os procedimentos sao gerados primeiro num arquivo temporario porque
     * registram os conjuntos (conj_N), que precisam vir antes no .c. */
    corpo = tmpfile();
    if (corpo == NULL) {
        diag_erro(0, 0, "nao foi possivel criar arquivo temporario");
        free(caminho);
        return -1;
    }
    gerar_procedimentos(corpo, g, op);

    sprintf(caminho, "%s.c", op->prefixo);
    f = fopen(caminho, "w");
    if (f == NULL) {
        diag_erro(0, 0, "nao foi possivel criar %s", caminho);
        fclose(corpo);
        free(caminho);
        return -1;
    }
    escrever_cabecalho_c(f, op);
    fputs("#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n", f);
    fprintf(f, "#include \"%s.h\"\n\n", nome_base(op->prefixo));
    fprintf(f, "#define SUPORTE_ARVORE %d\n\n", op->arvore ? 1 : 0);

    fputs("/* ---- conjuntos de tokens ---- */\n\n", f);
    emitir_conjuntos(f, g);
    fputc('\n', f);

    emit_linhas(f, rt_estado);
    if (!op->microc)
        erros += gerar_lexico(f, g);
    gerar_nome_token(f, g);
    gerar_posicao_e_avancar(f, op->microc);
    emit_linhas(f, rt_primitivas);
    gen_token_de_arquivo(f, g, op->microc);

    rewind(corpo);
    while ((c = fgetc(corpo)) != EOF)
        fputc(c, f);
    fclose(corpo);

    gerar_main(f, g, op);
    fclose(f);
    free(caminho);
    return erros == 0 ? 0 : -1;
}

int codegen_gerar(Gramatica *g, const OpcoesGeracao *op)
{
    int r;
    if (!op->microc && validar_regexes(g) > 0)
        return -1;
    nomear_terminais(g, op->microc);
    r = gerar_h(g, op);
    if (r == 0)
        r = gerar_c(g, op);
    liberar_conjuntos();
    return r;
}
