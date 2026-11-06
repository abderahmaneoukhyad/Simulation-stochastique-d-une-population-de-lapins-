#include <math.h>
#include "lapin.h"
#include "compteur.h"

/* ------------------------- aléatoire ------------------------- */
static uint64_t etat = 88172645463325252ULL; 

static uint32_t rng32(void)
{
    etat ^= etat << 13;
    etat ^= etat >> 7;
    etat ^= etat << 17;
    return (uint32_t)(etat >> 32);
}

uint32_t alea(uint32_t n)
{
    return (uint32_t)(((uint64_t)rng32() * n) >> 32);
}

/* ------------------------- tables de survie MENSUELLE -------------------------
 * Les taux de l'énoncé (60 % / 35 %) sont ANNUELS. Pour un pas de 1 mois :
 *     s_mois = s_an ^ (1/12)
 * Après 10 ans, la survie annuelle décroît linéairement jusqu'à 0 à 15 ans.   */
static uint64_t seuil_jeune;
static uint64_t seuil_adulte[AGE_MAX + 1];

static uint64_t seuil(double p)         /* probabilité -> seuil sur 32 bits */
{
    return (uint64_t)(p * 4294967296.0);
}

void sim_init(uint64_t graine)
{
    etat = graine ? graine : 88172645463325252ULL;    
    for (int i = 0; i < 20; i++) rng32();   /* chauffe du générateur */

    seuil_jeune = seuil(pow(SURVIE_JEUNE_AN, 1.0 / 12.0));
    for (int a = 0; a <= AGE_MAX; a++) {
        double s = SURVIE_ADULTE_AN;
        if (a >= AGE_VIEUX)
            s *= 1.0 - (double)(a - AGE_VIEUX) / (AGE_MAX - AGE_VIEUX);
        if (s < 0) s = 0;
        seuil_adulte[a] = seuil(pow(s, 1.0 / 12.0));
    }
}

/* nombre de portées par an : 3..9, plus probable autour de 5, 6, 7 */
static uint8_t tirer_portees(void)
{
    static const int poids[7] = {5, 10, 20, 25, 20, 12, 8};   /* somme = 100 */
    int r = (int)alea(100), cumul = 0;
    for (int i = 0; i < 7; i++) {
        cumul += poids[i];
        if (r < cumul) return (uint8_t)(3 + i);
    }
    return 9;
}

/* ------------------------- lapins ------------------------- */
static Lapin nouveau(uint8_t sexe, uint8_t age)
{
    Lapin l;
    l.age = age;
    l.maturite = (uint8_t)(5 + alea(4));       /* 5 à 8 mois */
    l.sexe = sexe;
    l.portees_an = tirer_portees();
    return l;
}

Lapin creer_lapin(void)
{
    return nouveau((uint8_t)alea(2), 0);       /* 50 % mâle / 50 % femelle */
}

Lapin creer_lapin_adulte(uint8_t sexe)
{
    return nouveau(sexe, 8);                  /* 8 >= maturité max */
}

int est_adulte(const Lapin *l)
{
    return l->age >= l->maturite;
}

/* ------------------------- population ------------------------- */
void pop_init(Population *p, size_t capacite, size_t max)
{
    p->tab = malloc(capacite * sizeof(Lapin));
    if (p->tab == NULL) { perror("malloc"); exit(1); }
    p->n = 0;
    p->capacite = capacite;
    p->max = max;
}

int pop_ajouter(Population *p, Lapin l)
{
    if (p->n >= p->max) return 0;             /* plafond atteint */
    if (p->n == p->capacite) {
        size_t nc = p->capacite * 2;
        if (nc > p->max) nc = p->max;
        Lapin *t = realloc(p->tab, nc * sizeof(Lapin));
        if (t == NULL) return 0;               /* on ne perd pas l'ancien tableau */
        p->tab = t;
        p->capacite = nc;
    }
    p->tab[p->n++] = l;
    return 1;
}

void pop_liberer(Population *p)
{
    free(p->tab);
    p->tab = NULL;
    p->n = p->capacite = 0;
}

/* ------------------------- un mois -------------------------
 * 1) vieillissement + mort (les nouveau-nés du mois ne vieillissent pas)
 * 2) reproduction : femelle adulte, au moins un mâle adulte vivant,
      portée avec probabilité portees_an/12 chaque mois, 3 à 6 petits.   */
int simuler_mois(Population *p, Stats *s)
{
    size_t i = 0;
    size_t males_adultes = 0;

    while (i < p->n) {
        Lapin *l = &p->tab[i];
        l->age++;

        int survit;
        if (l->age >= AGE_MAX)       survit = 0;
        else if (est_adulte(l))      survit = rng32() < seuil_adulte[l->age];
        else                         survit = rng32() < seuil_jeune;

        if (!survit) {
            s->morts++;
            s->morts_par_age[l->age]++;
            p->tab[i] = p->tab[--p->n];        
        } else {
            if (l->sexe == MALE && est_adulte(l)) males_adultes++;
            i++;
        }
    }

    if (males_adultes == 0) return 0;

    size_t n_avant = p->n;
    for (size_t j = 0; j < n_avant; j++) {
        Lapin f = p->tab[j];                   
        if (f.sexe == FEMELLE && est_adulte(&f) && alea(12) < f.portees_an) {
            int petits = 3 + (int)alea(4);     /* 3 à 6, équiprobable */
            for (int k = 0; k < petits; k++) {
                if (!pop_ajouter(p, creer_lapin())) return -1;
                s->naissances++;
            }
        }
    }
    return 0;
}
