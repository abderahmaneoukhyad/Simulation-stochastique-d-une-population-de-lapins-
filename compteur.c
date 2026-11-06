#include "compteur.h"

size_t compter_femelles_adultes(const Population *p)
{
    size_t c = 0;
    for (size_t i = 0; i < p->n; i++)
        if (p->tab[i].sexe == FEMELLE && est_adulte(&p->tab[i])) c++;
    return c;
}

size_t compter_males_adultes(const Population *p)
{
    size_t c = 0;
    for (size_t i = 0; i < p->n; i++)
        if (p->tab[i].sexe == MALE && est_adulte(&p->tab[i])) c++;
    return c;
}

size_t compter_femelles(const Population *p)
{
    size_t c = 0;
    for (size_t i = 0; i < p->n; i++)
        if (p->tab[i].sexe == FEMELLE) c++;
    return c;
}

void histogramme_ages(const Population *p, long h[16])
{
    for (int a = 0; a < 16; a++) h[a] = 0;
    for (size_t i = 0; i < p->n; i++) {
        int a = p->tab[i].age / 12;
        if (a > 15) a = 15;
        h[a]++;
    }
}