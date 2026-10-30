#ifndef ZAKON_LOGGER_H
#define ZAKON_LOGGER_H

#include "settings.h"
#include <stdarg.h>

void zd_log_write(const ZDPaths *paths, const char *level, const char *scope,
                  const char *format, ...);
char *zd_log_read_recent(const ZDPaths *paths, size_t max_bytes);
int zd_log_export(const ZDPaths *paths, char *saved_path, size_t path_cap,
                  char *error, size_t error_cap);

#endif
