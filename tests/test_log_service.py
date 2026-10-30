import pytest
from pathlib import Path
from zakon_downloader.services.log_service import LogService


@pytest.fixture()
def svc(tmp_path) -> LogService:
    return LogService(tmp_path)


def _make_log(tmp_path: Path, name: str, content: str) -> Path:
    p = tmp_path / name
    p.write_text(content, encoding='utf-8')
    return p


def test_list_empty(svc, tmp_path):
    assert svc.list_log_files() == []

def test_list_finds_logs(svc, tmp_path):
    _make_log(tmp_path, 'app.log', 'рядок 1')
    _make_log(tmp_path, 'error.log', 'рядок 2')
    assert len(svc.list_log_files()) == 2

def test_list_only_log_files(svc, tmp_path):
    _make_log(tmp_path, 'app.log', 'ok')
    (tmp_path / 'notes.txt').write_text('ignore')
    assert len(svc.list_log_files()) == 1

def test_read_log(svc, tmp_path):
    _make_log(tmp_path, 'app.log', 'тест вміст')
    p = svc.list_log_files()[0]
    assert 'тест вміст' in svc.read_log(p)

def test_read_missing_returns_empty(svc, tmp_path):
    assert svc.read_log(tmp_path / 'missing.log') == ''

def test_export_combined(svc, tmp_path):
    _make_log(tmp_path, 'a.log', 'вміст А')
    _make_log(tmp_path, 'b.log', 'вміст Б')
    out = tmp_path / 'combined.txt'
    result = svc.export_combined(out)
    assert result.exists()
    text = result.read_text(encoding='utf-8')
    assert 'вміст А' in text
    assert 'вміст Б' in text

def test_export_contains_header(svc, tmp_path):
    _make_log(tmp_path, 'a.log', 'дані')
    out = tmp_path / 'combined.txt'
    svc.export_combined(out)
    text = out.read_text(encoding='utf-8')
    assert 'Зведений журнал' in text
    assert '=' * 72 in text

def test_export_no_logs_raises(svc, tmp_path):
    with pytest.raises(FileNotFoundError):
        svc.export_combined(tmp_path / 'out.txt')

def test_export_utf8_no_bom(svc, tmp_path):
    _make_log(tmp_path, 'a.log', 'кирилиця')
    out = tmp_path / 'combined.txt'
    svc.export_combined(out)
    assert out.read_bytes()[:3] != b'\xef\xbb\xbf'