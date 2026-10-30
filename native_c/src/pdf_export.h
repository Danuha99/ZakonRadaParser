#ifndef ZAKON_PDF_EXPORT_H
#define ZAKON_PDF_EXPORT_H

#include <stddef.h>
#include <wchar.h>

/* Render clean UTF-16 document text as a paginated, book-style A4 PDF. */
int zd_pdf_export(const wchar_t *output_path, const wchar_t *title,
                  const wchar_t *metadata, const wchar_t *body,
                  char *error, size_t error_cap);

#endif
