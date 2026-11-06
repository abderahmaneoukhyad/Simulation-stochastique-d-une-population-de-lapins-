/* Usage : ./lapins [couples_initiaux] [mois] [nb_experiences] [graine] [plafond]
 * Ex.   : ./lapins 1 120 40
 *
 * Fichiers produits (dans le dossier courant) :
 *   trajectoires.csv      population : moyenne, écart-type, IC95 % par mois
 *   trajectoires_brutes.csv population de CHAQUE expérience, par mois
 *   morts_cumules.csv     morts cumulés : moyenne, écart-type, IC95 % par mois
 *   composition.csv       jeunes / mâles adultes / femelles adultes (moyennes par mois)
 *   ages_par_mois.csv     nombre moyen de lapins par année d'âge (0..15), par mois
 *   morts_par_age.csv     âge (en mois) au décès, cumulé sur toutes les expériences */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "lapin.h"
#include "compteur.h"

#define MAX_EXP 500

/* Photographie de la population à un mois donné */
typedef struct {
    double n, morts, jeunes, males, femelles;
    long   h[16];
} Snap;

static Snap prendre(const Population *p, const Stats *s)
{
    Snap x;
    size_t ma = compter_males_adultes(p), fa = compter_femelles_adultes(p);
    x.n = (double)p->n;
    x.morts = (double)s->morts;
    x.males = (double)ma;
    x.femelles = (double)fa;
    x.jeunes = (double)(p->n - ma - fa);
    histogramme_ages(p, x.h);
    return x;
}

/* Range la photo dans les tableaux : e = expérience, m = mois */
static void ranger(const Snap *x, int e, int m, int mois,
                   double *traj, double *mortc, double *comp, double *ages)
{
    size_t L = (size_t)mois + 1;
    traj[(size_t)e * L + m]  = x->n;
    mortc[(size_t)e * L + m] = x->morts;
    comp[(size_t)m * 3 + 0] += x->jeunes;
    comp[(size_t)m * 3 + 1] += x->males;
    comp[(size_t)m * 3 + 2] += x->femelles;
    for (int a = 0; a < 16; a++) ages[(size_t)m * 16 + a] += (double)x->h[a];
}

/* Écrit moyenne, écart-type et IC 95 % de chaque mois (lignes = expériences) */
static int ecrire_stats(const char *nom, const double *mat, int nexp, int mois)
{
    size_t L = (size_t)mois + 1;
    FILE *f = fopen(nom, "w");
    if (!f) { perror(nom); return 0; }
    fprintf(f, "mois,moyenne,ecart_type,ic95_bas,ic95_haut\n");
    for (int m = 0; m <= mois; m++) {
        double moy = 0, var = 0;
        for (int e = 0; e < nexp; e++) moy += mat[(size_t)e * L + m];
        moy /= nexp;
        for (int e = 0; e < nexp; e++) {
            double d = mat[(size_t)e * L + m] - moy;
            var += d * d;
        }
        double sd = nexp > 1 ? sqrt(var / (nexp - 1)) : 0;
        double demi = 1.96 * sd / sqrt((double)nexp);
        fprintf(f, "%d,%.2f,%.2f,%.2f,%.2f\n", m, moy, sd, moy - demi, moy + demi);
    }
    fclose(f);
    return 1;
}

int main(int argc, char **argv)
{
    int couples = argc > 1 ? atoi(argv[1]) : 1;
    int mois    = argc > 2 ? atoi(argv[2]) : 120;
    int nexp    = argc > 3 ? atoi(argv[3]) : 30;
    uint64_t graine = argc > 4 ? strtoull(argv[4], NULL, 10) : (uint64_t)time(NULL);
    size_t plafond  = argc > 5 ? (size_t)strtoull(argv[5], NULL, 10) : MAX_POP_DEFAUT;

    if (couples < 1 || mois < 1 || nexp < 1 || nexp > MAX_EXP || plafond < 2) {
        fprintf(stderr, "Usage: %s [couples>=1] [mois>=1] [experiences 1..%d] [graine] [plafond]\n",
                argv[0], MAX_EXP);
        return 1;
    }

    sim_init(graine);
    printf("Couples initiaux : %d | mois : %d | experiences : %d | graine : %llu | plafond : %zu\n",
           couples, mois, nexp, (unsigned long long)graine, plafond);

    size_t L = (size_t)mois + 1;
    double *traj  = malloc((size_t)nexp * L * sizeof(double));
    double *mortc = malloc((size_t)nexp * L * sizeof(double));
    double *comp  = calloc(L * 3, sizeof(double));
    double *ages  = calloc(L * 16, sizeof(double));
    if (!traj || !mortc || !comp || !ages) { fprintf(stderr, "Mémoire insuffisante\n"); return 1; }

    long morts_age[AGE_MAX + 1] = {0};
    int eteintes = 0, plafonnees = 0;
    clock_t t0 = clock();

    for (int e = 0; e < nexp; e++) {
        Population p;
        Stats s = {0};
        pop_init(&p, 1024, plafond);

        for (int c = 0; c < couples; c++) {
            pop_ajouter(&p, creer_lapin_adulte(MALE));
            pop_ajouter(&p, creer_lapin_adulte(FEMELLE));
        }

        Snap x = prendre(&p, &s);
        ranger(&x, e, 0, mois, traj, mortc, comp, ages);

        int m = 1, stop = 0;
        for (; m <= mois; m++) {
            if (simuler_mois(&p, &s) < 0) stop = 1;
            x = prendre(&p, &s);
            ranger(&x, e, m, mois, traj, mortc, comp, ages);
            if (p.n == 0 || stop) break;
        }
        /* arrêt anticipé : on recopie le dernier état sur les mois restants */
        for (int k = m + 1; k <= mois; k++)
            ranger(&x, e, k, mois, traj, mortc, comp, ages);

        if (p.n == 0) eteintes++;
        if (stop)     plafonnees++;
        for (int a = 0; a <= AGE_MAX; a++) morts_age[a] += s.morts_par_age[a];

        if (nexp <= 5)
            printf("  exp %d : pop finale %zu (dont %zu femelles adultes), %ld naissances, %ld morts%s%s\n",
                   e + 1, p.n, compter_femelles_adultes(&p), s.naissances, s.morts,
                   p.n == 0 ? " [EXTINCTION]" : "", stop ? " [PLAFOND]" : "");
        pop_liberer(&p);
    }
    double dt = (double)(clock() - t0) / CLOCKS_PER_SEC;

    /* ---- fichiers de sortie ---- */
    if (!ecrire_stats("trajectoires.csv", traj, nexp, mois)) return 1;
    if (!ecrire_stats("morts_cumules.csv", mortc, nexp, mois)) return 1;

    FILE *f = fopen("trajectoires_brutes.csv", "w");
    if (!f) { perror("trajectoires_brutes.csv"); return 1; }
    fprintf(f, "mois");
    for (int e = 0; e < nexp; e++) fprintf(f, ",exp%d", e + 1);
    fprintf(f, "\n");
    for (int m = 0; m <= mois; m++) {
        fprintf(f, "%d", m);
        for (int e = 0; e < nexp; e++) fprintf(f, ",%.0f", traj[(size_t)e * L + m]);
        fprintf(f, "\n");
    }
    fclose(f);

    f = fopen("composition.csv", "w");
    if (!f) { perror("composition.csv"); return 1; }
    fprintf(f, "mois,jeunes,males_adultes,femelles_adultes\n");
    for (int m = 0; m <= mois; m++)
        fprintf(f, "%d,%.2f,%.2f,%.2f\n", m, comp[m * 3] / nexp,
                comp[m * 3 + 1] / nexp, comp[m * 3 + 2] / nexp);
    fclose(f);

    f = fopen("ages_par_mois.csv", "w");
    if (!f) { perror("ages_par_mois.csv"); return 1; }
    fprintf(f, "mois");
    for (int a = 0; a < 16; a++) fprintf(f, ",annee%d", a);
    fprintf(f, "\n");
    for (int m = 0; m <= mois; m++) {
        fprintf(f, "%d", m);
        for (int a = 0; a < 16; a++) fprintf(f, ",%.2f", ages[(size_t)m * 16 + a] / nexp);
        fprintf(f, "\n");
    }
    fclose(f);

    f = fopen("morts_par_age.csv", "w");
    if (!f) { perror("morts_par_age.csv"); return 1; }
    fprintf(f, "age_mois,morts\n");
    for (int a = 0; a <= AGE_MAX; a++) fprintf(f, "%d,%ld\n", a, morts_age[a]);
    fclose(f);

    printf("\nExtinctions : %d/%d (%.1f %%) | plafond atteint : %d/%d\n",
           eteintes, nexp, 100.0 * eteintes / nexp, plafonnees, nexp);
    printf("Temps de calcul : %.2f s\n", dt);

    free(traj); free(mortc); free(comp); free(ages);
    return 0;
}