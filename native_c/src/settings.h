#ifndef ZAKON_SETTINGS_H
#define ZAKON_SETTINGS_H

#include <stddef.h>
#define ZD_PATH_MAX 32768

typedef struct {
    int request_timeout_connect;
    int request_timeout_read;
    int max_retry_attempts;
    char log_level[16];
    char theme[16];
    char ui_mode[16];
    char language[16];
    char documents_dir[ZD_PATH_MAX];
} ZDSettings;

typedef struct {
    char base[ZD_PATH_MAX];
    char data[ZD_PATH_MAX];
    char documents[ZD_PATH_MAX];
    char logs[ZD_PATH_MAX];
    char exported[ZD_PATH_MAX];
    char history[ZD_PATH_MAX];
    char config[ZD_PATH_MAX];
    char settings_file[ZD_PATH_MAX];
    char history_db[ZD_PATH_MAX];
} ZDPaths;

void zd_settings_defaults(ZDSettings *settings);
int zd_settings_load(const char *path, ZDSettings *settings, char *error, size_t error_cap);
int zd_settings_save(const char *path, const ZDSettings *settings, char *error, size_t error_cap);
int zd_paths_init(ZDPaths *paths, char *error, size_t error_cap);
int zd_ensure_directory_utf8(const char *path);

#endif
