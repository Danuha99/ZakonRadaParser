from __future__ import annotations
import pytest
from pathlib import Path
from unittest.mock import MagicMock
from zakon_downloader.models.history_entry import OperationStatus
from zakon_downloader.repositories.history_repository import HistoryRepository
from zakon_downloader.services.download_service import DownloadService
from zakon_downloader.services.history_service import HistoryService

VALID_URL = 'https://zakon.rada.gov.ua/laws/show/2297-vi'

SAMPLE_HTML = '''<!DOCTYPE html>
<html><head><title>Закон про тест</title></head>
<body><article id="article">
  <h1>Закон України про тест</h1>
  <p>Чинна редакція від 01.01.2024</p>
  <p>Стаття 1. Основні положення цього закону щодо регулювання.</p>
  <p>Стаття 2. Додаткові положення та умови застосування.</p>
</article></body></html>'''

LONG_TITLE_HTML = '''<!DOCTYPE html>
<html><head><title>Закон про тест</title></head>
<body><article id="article">
  <h1>Про затвердження Порядку провадження діяльності
  з управління активами інституційних інвесторів та здійснення
  контролю за дотриманням вимог законодавства у сфері
  спільного інвестування та пенсійного забезпечення в Україні</h1>
  <p>Чинна редакція від 15.06.2023</p>
  <p>Стаття 1. Це довгий документ з дуже довгою назвою для тестування.</p>
</article></body></html>'''

_SENTINEL = object()  # маркер 'не передано'


def make_svc(tmp_path, html=_SENTINEL, side_effect=None):
    repo = HistoryRepository(':memory:')
    http = MagicMock()
    if side_effect:
        http.get.side_effect = side_effect
    else:
        # Якщо html не передано — використовуємо SAMPLE_HTML
        # Якщо передано явно (навіть '') — використовуємо саме його [1]
        http.get.return_value = SAMPLE_HTML if html is _SENTINEL else html
    return DownloadService(http, tmp_path, HistoryService(repo)), repo


# ══ Некоректні URL [1] ═══════════════════════════════════════════════════
class TestInvalidUrls:
    def test_empty_url(self, tmp_path):
        s, _ = make_svc(tmp_path)
        r = s.download('')
        assert not r.success
        assert r.history_entry is None

    def test_no_scheme(self, tmp_path):
        s, _ = make_svc(tmp_path)
        r = s.download('zakon.rada.gov.ua/laws/show/1')
        assert not r.success

    def test_http_not_https(self, tmp_path):
        s, _ = make_svc(tmp_path)
        r = s.download('http://zakon.rada.gov.ua/laws/show/1')
        assert not r.success
        assert 'https' in r.error_message.lower()

    def test_wrong_domain(self, tmp_path):
        s, _ = make_svc(tmp_path)
        r = s.download('https://example.com/laws/show/1')
        assert not r.success
        assert r.history_entry is None

    def test_foreign_domain(self, tmp_path):
        s, _ = make_svc(tmp_path)
        r = s.download('https://google.com')
        assert not r.success

    def test_wrong_path(self, tmp_path):
        s, _ = make_svc(tmp_path)
        r = s.download('https://zakon.rada.gov.ua/home')
        assert not r.success

    def test_url_with_anchor_stripped(self, tmp_path):
        s, _ = make_svc(tmp_path)
        r = s.download(VALID_URL + '#Text')
        assert r.success
        assert '#' not in r.url


# ══ HTTP-помилки [1] ═════════════════════════════════════════════════════
class TestHttpErrors:
    def test_404(self, tmp_path):
        import httpx
        req = httpx.Request('GET', VALID_URL)
        resp = httpx.Response(404, request=req)
        s, _ = make_svc(
            tmp_path,
            side_effect=httpx.HTTPStatusError('404', request=req, response=resp),
        )
        r = s.download(VALID_URL)
        assert not r.success
        assert r.history_entry.status == OperationStatus.FAILED
        assert '404' in r.error_message

    def test_403(self, tmp_path):
        import httpx
        req = httpx.Request('GET', VALID_URL)
        resp = httpx.Response(403, request=req)
        s, _ = make_svc(
            tmp_path,
            side_effect=httpx.HTTPStatusError('403', request=req, response=resp),
        )
        r = s.download(VALID_URL)
        assert not r.success
        assert r.history_entry.status == OperationStatus.FAILED

    def test_429(self, tmp_path):
        import httpx
        req = httpx.Request('GET', VALID_URL)
        resp = httpx.Response(429, request=req)
        s, _ = make_svc(
            tmp_path,
            side_effect=httpx.HTTPStatusError('429', request=req, response=resp),
        )
        r = s.download(VALID_URL)
        assert not r.success
        assert r.history_entry.status == OperationStatus.FAILED

    def test_timeout(self, tmp_path):
        import httpx
        s, _ = make_svc(
            tmp_path,
            side_effect=httpx.TimeoutException('timeout'),
        )
        r = s.download(VALID_URL)
        assert not r.success
        assert r.history_entry.status == OperationStatus.FAILED

    def test_network_error(self, tmp_path):
        s, _ = make_svc(
            tmp_path,
            side_effect=ConnectionError('мережева помилка'),
        )
        r = s.download(VALID_URL)
        assert not r.success
        assert r.history_entry.status == OperationStatus.FAILED
        assert 'мережева' in r.error_message.lower()

    def test_generic_exception(self, tmp_path):
        s, _ = make_svc(
            tmp_path,
            side_effect=RuntimeError('unexpected'),
        )
        r = s.download(VALID_URL)
        assert not r.success
        assert r.history_entry.status == OperationStatus.FAILED


# ══ Порожній / сміттєвий HTML [1] ════════════════════════════════════════
class TestBadHtml:
    def test_empty_html(self, tmp_path):
        # html='' передається явно — не замінюється на SAMPLE_HTML [1]
        s, _ = make_svc(tmp_path, html='')
        r = s.download(VALID_URL)
        assert not r.success

    def test_html_no_date(self, tmp_path):
        s, _ = make_svc(
            tmp_path,
            html='<html><body><p>Просто текст без дати</p></body></html>',
        )
        r = s.download(VALID_URL)
        assert not r.success
        assert r.history_entry.status == OperationStatus.SKIPPED

    def test_js_placeholder(self, tmp_path):
        s, _ = make_svc(
            tmp_path,
            html='<html><body><p>Відбувається форматування тексту!</p></body></html>',
        )
        r = s.download(VALID_URL)
        assert not r.success

    def test_only_whitespace_html(self, tmp_path):
        s, _ = make_svc(tmp_path, html='   \n\t  ')
        r = s.download(VALID_URL)
        assert not r.success


# ══ Успішний сценарій [1] ════════════════════════════════════════════════
class TestSuccess:
    def test_file_created(self, tmp_path):
        s, _ = make_svc(tmp_path)
        r = s.download(VALID_URL)
        assert r.success
        assert r.saved_path.exists()

    def test_file_is_utf8(self, tmp_path):
        s, _ = make_svc(tmp_path)
        r = s.download(VALID_URL)
        raw = r.saved_path.read_bytes()
        assert raw[:3] != b'\xef\xbb\xbf'
        r.saved_path.read_text(encoding='utf-8')

    def test_file_extension_txt(self, tmp_path):
        s, _ = make_svc(tmp_path)
        r = s.download(VALID_URL)
        assert r.saved_path.suffix == '.txt'

    def test_file_contains_metadata(self, tmp_path):
        s, _ = make_svc(tmp_path)
        r = s.download(VALID_URL)
        text = r.saved_path.read_text(encoding='utf-8')
        assert 'Назва' in text
        assert 'Редакція' in text
        assert '=' * 72 in text

    def test_history_saved(self, tmp_path):
        s, repo = make_svc(tmp_path)
        s.download(VALID_URL)
        assert repo.count() == 1

    def test_history_status_success(self, tmp_path):
        s, repo = make_svc(tmp_path)
        s.download(VALID_URL)
        assert repo.get_all()[0].status == OperationStatus.SUCCESS


# ══ Перезапис існуючого файлу [1] ════════════════════════════════════════
class TestOverwrite:
    def test_overwrite_creates_same_path(self, tmp_path):
        s, _ = make_svc(tmp_path)
        r1 = s.download(VALID_URL)
        r2 = s.download(VALID_URL)
        assert r1.saved_path == r2.saved_path

    def test_overwrite_updates_content(self, tmp_path):
        s, _ = make_svc(tmp_path)
        s.download(VALID_URL)
        r2 = s.download(VALID_URL)
        assert r2.saved_path.exists()

    def test_overwrite_history_grows(self, tmp_path):
        s, repo = make_svc(tmp_path)
        s.download(VALID_URL)
        s.download(VALID_URL)
        assert repo.count() == 2


# ══ Довга назва документа [1] ════════════════════════════════════════════
class TestLongTitle:
    def test_long_title_file_created(self, tmp_path):
        s, _ = make_svc(tmp_path, html=LONG_TITLE_HTML)
        r = s.download(VALID_URL)
        assert r.success
        assert r.saved_path.exists()

    def test_long_title_filename_max_length(self, tmp_path):
        s, _ = make_svc(tmp_path, html=LONG_TITLE_HTML)
        r = s.download(VALID_URL)
        assert len(r.saved_path.stem) <= 180

    def test_long_title_no_forbidden_chars(self, tmp_path):
        import re
        s, _ = make_svc(tmp_path, html=LONG_TITLE_HTML)
        r = s.download(VALID_URL)
        assert not re.search(r'[\\/:*?"<>|]', r.saved_path.name)


# ══ Збереження історії після кожної операції [1] ═════════════════════════
class TestHistoryPersistence:
    def test_failed_also_saved(self, tmp_path):
        s, repo = make_svc(
            tmp_path,
            side_effect=ConnectionError('err'),
        )
        s.download(VALID_URL)
        assert repo.count() == 1
        assert repo.get_all()[0].status == OperationStatus.FAILED

    def test_skipped_also_saved(self, tmp_path):
        s, repo = make_svc(
            tmp_path,
            html='<html><body><p>Без дати</p></body></html>',
        )
        s.download(VALID_URL)
        assert repo.count() == 1
        assert repo.get_all()[0].status == OperationStatus.SKIPPED

    def test_multiple_operations_all_saved(self, tmp_path):
        s, repo = make_svc(tmp_path)
        for _ in range(5):
            s.download(VALID_URL)
        assert repo.count() == 5

    def test_invalid_url_not_saved_to_history(self, tmp_path):
        s, repo = make_svc(tmp_path)
        s.download('not-a-url')
        assert repo.count() == 0