"""Тести для rada_parser.py."""
from pathlib import Path
from datetime import date
import pytest
from zakon_downloader.parsers.rada_parser import RadaParser

FIXTURES = Path(__file__).parent / "fixtures"

def _html(): return (FIXTURES/"sample_rada_page.html").read_text(encoding="utf-8")

@pytest.fixture()
def p(): return RadaParser()

def test_success(p):          assert p.parse(_html()).success
def test_ready_to_save(p):    assert p.parse(_html()).is_ready_to_save
def test_title(p):            assert "захист" in p.parse(_html()).metadata.document_title.lower()
def test_date(p):             assert p.parse(_html()).metadata.revision_date == date(2024,1,1)
def test_content_len(p):      assert len(p.parse(_html()).content) > 100
def test_empty_html(p):       assert not p.parse("").success
def test_empty_errors(p):
    r = p.parse("")
    assert any("порожній" in e.lower() for e in r.errors)

def test_no_date_not_ready(p):
    assert not p.parse("<html><body><p>текст</p></body></html>").is_ready_to_save
