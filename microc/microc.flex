%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "tokens.h"   /* TokenType, Token e YYSTYPE */
#include "strtab.h"   /* tabela de strings */

/*
 * Scanner de Micro C do TP1, convertido em funcao para o TP2: o
 * analisador sintatico gerado chama microc_yylex() (por meio de
 * proximo_token(), em scanner.c) cada vez que precisa de um token.
 * O que mudou em relacao ao TP1 esta descrito no README.md.
 */

YYSTYPE microc_yylval;

int linha_atual = 1;

/* Guarda o lexema na tabela de strings: lexemas repetidos (o mesmo
 * identificador usado varias vezes) ocupam memoria uma unica vez. */
static void guarda_lexema(const char *texto) {
    microc_yylval.symbol = strtab_inserir(texto);
}

static TokenType ultimo_token = UNDEF;
static int existe_ultimo_token = 0; /* 0 = inicio do arquivo */

#define RETORNA(tok) do { \
        ultimo_token = (tok); \
        existe_ultimo_token = 1; \
        return (tok); \
    } while (0)

static int termina_expressao(void) {
    if (!existe_ultimo_token) return 0;
    switch (ultimo_token) {
        case ID:
        case INTEGERCONST:
        case CHARCONST:
        case STRINGCONST:
        case RPAREN:
        case RBRACKET:
            return 1;
        default:
            return 0;
    }
}

#define MAX_LEXEMA 4096
static char buffer_lex[MAX_LEXEMA];
static int  buffer_pos;
static int  buffer_transbordou;

static void buffer_reinicia(void) {
    buffer_pos = 0;
    buffer_transbordou = 0;
}

static void buffer_adiciona(char c) {
    if (buffer_pos < MAX_LEXEMA - 1) {
        buffer_lex[buffer_pos++] = c;
    } else {
        buffer_transbordou = 1; /* string absurdamente grande: ignora o excesso */
    }
}


static int qtd_chars_charconst;

%}

/* Algumas definições*/

DIGIT       [0-9]
LETRA       [a-zA-Z_]
ALFANUM     [a-zA-Z0-9_]

%option prefix="microc_yy" noyywrap nounput noinput

%x COMMENT
%x STRSTATE
%x CHARSTATE

%%

 /* Regras */
 
<INITIAL><<EOF>>    { RETORNA(END_OF_FILE); }

\n                  { linha_atual++; }
[ \t\r]+            {  }

 
"//".*              {  }

"/*"                { BEGIN(COMMENT); }
<COMMENT>"*/"       { BEGIN(INITIAL); }
<COMMENT>\n         { linha_atual++; }
<COMMENT><<EOF>>    {
                        microc_yylval.error_msg = "EOF em comentario";
                        BEGIN(INITIAL);
                        RETORNA(UNDEF);
                    }
<COMMENT>.          {  }

 /* Fechamento de comentario sem abertura correspondente. */
"*/"                {
                        microc_yylval.error_msg = "Comentario nao iniciado";
                        RETORNA(UNDEF);
                    }

{LETRA}{ALFANUM}*   {
                        if      (strcmp(yytext, "main")   == 0) RETORNA(MAIN);
                        else if (strcmp(yytext, "if")     == 0) RETORNA(IF);
                        else if (strcmp(yytext, "else")   == 0) RETORNA(ELSE);
                        else if (strcmp(yytext, "for")    == 0) RETORNA(FOR);
                        else if (strcmp(yytext, "return") == 0) RETORNA(RETURN);
                        else if (strcmp(yytext, "int")    == 0) RETORNA(INT);
                        else if (strcmp(yytext, "char")   == 0) RETORNA(CHAR);
                        else if (strcmp(yytext, "print")  == 0) RETORNA(PRINT);
                        else {
                            guarda_lexema(yytext);
                            RETORNA(ID);
                        }
                    }

"-"?{DIGIT}+        {
                        if (yytext[0] == '-' && termina_expressao()) {
                            /* "-" e o operador de subtracao: devolve os
                             * digitos para serem re-escaneados depois. */
                            yyless(1);
                            RETORNA(MINUS);
                        }
                        guarda_lexema(yytext);
                        RETORNA(INTEGERCONST);
                    }

\'                  {
                        buffer_reinicia();
                        qtd_chars_charconst = 0;
                        BEGIN(CHARSTATE);
                    }

<CHARSTATE>\\n      { buffer_adiciona('\n'); qtd_chars_charconst++; }
<CHARSTATE>\\t      { buffer_adiciona('\t'); qtd_chars_charconst++; }
<CHARSTATE>\\\\     { buffer_adiciona('\\'); qtd_chars_charconst++; }
<CHARSTATE>\\\"     { buffer_adiciona('"');  qtd_chars_charconst++; }
<CHARSTATE>\\\'     { buffer_adiciona('\''); qtd_chars_charconst++; }
<CHARSTATE>\\0      { buffer_adiciona('\0'); qtd_chars_charconst++; }

<CHARSTATE>\000     {
                        microc_yylval.error_msg = "Constante de caractere contem caractere nulo";
                        BEGIN(INITIAL);
                        RETORNA(UNDEF);
                    }

<CHARSTATE>\n       {
                        /* A quebra de linha e devolvida a entrada
                         * (yyless) e contada so depois, no estado
                         * INITIAL: assim o erro sai com a linha em que
                         * a constante comecou, e nao com a seguinte. */
                        microc_yylval.error_msg = "Constante de caractere nao terminada";
                        yyless(0);
                        BEGIN(INITIAL);
                        RETORNA(UNDEF);
                    }

<CHARSTATE><<EOF>>  {
                        microc_yylval.error_msg = "EOF em constante de caractere";
                        BEGIN(INITIAL);
                        RETORNA(UNDEF);
                    }

<CHARSTATE>\'       {
                        /* aspas simples de fechamento */
                        BEGIN(INITIAL);
                        if (qtd_chars_charconst == 0) {
                            microc_yylval.error_msg = "Constante de caractere vazia";
                            RETORNA(UNDEF);
                        }
                        if (qtd_chars_charconst > 1) {
                            microc_yylval.error_msg = "Constante de caractere invalida (mais de um caractere)";
                            RETORNA(UNDEF);
                        }
                        buffer_lex[buffer_pos] = '\0';
                        guarda_lexema(buffer_lex);
                        RETORNA(CHARCONST);
                    }

<CHARSTATE>.        { buffer_adiciona(yytext[0]); qtd_chars_charconst++; }

\"                  { buffer_reinicia(); BEGIN(STRSTATE); }

<STRSTATE>\\n       { buffer_adiciona('\n'); }
<STRSTATE>\\t       { buffer_adiciona('\t'); }
<STRSTATE>\\\\      { buffer_adiciona('\\'); }
<STRSTATE>\\\"      { buffer_adiciona('"');  }
<STRSTATE>\\0       { buffer_adiciona('\0'); }

<STRSTATE>\000      {
                        microc_yylval.error_msg = "String contem caractere nulo";
                        BEGIN(INITIAL);
                        RETORNA(UNDEF);
                    }

<STRSTATE>\n        {
                        /* idem: a linha do erro e a da string */
                        microc_yylval.error_msg = "String nao terminada";
                        yyless(0);
                        BEGIN(INITIAL);
                        RETORNA(UNDEF);
                    }

<STRSTATE><<EOF>>   {
                        microc_yylval.error_msg = "EOF em string";
                        BEGIN(INITIAL);
                        RETORNA(UNDEF);
                    }

<STRSTATE>\"        {
                        buffer_lex[buffer_pos] = '\0';
                        guarda_lexema(buffer_lex);
                        BEGIN(INITIAL);
                        RETORNA(STRINGCONST);
                    }

<STRSTATE>.         { buffer_adiciona(yytext[0]); }

"=="                { RETORNA(EQ); }
"="                 { RETORNA(ASSIGN); }

"!="                { RETORNA(NEQ); }
"!"                 { RETORNA(NOT); }
"<="                { RETORNA(LEQ); }
"<"                 { RETORNA(LT); }
">="                { RETORNA(GEQ); }
">"                 { RETORNA(GT); }
"&&"                { RETORNA(AND); }
"||"                { RETORNA(OR); }

"+"                 { RETORNA(PLUS); }
"-"                 { RETORNA(MINUS); }
"*"                 { RETORNA(MUL); }
"/"                 { RETORNA(DIV); }
"%"                 { RETORNA(MOD); }
";"                 { RETORNA(SEMICOLON); }
","                 { RETORNA(COMMA); }
"("                 { RETORNA(LPAREN); }
")"                 { RETORNA(RPAREN); }
"{"                 { RETORNA(LBRACE); }
"}"                 { RETORNA(RBRACE); }
"["                 { RETORNA(LBRACKET); }
"]"                 { RETORNA(RBRACKET); }
.                   {
                        microc_yylval.error_msg = strtab_inserir(yytext);
                        RETORNA(UNDEF);
                    }

%%

/* O programa principal do TP1 saiu daqui: quem chama o scanner agora e o
 * analisador sintatico gerado. O programa que imprime os tokens, no
 * formato do livro-texto, esta em testes/tokens_microc.c. */

/* Prepara o scanner para um novo arquivo. */
void microc_reiniciar(void) {
    linha_atual = 1;
    ultimo_token = UNDEF;
    existe_ultimo_token = 0;
}
