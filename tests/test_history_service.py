import pytest
from pathlib import Path
from zakon_downloader.models.history_entry import OperationStatus
from zakon_downloader.repositories.history_repository import HistoryRepository
from zakon_downloader.services.history_service import HistoryService

URL = 'https://zakon.rada.gov.ua/laws/show/2297-vi'

@pytest.fixture()
def svc() -> HistoryService:
    return HistoryService(HistoryRepository(':memory:'))


def test_start_creates_entry(svc):
    e = svc.start_operation(URL)
    assert e.id is not None
    assert e.status == OperationStatus.STARTED

def test_mark_success(svc):
    e = svc.start_operation(URL)
    saved = Path('/docs/test.txt')
    svc.mark_success(e, 'Закон про тест', saved)
    assert e.status == OperationStatus.SUCCESS
    assert e.document_title == 'Закон про тест'
    assert Path(e.saved_path) == saved  # крос-платформне порівняння
    assert e.finished_at is not None

def test_mark_failed(svc):
    e = svc.start_operation(URL)
    svc.mark_failed(e, 'timeout')
    assert e.status == OperationStatus.FAILED
    assert e.error_message == 'timeout'
    assert e.finished_at is not None

def test_mark_skipped(svc):
    e = svc.start_operation(URL)
    svc.mark_skipped(e, 'дата редакції невідома')
    assert e.status == OperationStatus.SKIPPED
    assert 'редакції' in e.error_message

def test_get_all(svc):
    svc.start_operation(URL)
    svc.start_operation(URL)
    assert len(svc.get_all()) == 2

def test_get_by_url(svc):
    svc.start_operation(URL)
    svc.start_operation('https://zakon.rada.gov.ua/laws/show/999')
    assert len(svc.get_by_url(URL)) == 1

def test_success_clears_error(svc):
    e = svc.start_operation(URL)
    svc.mark_failed(e, 'помилка')
    svc.mark_success(e, 'Заголовок', Path('/f.txt'))
    assert e.error_message is None