#define _WIN32_WINNT 0x0A00
#include "logger.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define LOG_LIMIT (2u * 1024u * 1024u)
#define LOG_KEEP 3
typedef struct { char *s; size_t n, cap; } LBuf;
typedef struct { wchar_t path[ZD_PATH_MAX]; wchar_t name[MAX_PATH]; FILETIME time; } LFile;
static SRWLOCK g_lock = SRWLOCK_INIT;

static int lb_add(LBuf *b, const char *s, size_t n) {
    if (n > (size_t)-1 - b->n - 1) return 0;
    size_t need = b->n + n + 1;
    if (need > b->cap) {
        size_t cap = b->cap ? b->cap : 512;
        while (cap < need) cap = cap > (size_t)-1 / 2 ? need : cap * 2;
        char *p = (char *)realloc(b->s, cap);
        if (!p) return 0;
        b->s = p; b->cap = cap;
    }
    memcpy(b->s + b->n, s, n); b->n += n; b->s[b->n] = 0;
    return 1;
}
static int lb_s(LBuf *b, const char *s) { return s ? lb_add(b, s, strlen(s)) : 1; }
static wchar_t *to_wide(const char *s) {
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, NULL, 0);
    if (n <= 0) return NULL;
    wchar_t *w = (wchar_t *)malloc((size_t)n * sizeof(*w));
    if (!w) return NULL;
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, w, n)) { free(w); return NULL; }
    return w;
}
static int from_wide(const wchar_t *s, char *out, size_t cap) {
    int n = WideCharToMultiByte(CP_UTF8, 0, s, -1, NULL, 0, NULL, NULL);
    if (n <= 0 || (size_t)n > cap) return 0;
    return WideCharToMultiByte(CP_UTF8, 0, s, -1, out, (int)cap, NULL, NULL) > 0;
}
static int join(char *out, size_t cap, const char *a, const char *b) {
    size_t n = strlen(a);
    while (n && (a[n-1] == '\\' || a[n-1] == '/')) n--;
    int k = snprintf(out, cap, "%.*s\\%s", (int)n, a, b);
    return k >= 0 && (size_t)k < cap;
}
static void rotate_log(const wchar_t *path) {
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &data)) return;
    ULARGE_INTEGER size; size.HighPart = data.nFileSizeHigh; size.LowPart = data.nFileSizeLow;
    if (size.QuadPart < LOG_LIMIT) return;
    wchar_t from[ZD_PATH_MAX], to[ZD_PATH_MAX];
    for (int i = LOG_KEEP; i >= 1; i--) {
        swprintf(from, ZD_PATH_MAX, L"%ls.%d.log", path, i);
        if (i == LOG_KEEP) DeleteFileW(from);
        else {
            swprintf(to, ZD_PATH_MAX, L"%ls.%d.log", path, i + 1);
            MoveFileExW(from, to, MOVEFILE_REPLACE_EXISTING);
        }
    }
    swprintf(to, ZD_PATH_MAX, L"%ls.1.log", path);
    MoveFileExW(path, to, MOVEFILE_REPLACE_EXISTING);
}
void zd_log_write(const ZDPaths *paths, const char *level, const char *scope, const char *format, ...) {
    if (!paths || !format) return;
    char utf8path[ZD_PATH_MAX];
    if (!join(utf8path, sizeof(utf8path), paths->logs, "native_c.log")) return;
    wchar_t *path = to_wide(utf8path); if (!path) return;
    char message[4096]; va_list ap; va_start(ap, format); vsnprintf(message, sizeof(message), format, ap); va_end(ap); message[sizeof(message)-1] = 0;
    SYSTEMTIME t; GetLocalTime(&t);
    char line[4608]; int n = snprintf(line, sizeof(line), "%04u-%02u-%02u %02u:%02u:%02u [%s] %s: %s\r\n", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, level ? level : "INFO", scope ? scope : "app", message);
    if (n < 0) { free(path); return; }
    if ((size_t)n >= sizeof(line)) n = (int)sizeof(line) - 1;
    AcquireSRWLockExclusive(&g_lock); rotate_log(path);
    HANDLE h = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) { DWORD wrote; WriteFile(h, line, (DWORD)n, &wrote, NULL); CloseHandle(h); }
    ReleaseSRWLockExclusive(&g_lock); free(path);
}
static char *read_tail_w(const wchar_t *path, size_t max_bytes) {
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return NULL;
    LARGE_INTEGER size;
    if (!GetFileSizeEx(h, &size) || size.QuadPart < 0) { CloseHandle(h); return NULL; }
    size_t take = size.QuadPart > (LONGLONG)max_bytes ? max_bytes : (size_t)size.QuadPart;
    LARGE_INTEGER pos; pos.QuadPart = size.QuadPart - (LONGLONG)take; SetFilePointerEx(h, pos, NULL, FILE_BEGIN);
    char *raw = (char *)malloc(take + 1);
    if (!raw) { CloseHandle(h); return NULL; }
    DWORD got = 0; BOOL ok = ReadFile(h, raw, (DWORD)take, &got, NULL); CloseHandle(h);
    if (!ok) { free(raw); return NULL; }
    raw[got] = 0; LBuf b = {0};
    if (size.QuadPart > (LONGLONG)take) {
        char *nl = (char *)memchr(raw, '\n', got);
        if (nl) {
            size_t skip = (size_t)(nl - raw) + 1;
            char amount[32]; snprintf(amount, sizeof(amount), "%llu", (unsigned long long)(take / 1024));
            lb_s(&b, "[... показано останні "); lb_s(&b, amount); lb_s(&b, " КБ ...]\r\n");
            lb_add(&b, raw + skip, got - skip);
        } else lb_add(&b, raw, got);
    } else lb_add(&b, raw, got);
    free(raw); return b.s ? b.s : (char *)calloc(1, 1);
}
char *zd_log_read_recent(const ZDPaths *paths, size_t max_bytes) {
    if (!paths) return NULL;
    if (!max_bytes) max_bytes = 512000;
    if (max_bytes > 2u * 1024u * 1024u) max_bytes = 2u * 1024u * 1024u;
    char p[ZD_PATH_MAX]; if (!join(p, sizeof(p), paths->logs, "native_c.log")) return NULL;
    wchar_t *w = to_wide(p); if (!w) return NULL;
    char *text = read_tail_w(w, max_bytes); free(w);
    return text ? text : (char *)calloc(1, 1);
}
static int cmp_lfile(const void *a, const void *b) {
    const LFile *x = (const LFile *)a, *y = (const LFile *)b;
    return CompareFileTime(&x->time, &y->time);
}
static int atomic_write(const wchar_t *path, const char *data, size_t n) {
    wchar_t temp[ZD_PATH_MAX];
    if (swprintf(temp, ZD_PATH_MAX, L"%ls.%lu.%lu.tmp", path, (unsigned long)GetCurrentProcessId(), (unsigned long)GetTickCount()) < 0) return 0;
    HANDLE h = CreateFileW(temp, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    DWORD wrote = 0; BOOL ok = n <= 0xffffffffu && WriteFile(h, data, (DWORD)n, &wrote, NULL) && wrote == (DWORD)n && FlushFileBuffers(h);
    CloseHandle(h); if (ok) ok = MoveFileExW(temp, path, MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
    if (!ok) DeleteFileW(temp); return ok;
}
int zd_log_export(const ZDPaths *paths, char *saved_path, size_t path_cap, char *error, size_t error_cap) {
    if (error && error_cap) error[0] = 0; if (saved_path && path_cap) saved_path[0] = 0;
    if (!paths || !saved_path) { if (error && error_cap) snprintf(error, error_cap, "Неправильний шлях експорту."); return 0; }
    wchar_t *dir = to_wide(paths->logs), *outdir = to_wide(paths->exported);
    if (!dir || !outdir) { free(dir); free(outdir); if (error && error_cap) snprintf(error, error_cap, "Некоректний UTF-8 шлях."); return 0; }
    wchar_t pattern[ZD_PATH_MAX]; swprintf(pattern, ZD_PATH_MAX, L"%ls\\*.log", dir);
    WIN32_FIND_DATAW fd; HANDLE find = FindFirstFileW(pattern, &fd);
    if (find == INVALID_HANDLE_VALUE) { free(dir); free(outdir); if (error && error_cap) snprintf(error, error_cap, "Немає файлів журналів для експорту."); return 0; }
    LFile files[256]; size_t count = 0;
    do {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || count >= 256) continue;
        LFile *f = &files[count];
        if (swprintf(f->path, ZD_PATH_MAX, L"%ls\\%ls", dir, fd.cFileName) < 0) continue;
        wcsncpy(f->name, fd.cFileName, MAX_PATH - 1); f->name[MAX_PATH - 1] = 0; f->time = fd.ftLastWriteTime; count++;
    } while (FindNextFileW(find, &fd));
    FindClose(find);
    if (!count) { free(dir); free(outdir); if (error && error_cap) snprintf(error, error_cap, "Немає файлів журналів для експорту."); return 0; }
    qsort(files, count, sizeof(files[0]), cmp_lfile);
    SYSTEMTIME t; GetLocalTime(&t); wchar_t outname[MAX_PATH], outfull[ZD_PATH_MAX];
    swprintf(outname, MAX_PATH, L"combined_log_%02u.%02u.%04u_%02u-%02u-%02u.txt", t.wDay,t.wMonth,t.wYear,t.wHour,t.wMinute,t.wSecond);
    swprintf(outfull, ZD_PATH_MAX, L"%ls\\%ls", outdir, outname);
    for (unsigned suffix=1; GetFileAttributesW(outfull)!=INVALID_FILE_ATTRIBUTES && suffix<100; suffix++) {
        swprintf(outname, MAX_PATH, L"combined_log_%02u.%02u.%04u_%02u-%02u-%02u_%02u.txt", t.wDay,t.wMonth,t.wYear,t.wHour,t.wMinute,t.wSecond,suffix);
        swprintf(outfull, ZD_PATH_MAX, L"%ls\\%ls", outdir, outname);
    }
    LBuf b={0}; char header[256];
    int hn=snprintf(header,sizeof(header),"Зведений журнал ZakonDownloader\r\nСформовано: %02u.%02u.%04u %02u:%02u:%02u\r\nФайлів: %llu\r\n========================================================================\r\n\r\n",t.wDay,t.wMonth,t.wYear,t.wHour,t.wMinute,t.wSecond,(unsigned long long)count);
    int ok=hn>0&&lb_add(&b,header,(size_t)hn);
    for(size_t i=0;i<count&&ok;i++){
        char name[MAX_PATH]; if(!from_wide(files[i].name,name,sizeof(name)))strcpy(name,"unknown.log");
        ok=lb_s(&b,"=== ")&&lb_s(&b,name)&&lb_s(&b," ===\r\n");
        char *text=ok?read_tail_w(files[i].path,512000):NULL;
        if(text){ok=lb_s(&b,text)&&lb_s(&b,"\r\n\r\n");free(text);}
        else if(ok)ok=lb_s(&b,"[Не вдалося прочитати файл журналу]\r\n\r\n");
        if(b.n>16u*1024u*1024u)ok=0;
    }
    if(ok)ok=atomic_write(outfull,b.s?b.s:"",b.n);
    if(!ok&&error&&error_cap)snprintf(error,error_cap,"Не вдалося сформувати або атомарно зберегти зведений журнал.");
    if(ok&&!from_wide(outfull,saved_path,path_cap))ok=0;
    free(b.s);free(dir);free(outdir);return ok;
}
