#ifndef ZAKON_SQLITE_API_H
#define ZAKON_SQLITE_API_H

#include <windows.h>
#include <stddef.h>
#include <stdint.h>

typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;
typedef void (__cdecl *sqlite3_destructor_type)(void *);

#define ZD_SQLITE_OK 0
#define ZD_SQLITE_ROW 100
#define ZD_SQLITE_DONE 101
#define ZD_SQLITE_OPEN_READWRITE 0x00000002
#define ZD_SQLITE_OPEN_CREATE 0x00000004
#define ZD_SQLITE_OPEN_FULLMUTEX 0x00010000
#define ZD_SQLITE_TRANSIENT ((sqlite3_destructor_type)(intptr_t)-1)

typedef struct {
    HMODULE module;
    int (__cdecl *open)(const char *, sqlite3 **);
    int (__cdecl *open_v2)(const char *, sqlite3 **, int, const char *);
    int (__cdecl *close)(sqlite3 *);
    const char *(__cdecl *errmsg)(sqlite3 *);
    int (__cdecl *prepare_v2)(sqlite3 *, const char *, int, sqlite3_stmt **, const char **);
    int (__cdecl *bind_text)(sqlite3_stmt *, int, const char *, int, sqlite3_destructor_type);
    int (__cdecl *bind_int)(sqlite3_stmt *, int, int);
    int (__cdecl *bind_int64)(sqlite3_stmt *, int, long long);
    int (__cdecl *busy_timeout)(sqlite3 *, int);
    int (__cdecl *step)(sqlite3_stmt *);
    long long (__cdecl *column_int64)(sqlite3_stmt *, int);
    const unsigned char *(__cdecl *column_text)(sqlite3_stmt *, int);
    int (__cdecl *finalize)(sqlite3_stmt *);
    int (__cdecl *exec)(sqlite3 *, const char *, void *, void *, char **);
    void (__cdecl *free_mem)(void *);
} ZDSqliteApi;

/* Loads the inbox-shipped Windows SQLite DLL; does not install or modify the OS. */
int zd_sqlite_load(ZDSqliteApi *api, char *error, size_t error_cap);
void zd_sqlite_unload(ZDSqliteApi *api);

#endif
