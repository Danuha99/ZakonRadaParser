#ifndef ZAKON_HISTORY_H
#define ZAKON_HISTORY_H

#include "sqlite_api.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int64_t id;
    char *url;
    char *status;
    char *started_at;
    char *finished_at;
    char *document_title;
    char *saved_path;
    char *error_message;
} ZDHistoryEntry;

typedef struct {
    ZDHistoryEntry *items;
    size_t count;
} ZDHistoryList;

int zd_history_start(ZDSqliteApi *api, const char *db_path, const char *url,
                     int64_t *entry_id, char *error, size_t error_cap);
int zd_history_finish(ZDSqliteApi *api, const char *db_path, int64_t entry_id,
                      const char *status, const char *title, const char *path,
                      const char *message, char *error, size_t error_cap);
int zd_history_load(ZDSqliteApi *api, const char *db_path, ZDHistoryList *list,
                    char *error, size_t error_cap);
void zd_history_free(ZDHistoryList *list);

#endif
