#ifndef ZAKON_READER_STORE_H
#define ZAKON_READER_STORE_H

#include "sqlite_api.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int64_t position;
    char *label;
} ZDReaderBookmark;

typedef struct {
    ZDReaderBookmark *items;
    size_t count;
} ZDReaderBookmarkList;

int zd_reader_progress_get(ZDSqliteApi *api, const char *db_path,
                           const char *file_path, int64_t *position,
                           char *error, size_t error_cap);
int zd_reader_progress_put(ZDSqliteApi *api, const char *db_path,
                           const char *file_path, int64_t position,
                           char *error, size_t error_cap);
int zd_reader_bookmarks_load(ZDSqliteApi *api, const char *db_path,
                             const char *file_path, ZDReaderBookmarkList *list,
                             char *error, size_t error_cap);
int zd_reader_bookmark_put(ZDSqliteApi *api, const char *db_path,
                           const char *file_path, int64_t position,
                           const char *label, char *error, size_t error_cap);
int zd_reader_bookmark_delete(ZDSqliteApi *api, const char *db_path,
                              const char *file_path, int64_t position,
                              char *error, size_t error_cap);
void zd_reader_bookmarks_free(ZDReaderBookmarkList *list);

#endif
