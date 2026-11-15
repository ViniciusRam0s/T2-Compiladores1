/*
 * tokens.h - Tokens de Micro C (TP1; livro-texto, secao 2.3.1).
 *
 * No TP1 a enumeracao ficava dentro do microc.flex. Ela veio para este
 * cabecalho para ser compartilhada entre o scanner, o adaptador
 * (scanner.c) e o analisador sintatico gerado, e para o gerador conferir
 * os nomes dos terminais da gramatica (TOKENS EXTERN "tokens.h").
 */
#ifndef TOKENS_H
#define TOKENS_H

typedef enum {
    /* Tokens fundamentais */
    UNDEF,
    ID,
    END_OF_FILE,

    /* Constantes literais */
    INTEGERCONST,
    CHARCONST,
    STRINGCONST,

    /* Operadores aritmeticos */
    PLUS, MINUS, MUL, DIV, MOD,

    /* Operadores relacionais e logicos */
    EQ, NEQ, LT, GT, LEQ, GEQ, AND, OR, NOT,

    /* Simbolos de atribuicao e pontuacao */
    ASSIGN, SEMICOLON, COMMA, LPAREN, RPAREN,
    LBRACE, RBRACE, LBRACKET, RBRACKET,

    /* Palavras reservadas */
    MAIN, IF, ELSE, FOR, RETURN, INT, CHAR, PRINT
} TokenType;

typedef struct {
    TokenType tipo;
    char lexema[100];
    int linha;
} Token;

/* Valor semantico do token que microc_yylex() acabou de devolver. Os
 * dois ponteiros apontam para a tabela de strings (strtab.h) ou para
 * mensagens constantes: quem le nao deve libera-los. */
typedef struct {
    const char *symbol;     /* lexema de ID, INTEGERCONST, CHARCONST, STRINGCONST */
    const char *error_msg;  /* mensagem de erro, quando o token e UNDEF */
} YYSTYPE;

extern YYSTYPE microc_yylval;

#endif
