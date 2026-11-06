CC = gcc
CFLAGS = -O2 -Wall -Wextra

all: lapins fibonacci

lapins: main.c lapin.c compteur.c lapin.h compteur.h
	$(CC) $(CFLAGS) -o lapins main.c lapin.c compteur.c -lm

fibonacci: fibonacci.c
	$(CC) $(CFLAGS) -o fibonacci fibonacci.c

clean:
	rm -f lapins fibonacci *.csv