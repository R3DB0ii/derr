// demo.c - Esempio completo di utilizzo della libreria derr.h
// gcc -Wall -Wextra -rdynamic demo.c -o demo

#define DERR_IMPLEMENTATION
#include "../derr.h"

#include <stdio.h>
#include <stdlib.h>

// Funzione di prova che genera un errore
static void funzione_pericolosa() {
    FILE *f = fopen("/path/inesistente", "r");
    if (!f) {
        // Stampa messaggio + errno
        DERR_ERROR("Impossibile aprire il file");
        // DIE_ERRNO("File mancante o inaccessibile");
    }
}

// Funzione che forza un'asserzione fallita
static void test_assert() {
    int x = 5;
    DASSERT(x == 10, "La variabile x non vale 10 come atteso!");
}

// Funzione che simula una chiamata fallita con DTRY
static void test_dtry() {
    int ret = -1; // Simuliamo un fallimento
    DTRY(ret);    // equivale a: se ret == -1 -> DIE_ERRNO(...)
}

// Funzione principale di demo
int main(int argc, char **argv) {
    // Configurazione iniziale
    derr_set_program_name(argv[0]);  // mostra il nome eseguibile nei log
    derr_set_min_level(DERR_DEBUG);  // mostra anche i DEBUG
    derr_enable_color(1);            // abilita colori ANSI
    derr_set_timestamp_utc(0);       // timestamp locale (non UTC)

    // Log a vari livelli
    DERR_DEBUG("Questo è un messaggio di DEBUG (dettagli tecnici)");
    DERR_INFO("Avvio del programma demo");
    DERR_WARN("Questo è un avviso, qualcosa non è ideale ma non blocca");

    // Esempio di log con errno (senza terminare)
    funzione_pericolosa();

    // Test DTRY (simula fallimento con -1)
    test_dtry();

    // Test di assert (uscirà con errore e backtrace)
    test_assert();

    // Questo punto non verrà mai raggiunto
    DERR_INFO("Fine programma (non si vedrà mai)");

    return 0;
}

