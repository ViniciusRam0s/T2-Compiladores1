/*
 * microc_mapa.c - Correspondencia com os tokens de Micro C (ver microc_mapa.h).
 *
 * A tabela de literais segue o tokens.h do livro-texto (secao 2.3.1):
 *
 *   UNDEF, ID, END_OF_FILE, INTEGERCONST, CHARCONST, STRINGCONST,
 *   PLUS, MINUS, MUL, DIV, MOD, EQ, NEQ, LT, GT, LEQ, GEQ, AND, OR, NOT,
 *   ASSIGN, SEMICOLON, COMMA, LPAREN, RPAREN, LBRACE, RBRACE,
 *   LBRACKET, RBRACKET, MAIN, IF, ELSE, FOR, RETURN, INT, CHAR, PRINT
 *
 * microc_carregar_tokens_h le os nomes da enumeracao TokenType para
 * conferir os terminais nomeados da gramatica (ID, INTEGERCONST...).
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "microc_mapa.h"
#include "util.h"

static const char *const literais[][2] = {
    { "+",  "PLUS" },      { "-",  "MINUS" },     { "*",  "MUL" },
    { "/",  "DIV" },       { "%",  "MOD" },
    { "==", "EQ" },        { "!=", "NEQ" },       { "<",  "LT" },
    { ">",  "GT" },        { "<=", "LEQ" },       { ">=", "GEQ" },
    { "&&", "AND" },       { "||", "OR" },        { "!",  "NOT" },
    { "=",  "ASSIGN" },    { ";",  "SEMICOLON" }, { ",",  "COMMA" },
    { "(",  "LPAREN" },    { ")",  "RPAREN" },    { "{",  "LBRACE" },
    { "}",  "RBRACE" },    { "[",  "LBRACKET" },  { "]",  "RBRACKET" },
    { "main", "MAIN" },    { "if", "IF" },        { "else", "ELSE" },
    { "for", "FOR" },      { "return", "RETURN" },{ "int", "INT" },
    { "char", "CHAR" },    { "print", "PRINT" }
};

static char **nomes = NULL;
static int nnomes = 0, capnomes = 0;

const char *microc_nome_literal(const char *literal)
{
    size_t i;
    for (i = 0; i < sizeof(literais) / sizeof(literais[0]); i++)
        if (strcmp(literais[i][0], literal) == 0)
            return literais[i][1];
    return NULL;
}

static void adicionar_nome(const char *s, size_t n)
{
    if (nnomes == capnomes) {
        capnomes = capnomes ? 2 * capnomes : 64;
        nomes = (char **) xrealloc(nomes, (size_t) capnomes * sizeof(char *));
    }
    nomes[nnomes++] = xstrndup(s, n);
}

/* Pula espacos e comentarios a partir de p. */
static const char *pular_brancos(const char *p)
{
    for (;;) {
        while (isspace((unsigned char) *p))
            p++;
        if (p[0] == '/' && p[1] == '*') {
            p += 2;
            while (*p != '\0' && !(p[0] == '*' && p[1] == '/'))
                p++;
            if (*p != '\0')
                p += 2;
        } else if (p[0] == '/' && p[1] == '/') {
            while (*p != '\0' && *p != '\n')
                p++;
        } else {
            return p;
        }
    }
}

static int eh_ident(int c)
{
    return isalnum(c) || c == '_';
}

/* Se p esta no '{' de um "enum { ... } TokenType", devolve o ponteiro
 * para o '}' correspondente; senao devolve NULL. */
static const char *fecha_de_tokentype(const char *p)
{
    const char *fecha = strchr(p, '}'), *q;
    if (fecha == NULL)
        return NULL;
    q = pular_brancos(fecha + 1);
    if (strncmp(q, "TokenType", 9) == 0 && !eh_ident((unsigned char) q[9]))
        return fecha;
    return NULL;
}

/* Le os nomes da enumeracao "typedef enum { ... } TokenType;" de um
 * tokens.h, ignorando comentarios e valores explicitos ("X = 3").
 * Devolve 0 se a enumeracao foi lida, -1 caso contrario. */
int microc_carregar_tokens_h(const char *caminho)
{
    char *fonte;
    const char *p, *fecha = NULL, *ini;

    microc_liberar();
    if (caminho == NULL || (fonte = ler_arquivo(caminho)) == NULL) {
        diag_aviso(0, 0, "nao foi possivel abrir %s: nomes dos tokens nao verificados",
                   caminho != NULL ? caminho : "tokens.h");
        return -1;
    }

    /* procura a enumeracao cujo nome (depois do '}') e TokenType */
    for (p = strstr(fonte, "enum"); p != NULL; p = strstr(p + 4, "enum")) {
        const char *q = pular_brancos(p + 4);
        while (eh_ident((unsigned char) *q))    /* "enum Nome {" */
            q++;
        q = pular_brancos(q);
        if (*q == '{' && (fecha = fecha_de_tokentype(q)) != NULL) {
            p = q + 1;
            break;
        }
    }
    if (p == NULL || fecha == NULL) {
        diag_aviso(0, 0, "enumeracao TokenType nao encontrada em %s: nomes dos tokens "
                   "nao verificados", caminho);
        free(fonte);
        return -1;
    }

    /* lista: nome [ "=" valor ] { "," nome [ "=" valor ] } */
    while (p < fecha) {
        p = pular_brancos(p);
        if (p >= fecha)
            break;
        if (isalpha((unsigned char) *p) || *p == '_') {
            ini = p;
            while (eh_ident((unsigned char) *p))
                p++;
            adicionar_nome(ini, (size_t) (p - ini));
        }
        while (p < fecha && *p != ',')          /* pula "= valor" */
            p++;
        if (p < fecha)
            p++;                                /* ',' */
    }
    free(fonte);
    return 0;
}

int microc_num_tokens(void)
{
    return nnomes;
}

int microc_token_existe(const char *nome)
{
    int i;
    for (i = 0; i < nnomes; i++)
        if (strcmp(nomes[i], nome) == 0)
            return 1;
    return 0;
}

void microc_liberar(void)
{
    int i;
    for (i = 0; i < nnomes; i++)
        free(nomes[i]);
    free(nomes);
    nomes = NULL;
    nnomes = capnomes = 0;
}
