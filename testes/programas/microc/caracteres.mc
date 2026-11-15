/* constantes de caractere, escapes, comentarios e numeros negativos */
int main() {
    char c;
    char nome[10];
    int n;
    c = 'a';
    nome[0] = '\n';
    n = -5;                 // INTEGERCONST negativo
    n = n - 3 * (n % 2);    /* aqui o "-" e o operador */
    if (c == 'a' && n <= 0 || !n) {
        print("linha\tcom \"aspas\"\n");
    } else
        ;
    return n;
}
