from __future__ import annotations
import pytest
from unittest.mock import MagicMock
from zakon_downloader.models.history_entry import OperationStatus
from zakon_downloader.repositories.history_repository import HistoryRepository
from zakon_downloader.services.download_service import DownloadService
from zakon_downloader.services.history_service import HistoryService
from zakon_downloader.services.http_client import HttpClientError

VALID_URL = 'https://zakon.rada.gov.ua/laws/show/2297-vi'

SAMPLE_HTML = '''<!DOCTYPE html>
<html><head><title>Закон про тест</title></head>
<body>
<article id="article">
  <h1>Закон України про тест</h1>
  <p>Чинна редакція від 01.01.2024</p>
  <p>Стаття 1. Основні положення цього закону.</p>
  <p>Стаття 2. Додаткові положення.</p>
</article>
</body></html>'''


@pytest.fixture()
def http_mock():
    m = MagicMock()
    m.get.return_value = SAMPLE_HTML
    return m


@pytest.fixture()
def svc(tmp_path, http_mock) -> DownloadService:
    repo = HistoryRepository(':memory:')
    history = HistoryService(repo)
    return DownloadService(
        http_client=http_mock,
        documents_dir=tmp_path,
        history_service=history,
    )


# ── Валідація URL [1] ────────────────────────────────────────────────────
def test_invalid_url_no_http(svc):
    r = svc.download('http://zakon.rada.gov.ua/laws/show/1')
    assert not r.success
    assert r.error_message

def test_invalid_url_wrong_domain(svc):
    r = svc.download('https://example.com/laws/show/1')
    assert not r.success

def test_empty_url(svc):
    r = svc.download('')
    assert not r.success

def test_invalid_url_no_history_entry(svc):
    r = svc.download('not-a-url')
    assert r.history_entry is None  # до старту операції не доходить

# ── Успішний сценарій [1] ────────────────────────────────────────────────
def test_success_returns_true(svc):
    r = svc.download(VALID_URL)
    assert r.success

def test_success_file_created(svc):
    r = svc.download(VALID_URL)
    assert r.has_file
    assert r.saved_path.suffix == '.txt'

def test_success_title_set(svc):
    r = svc.download(VALID_URL)
    assert r.document_title
    assert len(r.document_title) > 3

def test_success_history_status(svc):
    r = svc.download(VALID_URL)
    assert r.history_entry.status == OperationStatus.SUCCESS

def test_success_history_saved_path(svc):
    r = svc.download(VALID_URL)
    assert r.history_entry.saved_path is not None

# ── HTTP-помилка ─────────────────────────────────────────────────────────
def test_http_error_marks_failed(tmp_path):
    http = MagicMock()
    http.get.side_effect = ConnectionError('timeout')
    repo = HistoryRepository(':memory:')
    service = DownloadService(http, tmp_path, HistoryService(repo))
    r = service.download(VALID_URL)
    assert not r.success
    assert r.history_entry.status == OperationStatus.FAILED
    assert 'timeout' in r.error_message


def test_rate_limit_does_not_fallback_to_original_url(tmp_path):
    http = MagicMock()
    http.get.side_effect = HttpClientError('HTTP 429', status_code=429)
    repo = HistoryRepository(':memory:')
    service = DownloadService(http, tmp_path, HistoryService(repo))

    result = service.download(VALID_URL)

    assert not result.success
    assert http.get.call_count == 1

# ── Пропуск через відсутність дати редакції [1] ──────────────────────────
def test_no_revision_date_skipped(tmp_path):
    http = MagicMock()
    http.get.return_value = '<html><body><p>Просто текст</p></body></html>'
    repo = HistoryRepository(':memory:')
    service = DownloadService(http, tmp_path, HistoryService(repo))
    r = service.download(VALID_URL)
    assert not r.success
    assert r.history_entry.status == OperationStatus.SKIPPED

# ── Файл перезаписується [1] ─────────────────────────────────────────────
def test_overwrite_existing_file(svc):
    r1 = svc.download(VALID_URL)
    r2 = svc.download(VALID_URL)
    assert r1.saved_path == r2.saved_path
    assert r2.has_file
