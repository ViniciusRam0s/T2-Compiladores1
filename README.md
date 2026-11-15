aluno: 202219040485
uso_ia: sim

# Compiladores I — Trabalho Prático 2: Gerador de Analisadores Sintáticos Descendentes Recursivos

O gerador lê uma gramática em EBNF e gera, em ANSI C, um analisador sintático descendente recursivo para a linguagem descrita. O uso de IA está registrado em `USO_IA.md`, e o relatório está em `RELATORIO.pdf` (fonte em `RELATORIO.md`).

## Compilar e executar

```
make                      # compila o gerador e o analisador de Micro C
make test                 # executa os testes de testes/
make microc               # gera e compila build/microc/parser_microc
make clean
```

```
./gerador gramatica.ebnf -o saida/parser [--scanner microc] [--sets] [--tree] [--debug]
gcc -ansi -pedantic -Wall -Wextra -o parser saida/parser.c
./parser [--tree] programa.txt        # ou: ./parser --tokens tokens.txt
```

Requisitos: `gcc`, `make` e `flex` (este só para o modo Micro C). Todo o código compila sem avisos com `gcc -ansi -pedantic -Wall -Wextra`. A única exceção é o `lex.yy.c` gerado pelo flex.

## O que foi implementado

| Etapa | Arquivo | Implementação |
| --- | --- | --- |
| 1. Leitura da EBNF | `src/ebnf_parser.c` | `parse_expr`, `parse_seq` e `parse_fator`: descendente recursivo, uma função por não terminal da notação |
| 2. Verificação semântica | `src/verificacao.c` | inalcançáveis (aviso), improdutivos (erro) e recursão à esquerda direta e indireta (erro), todos por ponto fixo |
| 3. Conjuntos | `src/conjuntos.c` | anulável, FIRST e FOLLOW, para cada não terminal e para cada nó da árvore das regras |
| 4. LL(1) | `src/ll1.c` | as três condições; o conflito FIRST/FOLLOW em `[ ]` e `{ }` é aviso, resolvido de forma gulosa |
| 5. Geração | `src/codegen.c` | `gen_expr` (sequência, alternativa, `[ ]`, `{ }`); expressões regulares (Thompson + subconjuntos, emitidas como `switch`); leitura do arquivo de tokens (`--tokens`) |
| Micro C | `src/microc_mapa.c`, `microc/` | leitura da enumeração `TokenType` do `tokens.h`; scanner do TP1 adaptado |

Além dos pontos marcados no código-base, duas mudanças em `codegen.c`:

- as expressões regulares são conferidas antes de qualquer arquivo ser escrito, para que um erro nelas não deixe um `.c` pela metade;
- o analisador gerado libera seus recursos em uma função registrada com `atexit`, porque ele encerra com `exit` ao encontrar um erro. Assim o valgrind não acusa memória alocada nem nas entradas rejeitadas.

### Expressões regulares aceitas em TOKENS

Concatenação, `|`, `*`, `+`, `?`, parênteses, classes `[a-z]` e `[^...]`, o ponto (qualquer caractere menos a quebra de linha) e os escapes `\n \t \r \f \v`, `\d`, `\w`, `\s` e `\c` para qualquer outro caractere (`\/`, `\\`, `\.`). O léxico gerado escolhe o maior prefixo; em caso de empate, o literal vence o terminal com nome (palavra reservada antes de `ID`).

## Integração com o scanner do TP1

A pasta `microc/` contém o scanner do TP1 convertido em função. O analisador gerado com `--scanner microc` chama `proximo_token()` (em `scanner.c`), que chama o `microc_yylex()` do flex.

| Arquivo | Conteúdo |
| --- | --- |
| `microc/microc.flex` | o scanner do TP1, com as correções abaixo |
| `microc/tokens.h` | `TokenType`, `Token` e o valor semântico `YYSTYPE` |
| `microc/strtab.c`, `strtab.h` | tabela de strings (hash com encadeamento) |
| `microc/scanner.c`, `scanner.h` | adaptador entre `microc_yylex()` e `Token` |

### Correções feitas desde o TP1

1. **`tokens.h` separado.** No TP1 a enumeração `TokenType` ficava dentro do `microc.flex`. Ela foi para `microc/tokens.h`, com a struct `Token` do livro-texto, porque agora é usada também pelo adaptador, pelo analisador gerado e pelo gerador (`TOKENS EXTERN`).
2. **Tabela de strings.** O TP1 guardava cada lexema com `strdup` e nunca liberava a memória. Agora os lexemas de `ID`, `INTEGERCONST`, `CHARCONST` e `STRINGCONST` vão para uma tabela de strings (`strtab.c`): cada lexema distinto é guardado uma vez, e `scanner_finalizar` libera tudo. Era um requisito do TP1 (seção 4.2) e é o que zera os vazamentos no valgrind.
3. **Scanner como função.** Foram acrescentados `%option prefix="microc_yy" noyywrap` (o `yywrap` manual saiu) e a função `microc_reiniciar`. O programa principal saiu do `.flex` e foi para `testes/tokens_microc.c`.
4. **Formato do arquivo de tokens.** O TP1 imprimia o nome do tipo e não usava vírgulas (`Token: tipo = INT lexema = 'int' linha = 1`). O gravador agora segue o formato do livro-texto, com o número do tipo: `Token: tipo = 34, lexema = 'int', linha = 1`. Ele também grava os tokens `UNDEF` e `END_OF_FILE`, para que `--tokens` reporte os mesmos erros da leitura direta.
5. **Linha do erro em string não terminada.** O TP1 somava 1 à linha antes de devolver o erro de "String nao terminada" (e de "Constante de caractere nao terminada"), que saía com a linha seguinte à da string. A quebra de linha agora é devolvida à entrada com `yyless(0)` e contada depois, e o erro sai na linha da string.
6. **Caractere inválido.** O scanner continua devolvendo só o caractere como mensagem, como pede o TP1. O adaptador troca esse texto por `caractere invalido '@'`, igual ao do léxico gerado no modo próprio.

O `tokens.h` deste trabalho tem `INT = 34`. O exemplo do manual usa 33 porque numera a enumeração de outro jeito; o analisador usa os valores do `tokens.h`.

A gramática `testes/gramaticas/microc.ebnf` é a do Apêndice B do livro-texto, fatorada para LL(1). O `else` pendente gera um aviso de conflito em `[ "else" comando ]`, resolvido de forma gulosa: o `else` fica com o `if` mais próximo.

## Testes

`make test` roda `testes/executar.sh`, que compara com as saídas esperadas:

1. a saída do gerador com `--sets` para cada gramática (`testes/esperado/*.gerador`);
2. a saída dos analisadores gerados para cada programa (`testes/programas/<gramatica>/*.esperado`);
3. para cada programa de Micro C, a saída do analisador lendo o arquivo de tokens (`--tokens`), que tem de ser igual à da leitura direta.

| Gramática | Situação | Programas aceitos | Programas rejeitados |
| --- | --- | --- | --- |
| `expr.ebnf` | válida | 3 | 4 |
| `json.ebnf` | válida | 3 | 5 |
| `mini.ebnf` | válida (aviso: `senao` pendente) | 3 | 5 |
| `microc.ebnf` | válida (aviso: `else` pendente) | 4 | 4 |
| `recursao_esquerda.ebnf` | recusada: recursão à esquerda direta e indireta | | |
| `conflito_ll1.ebnf` | recusada: alternativas com o mesmo FIRST | | |
| `anulavel.ebnf` | recusada: três conflitos ligados à cadeia vazia | | |
| `erros_semanticos.ebnf` | recusada: não terminal indefinido e terminal não declarado | | |
| `improdutivo.ebnf` | recusada: não terminais improdutivos | | |

Cada caso termina com a linha `status <código de saída>`. Para criar um caso, escreva a entrada, confira a saída do analisador à mão e salve-a como `.esperado`.
