# Pasta microc/

Aqui fica o scanner de Micro C do TP1, convertido em função para o analisador gerado com `--scanner microc` (seção 3.1 do enunciado).

- `microc.flex`: o scanner do TP1, com `%option prefix="microc_yy" noyywrap` e `END_OF_FILE` na regra `<<EOF>>`.
- `tokens.h`: `TokenType` e `Token` como no livro-texto (seção 2.3.1), mais o valor semântico `YYSTYPE`.
- `strtab.c`, `strtab.h`: tabela de strings onde o scanner guarda os lexemas.
- `scanner.h`: interface usada pelo analisador gerado (`scanner_iniciar`, `proximo_token`, `scanner_finalizar`).
- `scanner.c`: adaptador entre `microc_yylex()` e `Token`.

O Makefile compila todos os `.c` desta pasta. O `lex.yy.c` gerado é compilado sem a exigência de ausência de avisos.

As correções feitas no scanner desde o TP1 estão descritas no `README.md` da raiz.

Este scanner devolve sempre valores de `TokenType`, nunca o código ASCII do caractere. O adaptador e a leitura do arquivo de tokens ainda assim aceitam códigos ASCII acima do último item da enumeração (36), como permite a seção 5 do TP1.
