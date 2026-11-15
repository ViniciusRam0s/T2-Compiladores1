/*
 * scanner.c - Adaptador entre o scanner do TP1 (microc.flex) e o
 * analisador sintatico gerado (secao 3.1 do enunciado).
 *
 * O scanner e o microc.flex do TP1, gerado com
 *     %option prefix="microc_yy"
 * e que mantem a variavel global linha_atual (TP1, secao 4.4).
 *
 *   - ID e constantes usam o texto guardado na tabela de strings
 *     (microc_yylval.symbol), ja com as sequencias de escape
 *     convertidas (TP1, secao 4.3);
 *   - UNDEF traz a mensagem de microc_yylval.error_msg
 *     ("String nao terminada", "EOF em comentario" etc.);
 *   - scanner_finalizar libera a tabela de strings.
 */
#include <stdio.h>
#include <string.h>
#include "scanner.h"
#include "strtab.h"

/* ---- simbolos do scanner gerado pelo flex (ajuste ao seu TP1) ---- */
extern int   microc_yylex(void);
extern FILE *microc_yyin;
extern char *microc_yytext;
extern int   linha_atual;
extern int   microc_yylex_destroy(void);
extern void  microc_reiniciar(void);

/* Texto a guardar em Token.lexema para o token que microc_yylex acabou
 * de devolver. */
static const char *texto_do_lexema(int tipo)
{
    static char invalido[32];
    const char *msg;

    switch (tipo) {
    case ID: case INTEGERCONST: case CHARCONST: case STRINGCONST:
        return microc_yylval.symbol;
    case UNDEF:
        /* Para um caractere invalido, o TP1 devolve como mensagem so o
         * proprio caractere (TP1, secao 4.1). Aqui ele ganha um texto,
         * igual ao do lexico gerado no modo proprio. */
        msg = microc_yylval.error_msg;
        if (msg != NULL && strlen(msg) == 1) {
            sprintf(invalido, "caractere invalido '%c'", msg[0]);
            return invalido;
        }
        return msg;
    default:
        return microc_yytext;
    }
}

/* O TP1 permite que tokens de um caractere sejam devolvidos pelo proprio
 * codigo ASCII (TP1, secao 5). O analisador gerado compara com os nomes
 * de TokenType, entao esses codigos sao convertidos aqui.
 * Cuidado: valores ate o ultimo item da enumeracao (PRINT = 36 no
 * tokens.h do livro) sao sempre TokenType. Por isso '!' (33, que e
 * RETURN), '"', '#' e '$' nao podem ser devolvidos como ASCII. */
static TokenType normalizar(int tipo)
{
    if (tipo <= (int) PRINT)
        return (TokenType) tipo;
    switch (tipo) {
    case '+': return PLUS;
    case '-': return MINUS;
    case '*': return MUL;
    case '/': return DIV;
    case '%': return MOD;
    case '<': return LT;
    case '>': return GT;
    case '=': return ASSIGN;
    case ';': return SEMICOLON;
    case ',': return COMMA;
    case '(': return LPAREN;
    case ')': return RPAREN;
    case '{': return LBRACE;
    case '}': return RBRACE;
    case '[': return LBRACKET;
    case ']': return RBRACKET;
    default:  return (TokenType) tipo;
    }
}

void scanner_iniciar(FILE *entrada)
{
    microc_yyin = entrada;
    microc_reiniciar();
}

Token proximo_token(void)
{
    Token t;
    int tipo;
    const char *texto;

    memset(&t, 0, sizeof t);
    tipo = microc_yylex();
    /* Observacao: o flex devolve 0 no fim da entrada, mas 0 e UNDEF no
     * tokens.h. O TP1 deve devolver END_OF_FILE na regra <<EOF>>. */
    t.tipo = normalizar(tipo);
    t.linha = linha_atual;
    texto = t.tipo == END_OF_FILE ? "" : texto_do_lexema(tipo);
    if (texto != NULL) {
        strncpy(t.lexema, texto, sizeof t.lexema - 1);
        t.lexema[sizeof t.lexema - 1] = '\0';
    }
    return t;
}

void scanner_finalizar(void)
{
    microc_yylex_destroy();
    strtab_liberar();
}
