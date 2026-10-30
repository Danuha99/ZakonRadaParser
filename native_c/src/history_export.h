#ifndef ZAKON_HISTORY_EXPORT_H
#define ZAKON_HISTORY_EXPORT_H

#include "history.h"
#include "settings.h"

int zd_history_export_csv(const ZDPaths *paths, const ZDHistoryList *history,
                          char *saved_path, size_t path_cap,
                          char *error, size_t error_cap);

#endif
