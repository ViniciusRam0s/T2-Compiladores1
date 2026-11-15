/*
 * bitset.h - Conjuntos de terminais representados como vetores de bits.
 * Modulo FORNECIDO: nao e necessario altera-lo.
 */
#ifndef BITSET_H
#define BITSET_H

typedef struct {
    int n;
    int npalavras;
    unsigned long *w;
} Bitset;

void bitset_iniciar(Bitset *b, int n);/
void bitset_liberar(Bitset *b);
void bitset_limpar(Bitset *b);
void bitset_add(Bitset *b, int i);
int  bitset_tem(const Bitset *b, int i);
int  bitset_vazio(const Bitset *b);
int  bitset_tamanho(const Bitset *b);     /* numero de elementos */

/* dest = dest U src. Retorna 1 se dest mudou (util no ponto fixo). */
int  bitset_unir(Bitset *dest, const Bitset *src);
void bitset_copiar(Bitset *dest, const Bitset *src);

/* 1 se a intersecao de a e b nao for vazia. */
int  bitset_intersecta(const Bitset *a, const Bitset *b);

/* res = a intersecao b (res ja iniciado com a mesma capacidade). */
void bitset_intersecao(Bitset *res, const Bitset *a, const Bitset *b);

/* Iteracao: menor elemento >= i, ou -1 se nao houver*/
int  bitset_proximo(const Bitset *b, int i);

#endif
