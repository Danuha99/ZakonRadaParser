#ifndef ZAKON_LOCAL_DOCUMENT_H
#define ZAKON_LOCAL_DOCUMENT_H

#include <windows.h>
#include <stddef.h>

#define ZD_LOCAL_TEXT_LIMIT (64u * 1024u * 1024u)
#define ZD_LOCAL_DOC_LIMIT 20000u

typedef struct {
    wchar_t *path;
    wchar_t *name;
    ULONGLONG bytes;
    FILETIME modified;
} ZDLocalDocument;

typedef struct {
    ZDLocalDocument *items;
    size_t count;
} ZDLocalDocumentList;

typedef struct {
    wchar_t *full_text;
    const wchar_t *clean_text; /* points inside full_text */
    wchar_t *title;
    wchar_t *metadata;
    char source_url[2048];
} ZDLoadedDocument;

int zd_local_documents_load(const wchar_t *folder, ZDLocalDocumentList *list,
                            char *error, size_t error_cap);
void zd_local_documents_free(ZDLocalDocumentList *list);
int zd_local_document_read(const wchar_t *path, ZDLoadedDocument *doc,
                           char *error, size_t error_cap);
void zd_loaded_document_free(ZDLoadedDocument *doc);

#endif
