/*
 * strtab.h - Tabela de strings do scanner de Micro C (TP1, secao 4.2).
 *
 * Cada lexema distinto e guardado uma unica vez. O scanner pede a
 * tabela uma copia do lexema e recebe sempre o mesmo ponteiro para
 * textos iguais; a tabela e a dona da memoria e libera tudo de uma vez
 * em strtab_liberar().
 */
#ifndef STRTAB_H
#define STRTAB_H

/* Devolve o ponteiro para a copia de s guardada na tabela, inserindo-a
 * se ainda nao existir. */
const char *strtab_inserir(const char *s);

/* Numero de strings distintas guardadas. */
int strtab_tamanho(void);

/* Libera todas as strings e a propria tabela. */
void strtab_liberar(void);

#endif
