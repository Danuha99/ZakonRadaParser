import pytest
from pathlib import Path
from zakon_downloader.services.log_export_service import LogExportService


@pytest.fixture()
def svc(tmp_path) -> LogExportService:
    logs_dir   = tmp_path / 'logs'
    export_dir = tmp_path / 'exported_logs'
    logs_dir.mkdir()
    return LogExportService(logs_dir, export_dir)


def _make_log(svc: LogExportService, name: str, content: str) -> Path:
    p = svc._logs_dir / name
    p.write_text(content, encoding='utf-8')
    return p


def test_list_empty(svc):
    assert svc.list_log_files() == []

def test_list_finds_logs(svc):
    _make_log(svc, 'a.log', 'рядок 1')
    _make_log(svc, 'b.log', 'рядок 2')
    assert len(svc.list_log_files()) == 2

def test_list_only_dot_log(svc):
    _make_log(svc, 'a.log', 'ok')
    (svc._logs_dir / 'notes.txt').write_text('ignore')
    assert len(svc.list_log_files()) == 1

def test_read_log(svc):
    _make_log(svc, 'a.log', 'тест вміст')
    p = svc.list_log_files()[0]
    assert 'тест вміст' in svc.read_log(p)

def test_read_missing_returns_empty(svc, tmp_path):
    assert svc.read_log(tmp_path / 'missing.log') == ''

def test_export_combined_creates_file(svc):
    _make_log(svc, 'a.log', 'вміст А')
    out = svc.export_combined()
    assert out.exists()

def test_export_combined_contains_all(svc):
    _make_log(svc, 'a.log', 'вміст А')
    _make_log(svc, 'b.log', 'вміст Б')
    text = svc.export_combined().read_text(encoding='utf-8')
    assert 'вміст А' in text
    assert 'вміст Б' in text

def test_export_contains_header(svc):
    _make_log(svc, 'a.log', 'дані')
    text = svc.export_combined().read_text(encoding='utf-8')
    assert 'Зведений журнал' in text
    assert '=' * 72 in text

def test_export_filename_format(svc):
    _make_log(svc, 'a.log', 'дані')
    out = svc.export_combined()
    assert out.name.startswith('combined_log_')
    assert out.suffix == '.txt'

def test_export_no_logs_raises(svc):
    with pytest.raises(FileNotFoundError):
        svc.export_combined()

def test_export_utf8_no_bom(svc):
    _make_log(svc, 'a.log', 'кирилиця')
    out = svc.export_combined()
    assert out.read_bytes()[:3] != b'\xef\xbb\xbf'

def test_export_saved_in_export_dir(svc):
    _make_log(svc, 'a.log', 'дані')
    out = svc.export_combined()
    assert out.parent == svc._export_dir