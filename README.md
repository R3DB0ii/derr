# DERR — Libreria di Error Handling per C

## Introduzione

La libreria **DERR** è una soluzione *header-only* per gestire errori e messaggi di log in C in modo professionale, leggibile e robusto. Supporta livelli di log, messaggi con timestamp, colori ANSI, descrizioni di `errno`, salvataggio su file, syslog (su sistemi POSIX) e perfino backtrace (dove disponibile).

Questa libreria è stata progettata per sostituire i classici `printf` e `perror`, offrendo un sistema centralizzato e coerente per il logging e la gestione degli errori nei programmi C.

---

## Funzionalità principali

- ✅ Livelli di log: **DEBUG, INFO, WARN, ERROR, FATAL**
- ✅ Funzioni per log normali e con `errno`
- ✅ Macro rapide per non scrivere troppo codice
- ✅ Funzione `DIE()` per errori fatali con `exit(EXIT_FAILURE)`
- ✅ Assert personalizzato `DASSERT()`
- ✅ Opzione `DTRY(call)` per gestire facilmente ritorni `-1`
- ✅ Timestamp in formato ISO-8601 (locale o UTC)
- ✅ Colori ANSI configurabili
- ✅ Output su `stderr`, su file o syslog (POSIX)
- ✅ Backtrace automatico in caso di crash (POSIX con `execinfo.h`)

---

## Come usare la libreria

### 1. Inclusione

Copia il file `derr.h` nel tuo progetto e includilo:

```c
#define DERR_IMPLEMENTATION
#include "derr.h"
```

> ⚠️ Definisci **DERR_IMPLEMENTATION** in **un solo file .c** del progetto, altrimenti il linker ti darà errori di simboli duplicati.

Negli altri file puoi semplicemente fare:

```c
#include "derr.h"
```

---

### 2. Configurazione

Prima di iniziare a loggare, puoi configurare il comportamento della libreria:

```c
derr_set_program_name(argv[0]);   // Nome del programma nei log
derr_set_min_level(DERR_INFO);   // Filtra messaggi: qui nasconde DEBUG
derr_enable_color(1);            // Abilita colori ANSI
derr_set_timestamp_utc(0);       // Timestamp locale (1 per UTC)
// derr_use_syslog(1);           // Abilita syslog (solo POSIX)
```

---

### 3. Log di base

```c
DERR_INFO("Programma avviato");
DERR_DEBUG("Valore di x=%d", x);
DERR_WARN("Uso memoria alto: %zu MB", memory);
DERR_ERROR("File non trovato");
```

Output tipico:
```
2025-08-20T14:32:10 [INFO]  ./programma: Programma avviato
2025-08-20T14:32:10 [WARN]  ./programma: Uso memoria alto: 512 MB
2025-08-20T14:32:10 [ERROR] ./programma: File non trovato
```

---

### 4. Errori fatali

```c
if (!fopen("/percorso/sbagliato", "r")) {
    DIE_ERRNO("Apertura file fallita");
}
```

Questo stampa:
```
2025-08-20T14:33:00 [FATAL] ./programma: Apertura file fallita: No such file or directory (errno=2)
```

E termina il programma con `exit(EXIT_FAILURE)`.

---

### 5. Assert personalizzati

```c
DASSERT(ptr != NULL, "Il puntatore non deve essere NULL");
```

Se la condizione è falsa:
- Stampa un messaggio `FATAL`
- Fornisce file e linea dove l’errore è avvenuto
- (POSIX) Mostra un backtrace
- Termina con `abort()`

---

### 6. Gestione chiamate a rischio con `DTRY`

```c
int fd = DTRY(open("/tmp/file.txt", O_RDONLY));
```

Se `open()` ritorna `-1`, la libreria stampa l’errore con `errno` e termina.

---

### 7. Output su file

Puoi redirigere i log su un file:

```c
FILE *logf = fopen("program.log", "a");
derr_set_log_file(logf);
```

---

## API Dettagliata

### Livelli di log
```c
typedef enum {
    DERR_DEBUG,
    DERR_INFO,
    DERR_WARN,
    DERR_ERROR,
    DERR_FATAL
} derr_level;
```

### Funzioni principali
```c
void derr_set_program_name(const char *name);
void derr_set_min_level(derr_level level);
void derr_enable_color(int enable);
void derr_set_timestamp_utc(int enable);
void derr_set_log_file(FILE *f);
void derr_use_syslog(int enable);
void derr_set_include_errno_details(int enable);
```

### Logging diretto
```c
void derr_log(derr_level level, const char *fmt, ...);
void derr_log_errno(derr_level level, int errnum, const char *fmt, ...);
```

### Macro
```c
DERR_DEBUG("...");
DERR_INFO("...");
DERR_WARN("...");
DERR_ERROR("...");
DERR_FATAL("...");

DIE("Messaggio fatale");
DIE_ERRNO("Messaggio con errno");

DASSERT(condizione, "messaggio");
DTRY(funzione());
```

---

## Portabilità

- **POSIX (Linux/macOS):** supporta backtrace e syslog
- **Windows:** funziona tutto tranne backtrace e syslog
- Richiede compilazione con `-rdynamic` su Linux per simboli leggibili nel backtrace

---

## Esempio completo

```c
#include <stdio.h>
#define DERR_IMPLEMENTATION
#include "derr.h"

int main(int argc, char **argv) {
    derr_set_program_name(argv[0]);
    derr_set_min_level(DERR_DEBUG);
    derr_enable_color(1);

    DERR_INFO("Avvio del programma");

    FILE *f = fopen("/non/esiste.txt", "r");
    if (!f) DIE_ERRNO("Errore apertura file");

    return 0;
}
```

---

## Conclusione

Con **DERR**, la gestione degli errori in C diventa chiara, coerente e potente. Non più `printf` sparsi o `perror` confusi, ma un sistema uniforme che ti permette di:
- Capire subito il contesto dell’errore
- Loggare in modo leggibile e professionale
- Migliorare il debugging grazie a backtrace e livelli di log

---

👉 Ora puoi integrare `derr.h` nei tuoi progetti C e avere subito un logging professionale all’altezza di software enterprise.

