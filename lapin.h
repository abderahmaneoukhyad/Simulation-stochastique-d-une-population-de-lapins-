#ifndef LAPIN_H
#define LAPIN_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* ---------- Paramètres du modèle (tous au même endroit, faciles à faire varier) ---------- */
#define MALE      0
#define FEMELLE   1
#define AGE_MAX   180          /* 15 ans en mois : mort certaine */
#define AGE_VIEUX 120          /* 10 ans : début du déclin de survie */
#define SURVIE_JEUNE_AN  0.35  /* survie ANNUELLE d'un jeune */
#define SURVIE_ADULTE_AN 0.60  /* survie ANNUELLE d'un adulte */
#define MAX_POP_DEFAUT   20000000UL  /* garde-fou mémoire (20 M lapins = 80 Mo) */

typedef struct {
    uint8_t age;         /* en mois (0..180) */
    uint8_t maturite;    /* âge de maturité sexuelle, tiré entre 5 et 8 mois */
    uint8_t sexe;        /* MALE ou FEMELLE */
    uint8_t portees_an;  /* nombre de portées par an (3..9), propre à chaque femelle */
} Lapin;

typedef struct {
    Lapin *tab;
    size_t n;            /* nombre de lapins vivants */
    size_t capacite;     /* taille allouée */
    size_t max;          /* plafond de population */
} Population;

typedef struct {
    long naissances;
    long morts;
    long morts_par_age[AGE_MAX + 1];   /* âge (en mois) au décès */
} Stats;

/* générateur aléatoire rapide (xorshift64) + tables de survie */
void     sim_init(uint64_t graine);
uint32_t alea(uint32_t n);             /* entier uniforme dans [0, n-1] */

Lapin creer_lapin(void);                       /* nouveau-né, sexe aléatoire */
Lapin creer_lapin_adulte(uint8_t sexe);        /* adulte de sexe imposé */
int   est_adulte(const Lapin *l);

void pop_init(Population *p, size_t capacite, size_t max);
int  pop_ajouter(Population *p, Lapin l);      /* 1 = ok, 0 = plafond/mémoire atteint */
void pop_liberer(Population *p);

/* un mois de simulation : 0 = normal, -1 = plafond atteint */
int  simuler_mois(Population *p, Stats *s);

#endif