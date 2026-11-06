/* Partie 1 : modèle de Fibonacci (aucune mort). Unité = couple, pas = 1 mois. */
#include <stdio.h>

int main(void)
{
    long jeunes = 1, adultes = 0;          /* départ : 1 couple de jeunes */
    printf("mois, jeunes, adultes, total\n");
    for (int m = 0; m <= 24; m++) {
        printf("%d, %ld, %ld, %ld\n", m, jeunes, adultes, jeunes + adultes);
        long nouveaux = adultes;           /* chaque couple adulte donne 1 couple de jeunes */
        adultes += jeunes;                 /* les jeunes deviennent adultes */
        jeunes = nouveaux;
    }
    return 0;
}