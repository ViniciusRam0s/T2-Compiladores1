# USO_IA.md

Ferramentas de IA generativa foram usadas neste trabalho. Cada uso está registrado abaixo.

    Ferramenta: Claude (Anthropic), pelo Claude Code
    Trecho:     src/ebnf_parser.c (parse_expr, parse_seq, parse_fator)
    Finalidade: implementar a leitura do lado direito das regras EBNF, seguindo
                a gramatica da notacao descrita no cabecalho do arquivo
    O que fiz:  usado para correção de erro

    Ferramenta: Claude (Anthropic), pelo Claude Code
    Trecho:     src/conjuntos.c (anulavel, FIRST, FOLLOW) e src/ll1.c
    Finalidade: implementar os calculos por ponto fixo e as condicoes LL(1),
                com as mensagens das saidas esperadas do pacote
    O que fiz:  uso moderado para criação de partes do codigo

    Ferramenta: Claude (Anthropic), pelo Claude Code
    Trecho:     src/codegen.c (gen_expr, expressoes regulares, gen_token_de_arquivo,
                gerar_main) e src/microc_mapa.c (leitura do tokens.h)
    Finalidade: implementar a traducao das regras, a conversao de expressao
                regular em automato (Thompson + subconjuntos), a leitura do
                arquivo de tokens e a liberacao de recursos com atexit
    O que fiz:  usado somente para verificação 

    Ferramenta: Claude (Anthropic), pelo Claude Code
    Trecho:     testes/ (gramaticas json, mini, anulavel, erros_semanticos e
                improdutivo; programas de teste; tokens_microc.c; executar.sh)
    Finalidade: escrever gramaticas e programas de teste novos, o gravador do
                arquivo de tokens e o teste da opcao --tokens
    O que fiz:  usado somente para verificação

