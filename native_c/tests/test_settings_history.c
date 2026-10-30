#include "../src/core.h"
#include "../src/history.h"
#include "../src/reader_store.h"
#include "../src/settings.h"
#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); failures++; } } while (0)

static int to_utf8(const wchar_t *input, char *output, size_t cap) {
    int n = WideCharToMultiByte(CP_UTF8, 0, input, -1, NULL, 0, NULL, NULL);
    if (n <= 0 || (size_t)n > cap) return 0;
    return WideCharToMultiByte(CP_UTF8, 0, input, -1, output, (int)cap, NULL, NULL) > 0;
}
static int write_utf8(const wchar_t *path, const char *text) {
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    DWORD size = (DWORD)strlen(text), written = 0;
    BOOL ok = WriteFile(h, text, size, &written, NULL) && written == size;
    CloseHandle(h);
    return ok != 0;
}

int main(void) {
    wchar_t root[ZD_PATH_MAX], temp_file[ZD_PATH_MAX], settings_w[ZD_PATH_MAX], db_w[ZD_PATH_MAX];
    DWORD n = GetTempPathW(ZD_PATH_MAX, root);
    CHECK(n > 0 && n < ZD_PATH_MAX);
    if (!n || n >= ZD_PATH_MAX) return 1;
    CHECK(GetTempFileNameW(root, L"ZDC", 0, temp_file) != 0);
    DeleteFileW(temp_file);
    CHECK(CreateDirectoryW(temp_file, NULL) != 0);
    swprintf(settings_w, ZD_PATH_MAX, L"%ls\\settings.json", temp_file);
    swprintf(db_w, ZD_PATH_MAX, L"%ls\\history.db", temp_file);
    char settings_path[ZD_PATH_MAX], db_path[ZD_PATH_MAX], error[ZD_ERROR_MAX];
    CHECK(to_utf8(settings_w, settings_path, sizeof(settings_path)));
    CHECK(to_utf8(db_w, db_path, sizeof(db_path)));

    ZDSettings settings;
    CHECK(zd_settings_load(settings_path, &settings, error, sizeof(error)) == 1);
    CHECK(strcmp(settings.ui_mode, "user") == 0);
    CHECK(strcmp(settings.theme, "light") == 0);
    CHECK(strcmp(settings.language, "uk") == 0);
    settings.request_timeout_connect = 19;
    settings.request_timeout_read = 45;
    settings.max_retry_attempts = 4;
    strcpy(settings.ui_mode, "developer");
    strcpy(settings.theme, "dark");
    strcpy(settings.language, "de");
    snprintf(settings.documents_dir, sizeof(settings.documents_dir), "%s", "C:\\Документи\\Закон");
    CHECK(zd_settings_save(settings_path, &settings, error, sizeof(error)) == 1);
    ZDSettings loaded;
    CHECK(zd_settings_load(settings_path, &loaded, error, sizeof(error)) == 1);
    CHECK(loaded.request_timeout_connect == 19 && loaded.request_timeout_read == 45);
    CHECK(loaded.max_retry_attempts == 4);
    CHECK(strcmp(loaded.ui_mode, "developer") == 0 && strcmp(loaded.theme, "dark") == 0);
    CHECK(strcmp(loaded.language, "de") == 0);
    CHECK(strcmp(loaded.documents_dir, settings.documents_dir) == 0);
    { const char *locales[] = {"uk", "en", "de", "fr", "es", "pt", "zh"};
      for (size_t i = 0; i < sizeof(locales) / sizeof(locales[0]); i++) {
          snprintf(settings.language, sizeof(settings.language), "%s", locales[i]);
          CHECK(zd_settings_save(settings_path, &settings, error, sizeof(error)) == 1);
          CHECK(zd_settings_load(settings_path, &loaded, error, sizeof(error)) == 1);
          CHECK(strcmp(loaded.language, locales[i]) == 0);
      }
    }
    CHECK(write_utf8(settings_w, "{\"theme\":\"dark\"}"));
    CHECK(zd_settings_load(settings_path, &loaded, error, sizeof(error)) == 1);
    CHECK(strcmp(loaded.ui_mode, "user") == 0 && strcmp(loaded.theme, "dark") == 0);
    CHECK(strcmp(loaded.language, "uk") == 0);
    CHECK(write_utf8(settings_w, "{\"theme\":\"light\",\"ui_mode\":\"admin\",\"language\":\"xx\"}"));
    CHECK(zd_settings_load(settings_path, &loaded, error, sizeof(error)) == 1);
    CHECK(strcmp(loaded.ui_mode, "user") == 0);
    CHECK(strcmp(loaded.language, "uk") == 0);

    ZDSqliteApi api;
    CHECK(zd_sqlite_load(&api, error, sizeof(error)) == 1);
    if (api.module) {
        sqlite3 *db = NULL;
        int rc = api.open_v2(db_path, &db, ZD_SQLITE_OPEN_READWRITE | ZD_SQLITE_OPEN_CREATE, NULL);
        CHECK(rc == ZD_SQLITE_OK && db != NULL);
        if (db) {
            const char *python_schema = "CREATE TABLE IF NOT EXISTS history(id INTEGER PRIMARY KEY AUTOINCREMENT,url TEXT NOT NULL,status TEXT NOT NULL,started_at TEXT NOT NULL,finished_at TEXT,document_title TEXT,saved_path TEXT,error_message TEXT);";
            char *sql_error = NULL;
            CHECK(api.exec(db, python_schema, NULL, NULL, &sql_error) == ZD_SQLITE_OK);
            if (sql_error) api.free_mem(sql_error);
            api.close(db);
        }
        int64_t id1 = 0, id2 = 0;
        CHECK(zd_history_start(&api, db_path, "https://zakon.rada.gov.ua/laws/show/1", &id1, error, sizeof(error)) == 1);
        CHECK(id1 > 0);
        CHECK(zd_history_finish(&api, db_path, id1, "Успішно", "Тестовий закон", "C:\\Документи\\Закон.txt", NULL, error, sizeof(error)) == 1);
        CHECK(zd_history_start(&api, db_path, "https://zakon.rada.gov.ua/laws/show/2", &id2, error, sizeof(error)) == 1);
        CHECK(id2 > id1);
        CHECK(zd_history_finish(&api, db_path, id2, "Помилка", NULL, NULL, "Тест помилки", error, sizeof(error)) == 1);
        ZDHistoryList list = {0};
        CHECK(zd_history_load(&api, db_path, &list, error, sizeof(error)) == 1);
        CHECK(list.count == 2);
        if (list.count == 2) {
            CHECK(list.items[0].id == id2);
            CHECK(strcmp(list.items[0].status, "Помилка") == 0);
            CHECK(strcmp(list.items[0].error_message, "Тест помилки") == 0);
            CHECK(list.items[1].id == id1);
            CHECK(strcmp(list.items[1].status, "Успішно") == 0);
            CHECK(strcmp(list.items[1].saved_path, "C:\\Документи\\Закон.txt") == 0);
        }
        zd_history_free(&list);
        int64_t position = -1;
        const char *doc_path = "C:\\Документи\\Закон.txt";
        CHECK(zd_reader_progress_get(&api, db_path, doc_path, &position, error, sizeof(error)) == 1);
        CHECK(position == 0);
        CHECK(zd_reader_progress_put(&api, db_path, doc_path, 2468, error, sizeof(error)) == 1);
        CHECK(zd_reader_progress_get(&api, db_path, doc_path, &position, error, sizeof(error)) == 1);
        CHECK(position == 2468);
        CHECK(zd_reader_progress_put(&api, db_path, doc_path, 4096, error, sizeof(error)) == 1);
        CHECK(zd_reader_progress_get(&api, db_path, doc_path, &position, error, sizeof(error)) == 1);
        CHECK(position == 4096);
        CHECK(zd_reader_bookmark_put(&api, db_path, doc_path, 128, "Стаття 1", error, sizeof(error)) == 1);
        CHECK(zd_reader_bookmark_put(&api, db_path, doc_path, 512, "Розділ II", error, sizeof(error)) == 1);
        CHECK(zd_reader_bookmark_put(&api, db_path, doc_path, 128, "Стаття 1 (оновлено)", error, sizeof(error)) == 1);
        ZDReaderBookmarkList bookmarks = {0};
        CHECK(zd_reader_bookmarks_load(&api, db_path, doc_path, &bookmarks, error, sizeof(error)) == 1);
        CHECK(bookmarks.count == 2);
        if (bookmarks.count == 2) {
            CHECK(bookmarks.items[0].position == 128);
            CHECK(strcmp(bookmarks.items[0].label, "Стаття 1 (оновлено)") == 0);
            CHECK(bookmarks.items[1].position == 512);
        }
        zd_reader_bookmarks_free(&bookmarks);
        CHECK(zd_reader_bookmark_delete(&api, db_path, doc_path, 128, error, sizeof(error)) == 1);
        CHECK(zd_reader_bookmarks_load(&api, db_path, doc_path, &bookmarks, error, sizeof(error)) == 1);
        CHECK(bookmarks.count == 1 && bookmarks.items[0].position == 512);
        zd_reader_bookmarks_free(&bookmarks);
        zd_sqlite_unload(&api);
    }

    DeleteFileW(settings_w);
    DeleteFileW(db_w);
    RemoveDirectoryW(temp_file);
    if (failures) { fprintf(stderr, "%d settings/history integration test(s) failed.\n", failures); return 1; }
    puts("Settings and history integration tests passed.");
    return 0;
}
