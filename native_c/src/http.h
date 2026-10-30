#ifndef ZAKON_HTTP_H
#define ZAKON_HTTP_H

#include "core.h"

typedef struct {
    int timeout_connect_seconds;
    int timeout_read_seconds;
    int max_attempts;
} ZDHttpSettings;

typedef struct {
    char *html;
    char error[ZD_ERROR_MAX * 2];
    int status_code;
    int attempts;
} ZDHttpResult;

/* Fetches only HTTPS pages on zakon.rada.gov.ua; uses system proxy settings. */
int zd_http_fetch_document(const char *url, const ZDHttpSettings *settings,
                           ZDHttpResult *result);
void zd_http_result_free(ZDHttpResult *result);

#endif
