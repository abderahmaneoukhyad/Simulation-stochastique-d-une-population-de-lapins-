#ifndef COMPTEUR_H
#define COMPTEUR_H

#include "lapin.h"

size_t compter_femelles_adultes(const Population *p);
size_t compter_males_adultes(const Population *p);
size_t compter_femelles(const Population *p);

/* nombre de lapins par tranche d'un an : h[0..15] */
void histogramme_ages(const Population *p, long h[16]);

#endif