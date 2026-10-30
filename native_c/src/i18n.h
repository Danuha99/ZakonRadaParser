#ifndef ZAKON_I18N_H
#define ZAKON_I18N_H

#include <stddef.h>
#include <wchar.h>

typedef struct {
    const char *code;
    const wchar_t *native_name;
} ZDLocale;

#define ZD_LOCALE_COUNT 7

const ZDLocale *zd_i18n_locales(size_t *count);
const char *zd_i18n_code_at(size_t index);
int zd_i18n_index(const char *code);
const wchar_t *zd_i18n_translate(const char *locale, const wchar_t *source);

#endif
