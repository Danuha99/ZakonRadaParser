#ifndef ZAKON_DOWNLOAD_SERVICE_H
#define ZAKON_DOWNLOAD_SERVICE_H

#include "core.h"
#include "history.h"
#include "http.h"
#include "logger.h"
#include "settings.h"

typedef void (*ZDProgressCallback)(void *context, int stage, const char *message);

typedef struct {
    int success;
    int64_t history_id;
    int http_status;
    char url[ZD_URL_MAX];
    char title[ZD_TITLE_MAX];
    char saved_path[ZD_PATH_MAX];
    char error[ZD_ERROR_MAX * 2];
} ZDDownloadResult;

int zd_download_run(const char *url, const ZDSettings *settings,
                    const ZDPaths *paths, ZDSqliteApi *sqlite,
                    ZDProgressCallback progress, void *progress_context,
                    ZDDownloadResult *result);

#endif
