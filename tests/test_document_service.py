import pytest
from datetime import date, datetime
from pathlib import Path
from zakon_downloader.models.document import Document
from zakon_downloader.services.document_service import DocumentService

def _doc(**kw) -> Document:
    defaults = dict(
        title='Закон про тест',
        content='Стаття 1. Тестовий вміст документа.',
        source_url='https://zakon.rada.gov.ua/laws/show/1',
        downloaded_at=datetime(2024, 1, 1, 12, 0),
        document_type='Закон',
        document_number='1-I',
        adoption_date=date(2020, 6, 1),
        current_revision_date=date(2024, 1, 1),
        status='чинний',
    )
    defaults.update(kw)
    return Document(**defaults)


@pytest.fixture()
def svc(tmp_path) -> DocumentService:
    return DocumentService(tmp_path)


def test_save_returns_path(svc):
    path = svc.save(_doc())
    assert path.exists()

def test_file_encoding_utf8(svc):
    path = svc.save(_doc(title='Тест кирилиці'))
    raw = path.read_bytes()
    assert raw[:3] != b'\xef\xbb\xbf'  # без BOM [1]
    path.read_text(encoding='utf-8')      # читається без помилок

def test_filename_contains_date(svc):
    path = svc.save(_doc())
    assert '01.01.2024' in path.name

def test_file_contains_title(svc):
    path = svc.save(_doc())
    assert 'Закон про тест' in path.read_text(encoding='utf-8')

def test_file_contains_content(svc):
    path = svc.save(_doc())
    assert 'Стаття 1' in path.read_text(encoding='utf-8')

def test_file_contains_url(svc):
    path = svc.save(_doc())
    assert 'zakon.rada.gov.ua' in path.read_text(encoding='utf-8')

def test_empty_content_raises(svc):
    with pytest.raises(ValueError, match='порожній'):
        svc.save(_doc(content=''))

def test_no_revision_date_raises(svc):
    with pytest.raises(ValueError, match='редакції'):
        svc.save(_doc(current_revision_date=None))

def test_overwrite_same_file(svc):
    svc.save(_doc(content='Перший запис'))
    path = svc.save(_doc(content='Другий запис'))
    assert 'Другий запис' in path.read_text(encoding='utf-8')

def test_header_separator(svc):
    path = svc.save(_doc())
    assert '=' * 72 in path.read_text(encoding='utf-8')