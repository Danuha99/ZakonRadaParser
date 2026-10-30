#include "../src/i18n.h"
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static int failures = 0;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); failures++; } } while (0)

int main(void) {
    static const char *codes[] = {"uk", "en", "de", "fr", "es", "pt", "zh"};
    static const wchar_t *overview[] = {
        L"Огляд", L"Overview", L"Übersicht", L"Vue d’ensemble", L"Resumen", L"Visão geral", L"概览"
    };
    size_t count = 0;
    const ZDLocale *locales = zd_i18n_locales(&count);
    CHECK(locales != NULL && count == ZD_LOCALE_COUNT);
    for (size_t i = 0; i < ZD_LOCALE_COUNT; ++i) {
        CHECK(strcmp(zd_i18n_code_at(i), codes[i]) == 0);
        CHECK(strcmp(locales[i].code, codes[i]) == 0);
        CHECK(wcscmp(zd_i18n_translate(codes[i], L"Огляд"), overview[i]) == 0);
        CHECK(zd_i18n_index(codes[i]) == (int)i);
    }
    CHECK(zd_i18n_index("unknown") == 0);
    CHECK(strcmp(zd_i18n_code_at(999), "uk") == 0);
    CHECK(wcscmp(zd_i18n_translate("en", L"unregistered source"), L"unregistered source") == 0);
    CHECK(wcscmp(zd_i18n_translate("de", L"Локальних документів: %llu"), L"Lokale Dokumente: %llu") == 0);
    CHECK(wcscmp(zd_i18n_translate("en", L"%ls\r\n\r\n— %ls\r\nДжерело: %ls"),
                 L"%ls\r\n\r\n— %ls\r\nSource: %ls") == 0);
    CHECK(wcscmp(zd_i18n_translate("fr", L"Мова інтерфейсу"), L"Langue de l’interface") == 0);
    CHECK(wcscmp(zd_i18n_translate("zh", L"%ls  ·  %lld"), L"%ls  ·  %lld") == 0);
    if (failures) {
        fprintf(stderr, "%d localization test(s) failed.\n", failures);
        return 1;
    }
    puts("Localization tests passed.");
    return 0;
}
