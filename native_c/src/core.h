#ifndef ZAKON_CORE_H
#define ZAKON_CORE_H

#include <stddef.h>

#define ZD_URL_MAX 2048
#define ZD_TITLE_MAX 1024
#define ZD_FIELD_MAX 128
#define ZD_ERROR_MAX 512

typedef struct {
    char title[ZD_TITLE_MAX];
    char document_type[ZD_FIELD_MAX];
    char number[ZD_FIELD_MAX];
    char adoption_date[16];
    char revision_date[16];
    char status[ZD_FIELD_MAX];
    char *content;
} ZDDocument;

/* Returns 1 for an accepted direct document URL, otherwise 0 and an explanation. */
int zd_validate_url(const char *url, char *error, size_t error_cap);

/* Parses UTF-8 HTML and extracts document text and metadata; caller frees doc->content. */
int zd_parse_html(const char *html, ZDDocument *doc, char *error, size_t error_cap);
void zd_free_document(ZDDocument *doc);

/* Windows-safe, lowercase transliterated output stem; includes the revision date and .txt. */
int zd_build_filename(const char *title, const char *ddmmyyyy, char *out, size_t out_cap);

#endif
