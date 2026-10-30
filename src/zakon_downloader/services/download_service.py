from __future__ import annotations
import logging
from datetime import datetime
from pathlib import Path

from zakon_downloader.models.document import Document
from zakon_downloader.models.download_result import DownloadResult
from zakon_downloader.parsers.rada_parser import RadaParser
from zakon_downloader.services.document_service import DocumentService
from zakon_downloader.services.history_service import HistoryService
from zakon_downloader.validators.url_validator import UrlValidator

log = logging.getLogger('zakon_downloader.download_service')

# Маркер динамічного контенту — сторінка ще не завантажена [1]
_JS_PLACEHOLDER = 'відбувається форматування тексту'


class DownloadService:
    def __init__(
        self,
        http_client,
        documents_dir: Path,
        history_service: HistoryService,
    ) -> None:
        self._http      = http_client
        self._parser    = RadaParser()
        self._doc_svc   = DocumentService(documents_dir)
        self._history   = history_service
        self._validator = UrlValidator()

    def set_documents_dir(self, documents_dir: Path) -> None:
        """Change the output folder after the GUI applies new settings."""
        self._doc_svc = DocumentService(documents_dir)

    def download(self, url: str) -> DownloadResult:
        # Відкидаємо #anchor з URL [1]
        clean_url = url.split('#')[0].strip()

        # 1. Валідація URL [1]
        valid, err = self._validator.validate(clean_url)
        if not valid:
            log.warning('Невалідний URL: %s | %s', clean_url, err)
            return DownloadResult(success=False, url=clean_url,
                                  error_message=err, errors=[err])

        # 2. Запис в історію зі статусом «Розпочато» [1]
        entry = self._history.start_operation(clean_url)

        try:
            # 3. HTTP-запит з /print стратегією [1]
            html = self._fetch_full_html(clean_url)

            # 4. Парсинг HTML [1]
            parse_result = self._parser.parse(html, source_url=clean_url)

            if not parse_result.is_ready_to_save:
                reason = '; '.join(parse_result.errors) or 'Не вдалося розібрати документ.'
                self._history.mark_skipped(entry, reason)
                return DownloadResult(success=False, url=clean_url,
                                      history_entry=entry,
                                      error_message=reason,
                                      errors=parse_result.errors)

            # 5. Формування Document [1]
            meta = parse_result.metadata
            doc = Document(
                title=meta.document_title,
                content=parse_result.content,
                source_url=clean_url,
                downloaded_at=datetime.now(),
                document_type=meta.document_type,
                document_number=meta.document_number,
                adoption_date=meta.adoption_date,
                current_revision_date=meta.revision_date,
                status=meta.status,
            )

            # 6. Збереження файлу [1]
            saved_path = self._doc_svc.save(doc)

            # 7. Оновлення історії [1]
            self._history.mark_success(entry, doc.title, saved_path)

            log.info('OK | %s | %s', clean_url, saved_path)
            return DownloadResult(
                success=True,
                url=clean_url,
                history_entry=entry,
                saved_path=saved_path,
                document_title=doc.title,
            )

        except Exception as exc:
            msg = str(exc)
            log.exception('FAILED | %s | %s', clean_url, msg)
            self._history.mark_failed(entry, msg)
            return DownloadResult(success=False, url=clean_url,
                                  history_entry=entry,
                                  error_message=msg,
                                  errors=[msg])

    def _fetch_full_html(self, url: str) -> str:
        # Спочатку пробуємо /print — повертає повний текст без JS [1]
        print_url = url.rstrip('/') + '/print'
        try:
            html = self._http.get(print_url)
            if _JS_PLACEHOLDER not in html.lower():
                log.info('Використано /print версію: %s', print_url)
                return html
            log.warning('/print містить JS-заглушку, fallback на оригінальний URL')
        except Exception as e:
            if (
                getattr(e, 'status_code', None) in {401, 403, 429}
                or 'captcha' in str(e).lower()
            ):
                raise
            log.warning('/print недоступний (%s), fallback на оригінальний URL', e)

        # Fallback — оригінальний URL
        html = self._http.get(url)
        if _JS_PLACEHOLDER in html.lower():
            log.warning('Сторінка містить JS-заглушку — текст може бути неповним')
        return html
