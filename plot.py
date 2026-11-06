""" python3 plot.py [mois] [nb_experiences]

Les graphes :
  graphe_population.png    population moyenne + IC 95 % (1, 2, 5, 10 couples)
  graphe_extinction.png    taux d'extinction selon les conditions initiales
  graphe_variabilite.png   trajectoires individuelles + population finale par expérience
  graphe_ages.png          âge des lapins vivants (final + évolution dans le temps)
  graphe_composition.png   jeunes / mâles adultes / femelles adultes
  graphe_morts_temps.png   morts cumulés dans le temps
  graphe_morts_age.png     âge au décès
  graphe_performance.png   temps de calcul (compare plusieurs machines : voir plus bas)
"""
import glob
import os
import re
import socket
import subprocess
import sys
import tempfile
import time

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

MOIS = int(sys.argv[1]) if len(sys.argv) > 1 else 60
NEXP = int(sys.argv[2]) if len(sys.argv) > 2 else 30
COUPLES = [1, 2, 5, 10]
REF = 5                     # configuration de référence pour les graphes détaillés
SEED = 12345                # graine fixe : résultats reproductibles
PLAFOND = 20_000_000
EXE = os.path.abspath("./lapins")
COULEUR = {c: f"C{i}" for i, c in enumerate(COUPLES)}


def csv(d, nom):
    return np.loadtxt(os.path.join(d, nom), delimiter=",", skiprows=1)


def lancer(couples, mois, nexp):
    """Exécute ./lapins dans un dossier temporaire et charge tous les CSV."""
    with tempfile.TemporaryDirectory() as d:
        sortie = subprocess.run([EXE, str(couples), str(mois), str(nexp), str(SEED), str(PLAFOND)],
                                cwd=d, capture_output=True, text=True, check=True).stdout
        ext = re.search(r"Extinctions : (\d+)/(\d+)", sortie)
        pla = re.search(r"plafond atteint : (\d+)/(\d+)", sortie)
        traj = csv(d, "trajectoires.csv")
        mor = csv(d, "morts_cumules.csv")
        return {
            "mois": traj[:, 0], "moy": traj[:, 1],
            "bas": np.maximum(traj[:, 3], 0.5), "haut": traj[:, 4],
            "brut": csv(d, "trajectoires_brutes.csv")[:, 1:],
            "mort_moy": mor[:, 1], "mort_bas": np.maximum(mor[:, 3], 0.5), "mort_haut": np.maximum(mor[:, 4], 0.5),
            "comp": csv(d, "composition.csv")[:, 1:],
            "ages": csv(d, "ages_par_mois.csv")[:, 1:],
            "morts_age": csv(d, "morts_par_age.csv")[:, 1],
            "ext": 100.0 * int(ext.group(1)) / int(ext.group(2)),
            "plaf": int(pla.group(1)),
        }


def sauver(fig, nom):
    fig.tight_layout()
    fig.savefig(nom, dpi=150)
    plt.close(fig)
    print("  ->", nom)


# simulations
print(f"Simulations : {MOIS} mois, {NEXP} expériences par condition")
R = {}
for c in COUPLES:
    R[c] = lancer(c, MOIS, NEXP)
    print(f"  {c:2d} couple(s) : extinction {R[c]['ext']:.1f} %, plafond atteint {R[c]['plaf']}/{NEXP}")
if REF not in R:
    REF = COUPLES[-1]
mois = R[COUPLES[0]]["mois"]

# 1. population moyenne
fig, ax = plt.subplots(figsize=(8, 5))
for c in COUPLES:
    r = R[c]
    ax.plot(mois, r["moy"], color=COULEUR[c], label=f"{c} couple(s) au départ")
    ax.fill_between(mois, r["bas"], r["haut"], color=COULEUR[c], alpha=0.2)
ax.set_yscale("log")
ax.set_xlabel("Mois")
ax.set_ylabel("Nombre de lapins (moyenne)")
ax.set_title(f"Évolution de la population ({NEXP} expériences, IC 95 %)")
ax.legend()
ax.grid(True, which="both", alpha=0.3)
sauver(fig, "graphe_population.png")

# 2. extinction
fig, ax = plt.subplots(figsize=(6, 4))
ax.bar([str(c) for c in COUPLES], [R[c]["ext"] for c in COUPLES], color=[COULEUR[c] for c in COUPLES])
ax.set_xlabel("Couples au départ")
ax.set_ylabel("Extinctions (%)")
ax.set_title(f"Taux d'extinction ({NEXP} expériences)")
sauver(fig, "graphe_extinction.png")

# 3. variabilité entre expériences
r = R[COUPLES[0]]
fig, (a1, a2) = plt.subplots(1, 2, figsize=(12, 4.5))
for e in range(r["brut"].shape[1]):
    a1.plot(mois, r["brut"][:, e], color="gray", alpha=0.35, lw=0.8)
a1.plot(mois, r["moy"], color=COULEUR[COUPLES[0]], lw=2.5, label="moyenne")
a1.set_yscale("symlog", linthresh=10)
a1.set_xlabel("Mois")
a1.set_ylabel("Nombre de lapins")
a1.set_title(f"{NEXP} expériences individuelles ({COUPLES[0]} couple au départ)")
a1.legend()
a1.grid(True, which="both", alpha=0.3)

finales = r["brut"][-1, :]
log_f = np.log10(finales + 1)
a2.hist(log_f, bins=20, color=COULEUR[COUPLES[0]], edgecolor="white")
a2.set_xlabel("log10(population finale + 1)   (0 = extinction)")
a2.set_ylabel("Nombre d'expériences")
a2.set_title(f"Population finale au mois {int(mois[-1])} : variabilité entre expériences")
sauver(fig, "graphe_variabilite.png")

# 4. âge des lapins vivants
ages = R[REF]["ages"]                       # (mois, 16 années d'âge)
classes = np.column_stack([ages[:, 0], ages[:, 1], ages[:, 2], ages[:, 3], ages[:, 4:].sum(axis=1)])
noms = ["< 1 an", "1-2 ans", "2-3 ans", "3-4 ans", "4 ans et +"]
part = 100.0 * classes / np.maximum(classes.sum(axis=1, keepdims=True), 1)

fig, (a1, a2) = plt.subplots(1, 2, figsize=(12, 4.5))
a1.bar(noms, part[-1], color="C0")
a1.set_ylabel("% des lapins vivants")
a1.set_title(f"Âge des lapins au mois {int(mois[-1])} ({REF} couples au départ)")
a2.stackplot(mois, part.T, labels=noms)
a2.set_xlabel("Mois")
a2.set_ylabel("% des lapins vivants")
a2.set_ylim(0, 100)
a2.set_title("Répartition par âge au cours du temps")
a2.legend(loc="lower left", fontsize=8)
sauver(fig, "graphe_ages.png")

# 5. composition
comp = R[REF]["comp"]
fig, ax = plt.subplots(figsize=(8, 5))
for k, nom in enumerate(["Jeunes (non matures)", "Mâles adultes", "Femelles adultes"]):
    ax.plot(mois, np.maximum(comp[:, k], 0.5), label=nom)
ax.set_yscale("log")
ax.set_xlabel("Mois")
ax.set_ylabel("Nombre de lapins (moyenne)")
ax.set_title(f"Composition de la population ({REF} couples au départ)")
ax.legend()
ax.grid(True, which="both", alpha=0.3)
sauver(fig, "graphe_composition.png")

# 6. morts cumulés dans le temps
fig, ax = plt.subplots(figsize=(8, 5))
for c in COUPLES:
    r = R[c]
    ax.plot(mois, np.maximum(r["mort_moy"], 0.5), color=COULEUR[c], label=f"{c} couple(s) au départ")
    ax.fill_between(mois, r["mort_bas"], r["mort_haut"], color=COULEUR[c], alpha=0.2)
ax.set_yscale("log")
ax.set_xlabel("Mois")
ax.set_ylabel("Morts cumulés (moyenne)")
ax.set_title(f"Nombre de lapins morts au cours du temps ({NEXP} expériences, IC 95 %)")
ax.legend()
ax.grid(True, which="both", alpha=0.3)
sauver(fig, "graphe_morts_temps.png")

# 7. âge au décès
morts = R[COUPLES[0]]["morts_age"]
fig, ax = plt.subplots(figsize=(8, 4))
ax.bar(range(len(morts)), morts, width=1.0)
ax.set_yscale("log")
ax.set_xlabel("Âge au décès (mois)")
ax.set_ylabel("Nombre de morts (cumulé)")
ax.set_title(f"Répartition des décès par âge ({COUPLES[0]} couple au départ, {NEXP} expériences)")
sauver(fig, "graphe_morts_age.png")

# 8. performance
print("Mesure des performances...")
hote = socket.gethostname()
points = sorted(set(int(round(MOIS * k / 5)) for k in range(1, 6)))
temps = []
for m in points:
    t0 = time.perf_counter()
    subprocess.run([EXE, str(REF), str(m), str(NEXP), str(SEED), str(PLAFOND)],
                   capture_output=True, check=True)
    temps.append(time.perf_counter() - t0)
np.savetxt(f"perf_{hote}.csv", np.column_stack([points, temps]), delimiter=",",
           header="mois,secondes", comments="")

fig, ax = plt.subplots(figsize=(7, 4.5))
for fichier in sorted(glob.glob("perf_*.csv")):
    d = np.loadtxt(fichier, delimiter=",", skiprows=1, ndmin=2)
    ax.plot(d[:, 0], d[:, 1], marker="o", label=fichier[5:-4])
ax.set_yscale("log")
ax.set_xlabel("Mois simulés")
ax.set_ylabel("Temps de calcul (s, échelle log)")
ax.set_title(f"Performance ({REF} couples, {NEXP} expériences)")
ax.legend(title="Machine")
ax.grid(True, which="both", alpha=0.3)
sauver(fig, "graphe_performance.png")
print("Terminé.")
