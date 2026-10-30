import pytest
from datetime import datetime
from zakon_downloader.models.history_entry import HistoryEntry, OperationStatus
from zakon_downloader.repositories.history_repository import HistoryRepository

URL = 'https://zakon.rada.gov.ua/laws/show/2297-vi'

@pytest.fixture()
def repo() -> HistoryRepository:
    return HistoryRepository(':memory:')

def _entry(url=URL) -> HistoryEntry:
    return HistoryEntry(url=url, status=OperationStatus.STARTED,
                        started_at=datetime(2024, 1, 1, 10, 0))


def test_add_returns_id(repo):
    e = repo.add(_entry())
    assert e.id is not None and e.id > 0

def test_count_increments(repo):
    repo.add(_entry())
    repo.add(_entry())
    assert repo.count() == 2

def test_get_by_id(repo):
    e = repo.add(_entry())
    found = repo.get_by_id(e.id)
    assert found.url == URL

def test_get_by_id_missing(repo):
    assert repo.get_by_id(9999) is None

def test_update_status(repo):
    e = repo.add(_entry())
    e.status = OperationStatus.SUCCESS
    e.finished_at = datetime(2024, 1, 1, 10, 5)
    e.document_title = 'Закон про тест'
    repo.update(e)
    found = repo.get_by_id(e.id)
    assert found.status == OperationStatus.SUCCESS
    assert found.document_title == 'Закон про тест'

def test_get_all_order(repo):
    for i in range(3):
        repo.add(_entry())
    all_ = repo.get_all()
    assert len(all_) == 3

def test_get_by_url(repo):
    repo.add(_entry(url=URL))
    repo.add(_entry(url='https://zakon.rada.gov.ua/laws/show/999'))
    assert len(repo.get_by_url(URL)) == 1

def test_clear(repo):
    repo.add(_entry())
    repo.clear()
    assert repo.count() == 0

def test_status_stored_as_label(repo):
    e = repo.add(_entry())
    found = repo.get_by_id(e.id)
    assert found.status == OperationStatus.STARTED

def test_finished_at_none_by_default(repo):
    e = repo.add(_entry())
    assert repo.get_by_id(e.id).finished_at is None

def test_error_message_saved(repo):
    e = repo.add(_entry())
    e.status = OperationStatus.FAILED
    e.error_message = 'timeout'
    repo.update(e)
    assert repo.get_by_id(e.id).error_message == 'timeout'