---
title: "Gerador de Analisadores Sintáticos Descendentes Recursivos"
subtitle: "Compiladores I — Trabalho Prático 2 — Relatório"
author: "Vinicius Ramos — matrícula 202219040485"
lang: pt-BR
---

# 1. Introdução

Este trabalho implementa um gerador de analisadores sintáticos. O gerador lê uma gramática em EBNF e escreve, em ANSI C, um analisador descendente recursivo para a linguagem que ela descreve. O gerador nunca lê o programa do usuário: quem faz isso é o analisador gerado, que lê um token por vez e decide o que fazer olhando só para o token corrente.

O ponto de partida foi o código-base da disciplina, que já trazia o léxico da notação EBNF, as estruturas de dados e o programa principal. Coube a este trabalho implementar as cinco etapas do processamento da gramática e integrar o scanner de Micro C do TP1. O resultado passa em todos os 48 testes automáticos, compila sem avisos com `gcc -ansi -pedantic -Wall -Wextra` e não deixa memória alocada, segundo o valgrind, nem no gerador nem nos analisadores gerados.

# 2. Visão geral

O gerador executa cinco etapas em ordem e para na primeira que encontrar erros. Quando há erro, nenhum arquivo é escrito.

| Etapa | Arquivo | O que faz |
| --- | --- | --- |
| 1. Leitura | `ebnf_parser.c` | lê o `.ebnf` e monta uma árvore por regra |
| 2. Verificação semântica | `verificacao.c` | símbolos indefinidos, inalcançáveis, improdutivos e recursão à esquerda |
| 3. Conjuntos | `conjuntos.c` | anulável, FIRST e FOLLOW |
| 4. LL(1) | `ll1.c` | confere se um token basta para decidir |
| 5. Geração | `codegen.c` | escreve o `.c` e o `.h` do analisador |

Todas as etapas trabalham sobre a mesma estrutura: o lado direito de cada regra é uma árvore de nós `Expr`. Há um tipo de nó para cada construção da EBNF: terminal, não terminal, sequência, alternativa, opcional `[ ]`, repetição `{ }` e vazio. As etapas 3 e 4 anotam os nós, e a etapa 5 os traduz.

# 3. Etapa 1: leitura da gramática

A leitura usa o mesmo método que o gerador produz: um analisador descendente recursivo, com uma função para cada não terminal da gramática da própria notação.

```
expr  = seq { "|" seq } ;
seq   = { fator } ;
fator = NOME_TERM | LITERAL | NAOTERM | "(" expr ")" | "[" expr "]" | "{" expr "}" ;
```

`parse_fator` cria os nós de terminais e não terminais e os nós unários de `[ ]` e `{ }`. Os parênteses não criam nó: a função devolve a árvore de dentro. `parse_seq` e `parse_expr` evitam nós com um único filho. Uma sequência com um só fator devolve o próprio fator, e uma alternativa única devolve a própria sequência. Isso deixa a árvore menor e o código gerado mais limpo. Uma sequência sem nenhum fator vira um nó vazio, o que permite escrever alternativas vazias, como em `args = expr { "," expr } | ;`.

A função auxiliar `inicia_fator` diz se o token corrente pode começar um fator. É ela que encerra o laço de `parse_seq`, e é por causa dela que a falta de um `;` é apontada no `=` da regra seguinte: o nome da regra seguinte é lido como mais um fator, e o erro só aparece quando surge um token que não continua a sequência.

# 4. Etapa 2: verificações semânticas

As três verificações implementadas usam iteração até ponto fixo.

**Inalcançáveis (aviso).** Marca-se o símbolo inicial e, repetidamente, todo não terminal que aparece na regra de um já marcado. O que sobrar sem marca nunca é usado. É só um aviso, porque não impede a geração.

**Improdutivos (erro).** Um não terminal é produtivo se deriva alguma cadeia de terminais. Terminais, o vazio, `[ ]` e `{ }` são produtivos; uma sequência exige que todos os filhos sejam; uma alternativa exige ao menos um. Começa-se com nenhum não terminal produtivo e repete-se até nada mudar. O caso típico é a regra que só se refere a si mesma, como `laco = "(" laco ")" ;`, que nunca termina de derivar.

**Recursão à esquerda (erro).** Para cada não terminal, calcula-se quais não terminais podem aparecer na ponta esquerda da sua regra. Numa sequência, olha-se cada filho enquanto os anteriores forem anuláveis: em `item = [ "," ] lista`, `lista` está na ponta esquerda, porque `[ "," ]` pode sumir. Por isso esta verificação roda depois do cálculo de anulável. Se um não terminal aparece na ponta esquerda da própria regra, a recursão é direta. Se só aparece no fecho transitivo dessa relação, é indireta. Um analisador descendente entraria em laço infinito nos dois casos, pois chamaria a si mesmo sem consumir nenhum token.

# 5. Etapa 3: anulável, FIRST e FOLLOW

Os três atributos são calculados para cada não terminal e também para cada nó da árvore, porque o gerador de código precisa do FIRST de cada alternativa e de cada `[ ]` e `{ }`, e não só o da regra inteira.

| Construção | Anulável | FIRST |
| --- | --- | --- |
| terminal `t` | não | `{t}` |
| não terminal `B` | o de `B` | FIRST(`B`) |
| vazio | sim | `{}` |
| sequência `a1 ... an` | se todos forem | união dos FIRST até o primeiro filho não anulável |
| alternativa | se alguma for | união dos FIRST dos filhos |
| `[ a ]` e `{ a }` | sim | FIRST(`a`) |

Os valores começam vazios e só crescem, então a iteração termina. Repete-se uma passada sobre todas as regras até que nenhum conjunto mude. `bitset_unir` devolve 1 quando o conjunto de destino cresce, e é isso que detecta o ponto fixo.

O FOLLOW desce pela árvore. O FOLLOW do símbolo inicial começa com `$`, e o FOLLOW de cada não terminal é passado para a raiz da sua regra. Na sequência, a implementação percorre os filhos da direita para a esquerda guardando em um conjunto o que pode vir depois do filho atual: ao passar por um filho não anulável, o conjunto é trocado pelo FIRST dele; ao passar por um anulável, o FIRST é acrescentado. Em `{ a }`, o FIRST de `a` também segue `a`, porque depois de uma volta do laço pode vir outra. Ao chegar a um nó de não terminal, o conjunto é acrescentado ao FOLLOW daquele não terminal.

# 6. Etapa 4: condição LL(1)

A verificação percorre cada regra e testa três condições.

1. Em uma alternativa, os FIRST dos filhos são disjuntos dois a dois, e no máximo um filho é anulável.
2. Se um filho é anulável, o FOLLOW da alternativa é disjunto do FIRST dos demais.
3. Em `[ a ]` e `{ a }`, `a` não é anulável e FIRST(`a`) é disjunto do FOLLOW da construção.

As violações são erros, com uma exceção: a interseção entre FIRST e FOLLOW em `[ ]` e `{ }` é só um aviso. Ela é resolvida de forma gulosa, entrando na construção sempre que o token corrente estiver no FIRST. É o caso do `else` pendente de Micro C: em `if (a) if (b) x; else y;`, o `else` fica com o `if` mais próximo, como em C. Já um `{ a }` com `a` anulável é erro, porque o laço gerado poderia nunca terminar.

# 7. Etapa 5: geração de código

## 7.1 Tradução das regras

Cada não terminal vira um procedimento `parse_X`, e cada construção tem uma tradução fixa.

| EBNF | Código gerado |
| --- | --- |
| terminal `t` | `match(T);` |
| não terminal `B` | `parse_B();` |
| `a1 a2 ... an` | o código de cada filho, em ordem |
| <code>a1 &#124; ... &#124; an</code> | `if (FIRST(a1)) {...} else if (...) {...} else erro` |
| `[ a ]` | `if (FIRST(a)) {...}` |
| `{ a }` | `while (FIRST(a)) {...}` |

A alternativa anulável não tem teste: ela fica no `else` final, no lugar do erro, pois é a escolha certa quando o token não inicia nenhuma das outras. O erro sintático informa o FIRST da alternativa inteira, que é a lista dos tokens aceitos naquele ponto. Para a regra `factor = NUM | ID | "(" expr ")" ;`, o gerador produz:

```c
void parse_factor(void)
{
    if (tok.tipo == T_NUM) {
        match(T_NUM);
    } else if (tok.tipo == T_ID) {
        match(T_ID);
    } else if (tok.tipo == T_ABRE_PAR) {
        match(T_ABRE_PAR);
        parse_expr();
        match(T_FECHA_PAR);
    } else {
        erro_sintatico(conj_2);
    }
}
```

## 7.2 Expressões regulares

No modo próprio, o gerador também escreve o analisador léxico. Cada expressão regular de `TOKENS` vira uma função C que devolve o tamanho do maior prefixo reconhecido. A conversão tem três fases.

1. **Análise e AFN.** Um analisador descendente recursivo lê a expressão (`alt`, `concat`, `repet`, `atomo`) e monta o autômato não determinístico pela construção de Thompson, sem árvore intermediária. Cada função devolve um fragmento com um estado inicial e um final.
2. **AFD.** A construção de subconjuntos transforma o AFN em determinístico: cada estado do AFD é um conjunto de estados do AFN, fechado por transições vazias.
3. **Emissão.** O AFD vira um `switch` sobre o estado. As transições são agrupadas por faixas de caracteres, como em `(c >= '0' && c <= '9')`. A função guarda a posição da última passagem por um estado final, e é esse valor que ela devolve quando não há mais transição.

São aceitos concatenação, `|`, `*`, `+`, `?`, parênteses, classes, classes negadas, o ponto e escapes. Para conferir a conversão, as funções geradas para 24 expressões foram comparadas com o módulo `re` do Python em 72 mil cadeias aleatórias, sem nenhuma diferença.

O léxico gerado testa todos os terminais e fica com o maior prefixo. No empate, vence o literal: `se` é palavra reservada, e `sete` é `ID`.

## 7.3 Arquivo de tokens e liberação de memória

Com `--tokens`, o analisador lê os tokens de um arquivo no formato do livro-texto, em vez de ler o programa. Como o lexema pode conter aspas e vírgulas, o fim dele é procurado de trás para a frente, no último `linha =` da linha.

O analisador gerado encerra com `exit` no primeiro erro. Para que nada fique alocado nesse caminho, a liberação dos recursos foi registrada com `atexit`, e roda tanto no fim normal quanto em qualquer `exit`.

# 8. Integração com o scanner do TP1

No modo Micro C, o analisador não tem léxico próprio: ele chama `proximo_token()`, que encapsula o scanner do TP1. Os terminais da gramática são os nomes da enumeração `TokenType`, que o gerador lê do `tokens.h` para conferir se cada terminal existe. O scanner precisou de seis correções:

1. a enumeração saiu do `.flex` e foi para `tokens.h`, com a struct `Token`;
2. os lexemas, antes copiados com `strdup` e nunca liberados, vão para uma tabela de strings (hash com encadeamento), que guarda cada lexema uma vez e libera tudo no fim;
3. o scanner virou função (`%option prefix="microc_yy" noyywrap`), e o programa principal saiu do `.flex`;
4. o arquivo de tokens passou a seguir o formato do livro-texto, com o número do tipo;
5. o erro de string não terminada saía com a linha seguinte à da string, e agora sai na linha certa;
6. o erro de caractere inválido ganhou um texto, igual ao do modo próprio.

A gramática de Micro C é a do Apêndice B do livro-texto, fatorada à esquerda nas regras que começavam com `ID`.

# 9. Testes

O comando `make test` executa 48 casos e compara cada saída com a esperada.

| Gramática | Situação | Aceitos | Rejeitados |
| --- | --- | --- | --- |
| `expr` | válida | 3 | 4 |
| `json` | válida | 3 | 5 |
| `mini` | válida, com aviso do `senao` pendente | 3 | 5 |
| `microc` | válida, com aviso do `else` pendente | 4 | 4 |
| `recursao_esquerda` | recusada | | |
| `conflito_ll1` | recusada | | |
| `anulavel` | recusada (três conflitos com a cadeia vazia) | | |
| `erros_semanticos` | recusada (indefinido e não declarado) | | |
| `improdutivo` | recusada | | |

A gramática `json` exercita o léxico gerado, com strings com escapes e números com partes opcionais. A `mini`, uma linguagem imperativa pequena, exercita três diretivas `SKIP` (inclusive comentário de bloco), a prioridade das palavras reservadas e uma alternativa vazia. Os oito programas de Micro C são testados duas vezes: lendo o programa e lendo o arquivo de tokens, e as duas saídas têm de ser iguais. O gerador e os analisadores gerados foram executados com `valgrind --leak-check=full` sobre todas as gramáticas e todos os programas, aceitos e rejeitados, sem vazamentos nem acessos inválidos.

# 10. Limitações

- O analisador gerado para no primeiro erro: não há recuperação de erros.
- Quando o erro ocorre em um `match`, a mensagem cita só o token que aquele `match` esperava, mesmo que outros também fossem aceitos ali. Em `{ "a": 1 2 }`, por exemplo, a mensagem pede `}`, embora `,` também servisse.
- O AFD não é minimizado. Os autômatos são corretos, mas podem ter estados equivalentes repetidos.
- As expressões regulares não têm repetição contada (`a{2,3}`) nem âncoras.
- No modo Micro C as mensagens não têm coluna, porque o scanner do TP1 só registra a linha.

# 11. Uso de IA

O código das partes a implementar, os testes novos e a primeira versão deste relatório foram produzidos com o auxílio do Claude (Anthropic). Cada uso está registrado em `USO_IA.md`.
