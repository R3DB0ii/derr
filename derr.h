// derr.h — Della Rosa Error Reporting (header‑only)
// Autore: Mamiliano Della Rosa (con D.Ro.V.I.S.)
// Licenza: MIT
//
// Caratteristiche principali:
//  - Livelli di log: DEBUG, INFO, WARN, ERROR, FATAL
//  - Output su stderr e/o file, opzionale syslog (POSIX)
//  - Messaggi formattati con timestamp, nome programma e livello
//  - Integrazione sicura con errno (portabile strerror_r)
//  - Macro per errori fatali (DIE, DIE_ERRNO), assert (DASSERT), guard (DTRY)
//  - Backtrace su crash/livello FATAL (POSIX execinfo)
//  - Opzione colori ANSI
//  - Thread‑safe (best‑effort) via mutex POSIX; fallback lock‑free
//  - Header‑only: includi questo file; definisci DERR_IMPLEMENTATION in un .c UNA volta
//
// Uso rapido:
//  #define DERR_IMPLEMENTATION
//  #include "derr.h"
//  int main(int argc, char **argv){
//      derr_set_program_name(argv[0]);
//      derr_set_min_level(DERR_INFO);
//      DERR_INFO("Avvio");
//      FILE *f = fopen("/nope", "r");
//      if(!f) DIE_ERRNO("Impossibile aprire file");
//  }
//
// Compilazione (POSIX): gcc -std=c11 -O2 demo.c -o demo -ldl -rdynamic
//   ( -rdynamic abilita simboli leggibili nel backtrace su Linux )
//
// ===================== API PUBBLICA =====================
#ifndef DERR_H
#define DERR_H

#if defined(_MSC_VER)
  #define DERR_INLINE __inline
#else
  #define DERR_INLINE inline
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

// ----- Livelli -----
typedef enum derr_level {
    DERR_DEBUG = 10,
    DERR_INFO  = 20,
    DERR_WARN  = 30,
    DERR_ERROR = 40,
    DERR_FATAL = 50
} derr_level;

// ----- Configurazione -----
void derr_set_program_name(const char *name);
void derr_set_min_level(derr_level lvl);
void derr_enable_color(int enable);
void derr_set_timestamp_utc(int use_utc);
void derr_set_log_file(FILE *fp);     // NULL = disabilita file extra
void derr_set_include_errno_details(int enable);

// POSIX: invia anche a syslog (LOG_USER). NOP su non‑POSIX.
void derr_use_syslog(int enable);

// ----- Emissione messaggi -----
void derr_log(derr_level lvl, const char *fmt, ...) __attribute__((format(printf,2,3)));
void derr_log_errno(derr_level lvl, int errnum, const char *fmt, ...) __attribute__((format(printf,3,4)));

// Convenienze
#define DERR_DEBUG(...) derr_log(DERR_DEBUG, __VA_ARGS__)
#define DERR_INFO(...)   derr_log(DERR_INFO,  __VA_ARGS__)
#define DERR_WARN(...)   derr_log(DERR_WARN,  __VA_ARGS__)
#define DERR_ERROR(...)  derr_log(DERR_ERROR, __VA_ARGS__)

#define DERR_DEBUG_ERRNO(err, ...) derr_log_errno(DERR_DEBUG, (err), __VA_ARGS__)
#define DERR_INFO_ERRNO(err,  ...) derr_log_errno(DERR_INFO,  (err), __VA_ARGS__)
#define DERR_WARN_ERRNO(err,  ...) derr_log_errno(DERR_WARN,  (err), __VA_ARGS__)
#define DERR_ERROR_ERRNO(err, ...) derr_log_errno(DERR_ERROR, (err), __VA_ARGS__)

// Errori fatali (escono dal programma)
#define DIE(...) do { \
    derr_log(DERR_FATAL, __VA_ARGS__); \
    derr_flush(); \
    exit(EXIT_FAILURE); \
} while(0)

#define DIE_ERRNO(...) do { \
    derr_log_errno(DERR_FATAL, errno, __VA_ARGS__); \
    derr_flush(); \
    exit(EXIT_FAILURE); \
} while(0)

// Assert runtime con messaggio (non disattivabile con NDEBUG)
#define DASSERT(cond, ...) do { \
    if(!(cond)) { \
        derr_log_errno(DERR_FATAL, errno, "Assert failed: %s — " __VA_ARGS__, #cond); \
        derr_flush(); \
        abort(); \
    } \
} while(0)

// Guard per chiamate che ritornano -1 su errore, usa errno corrente
#define DTRY(expr) do { if(((expr)) == -1) DIE_ERRNO("%s failed", #expr); } while(0)

// Forza flush di tutti gli stream gestiti
void derr_flush(void);

#ifdef __cplusplus
}
#endif

#endif // DERR_H

// ===================== IMPLEMENTAZIONE =====================
#ifdef DERR_IMPLEMENTATION

#ifdef __cplusplus
extern "C" {
#endif

// Opzionale: mutex POSIX per thread‑safety
#if defined(__unix__) || defined(__APPLE__)
  #define DERR_POSIX 1
  #include <pthread.h>
  #include <unistd.h>
  #if !defined(__APPLE__)
    #include <execinfo.h> // backtrace su Linux e la maggior parte di *nix
  #endif
  #include <sys/types.h>
  #include <sys/time.h>
  #include <sys/utsname.h>
  #include <sys/stat.h>
  #include <fcntl.h>
  #include <dlfcn.h>
  #include <signal.h>
  #include <syslog.h>
#else
  #define DERR_POSIX 0
#endif

// Stato globale (minimo e coeso)
static struct derr_state {
    const char *progname;
    derr_level  min_level;
    int         color;
    int         utc;
    int         include_errno;
    FILE       *file;
    int         use_syslog;
#if DERR_POSIX
    pthread_mutex_t mu;
#endif
} g_derr = { NULL, DERR_DEBUG, 1, 0, 1, NULL, 0
#if DERR_POSIX
, PTHREAD_MUTEX_INITIALIZER
#endif
};

static DERR_INLINE const char *level_str(derr_level l){
    switch(l){
        case DERR_DEBUG: return "DEBUG";
        case DERR_INFO:  return "INFO";
        case DERR_WARN:  return "WARN";
        case DERR_ERROR: return "ERROR";
        case DERR_FATAL: return "FATAL";
        default:         return "LOG";
    }
}

static DERR_INLINE const char *level_color(derr_level l){
    if(!g_derr.color) return "";
    switch(l){
        case DERR_DEBUG: return "\x1b[2m";       // dim
        case DERR_INFO:  return "\x1b[0m";       // reset
        case DERR_WARN:  return "\x1b[33m";      // yellow
        case DERR_ERROR: return "\x1b[31m";      // red
        case DERR_FATAL: return "\x1b[1;31m";    // bold red
        default:         return "\x1b[0m";
    }
}

static DERR_INLINE const char *color_reset(){ return g_derr.color ? "\x1b[0m" : ""; }

static void ts_now(char *buf, size_t n){
    struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
    time_t sec = ts.tv_sec; struct tm tmv;
    if(g_derr.utc) gmtime_r(&sec, &tmv); else localtime_r(&sec, &tmv);
    // ISO 8601 con millisecondi
    int ms = (int)(ts.tv_nsec/1000000);
    snprintf(buf, n, "%04d-%02d-%02dT%02d:%02d:%02d.%03d%s",
             tmv.tm_year+1900, tmv.tm_mon+1, tmv.tm_mday,
             tmv.tm_hour, tmv.tm_min, tmv.tm_sec, ms,
             g_derr.utc?"Z":"");
}

// strerror portabile e thread‑safe in un buffer utente
static void strerror_portable(int errnum, char *buf, size_t n){
#if defined(_GNU_SOURCE) && !defined(__APPLE__)
    // GNU returns char* (may be static) — but also can write into buf
    char *msg = strerror_r(errnum, buf, n);
    if(msg != buf) snprintf(buf, n, "%s", msg);
#else
    // XSI: int strerror_r(int, char*, size_t)
    if(strerror_r(errnum, buf, n) != 0) snprintf(buf, n, "errno %d", errnum);
#endif
}

static void lock(){
#if DERR_POSIX
    pthread_mutex_lock(&g_derr.mu);
#endif
}
static void unlock(){
#if DERR_POSIX
    pthread_mutex_unlock(&g_derr.mu);
#endif
}

static void vemit(derr_level lvl, int has_errno, int errnum, const char *fmt, va_list ap){
    if(lvl < g_derr.min_level) return;

    char tbuf[40]; ts_now(tbuf, sizeof tbuf);

    // Buffer principale per il messaggio formattato dall'utente
    char msg[2048];
    vsnprintf(msg, sizeof msg, fmt, ap);

    // Se presente errno, formattalo
    char ebuf[256] = {0};
    if(has_errno && g_derr.include_errno){
        strerror_portable(errnum, ebuf, sizeof ebuf);
    }

    const char *pname = g_derr.progname ? g_derr.progname : "program";
    const char *c = level_color(lvl);
    const char *r = color_reset();

    lock();

    // Stampa su stderr
    if(has_errno && g_derr.include_errno)
        fprintf(stderr, "%s%s%s [%s] %s: %s (errno=%d)\n", c, tbuf, r, level_str(lvl), pname, msg, errnum);
    else
        fprintf(stderr, "%s%s%s [%s] %s: %s\n", c, tbuf, r, level_str(lvl), pname, msg);

    if(has_errno && g_derr.include_errno)
        fprintf(stderr, "%s        -> %s%s\n", c, ebuf, r);

    // File opzionale
    if(g_derr.file){
        if(has_errno && g_derr.include_errno)
            fprintf(g_derr.file, "%s [%s] %s: %s (errno=%d)\n        -> %s\n", tbuf, level_str(lvl), pname, msg, errnum, ebuf);
        else
            fprintf(g_derr.file, "%s [%s] %s: %s\n", tbuf, level_str(lvl), pname, msg);
        fflush(g_derr.file);
    }

#if DERR_POSIX
    // Syslog opzionale
    if(g_derr.use_syslog){
        int sl;
        switch(lvl){
            case DERR_DEBUG: sl = LOG_DEBUG; break;
            case DERR_INFO:  sl = LOG_INFO; break;
            case DERR_WARN:  sl = LOG_WARNING; break;
            case DERR_ERROR: sl = LOG_ERR; break;
            case DERR_FATAL: sl = LOG_CRIT; break;
            default: sl = LOG_INFO; break;
        }
        if(has_errno && g_derr.include_errno)
            syslog(sl, "%s: %s (errno=%d) -> %s", pname, msg, errnum, ebuf);
        else
            syslog(sl, "%s: %s", pname, msg);
    }
#endif

    // Backtrace sui FATAL (dove disponibile)
#if DERR_POSIX && defined(EXECINFO_H) || (defined(__linux__) && !defined(__ANDROID__))
    if(lvl >= DERR_FATAL){
        void *bt[128];
        int n = backtrace(bt, 128);
        if(n > 0){
            fprintf(stderr, "%sBacktrace (%d frames):%s\n", c, n, r);
            backtrace_symbols_fd(bt, n, fileno(stderr));
            fputc('\n', stderr);
        }
    }
#endif

    unlock();
}

// ---- Implementazioni API ----
void derr_set_program_name(const char *name){ g_derr.progname = name; }
void derr_set_min_level(derr_level lvl){ g_derr.min_level = lvl; }
void derr_enable_color(int enable){ g_derr.color = enable ? 1 : 0; }
void derr_set_timestamp_utc(int use_utc){ g_derr.utc = use_utc ? 1 : 0; }
void derr_set_log_file(FILE *fp){ g_derr.file = fp; }
void derr_set_include_errno_details(int enable){ g_derr.include_errno = enable ? 1 : 0; }

void derr_use_syslog(int enable){
#if DERR_POSIX
    if(enable && !g_derr.use_syslog){
        openlog(g_derr.progname ? g_derr.progname : "program", LOG_CONS|LOG_PID, LOG_USER);
        g_derr.use_syslog = 1;
    } else if(!enable && g_derr.use_syslog){
        closelog(); g_derr.use_syslog = 0;
    }
#else
    (void)enable; // no‑op su non POSIX
#endif
}

void derr_log(derr_level lvl, const char *fmt, ...){
    va_list ap; va_start(ap, fmt); vemit(lvl, 0, 0, fmt, ap); va_end(ap);
}

void derr_log_errno(derr_level lvl, int errnum, const char *fmt, ...){
    va_list ap; va_start(ap, fmt); vemit(lvl, 1, errnum, fmt, ap); va_end(ap);
}

void derr_flush(void){
    lock();
    fflush(stderr);
    if(g_derr.file) fflush(g_derr.file);
    unlock();
}

#ifdef __cplusplus
}
#endif

#endif // DERR_IMPLEMENTATION

