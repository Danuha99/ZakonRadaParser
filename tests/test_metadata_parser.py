"""Тести для metadata_parser.py."""
from datetime import date
from pathlib import Path
import pytest
from bs4 import BeautifulSoup
from zakon_downloader.parsers.metadata_parser import MetadataParser

FIXTURES = Path(__file__).parent / "fixtures"

def _soup(html): return BeautifulSoup(html, "lxml")
def _file(): return BeautifulSoup(
    (FIXTURES/"sample_rada_page.html").read_text(encoding="utf-8"), "lxml")

@pytest.fixture()
def p(): return MetadataParser()

def test_title(p):       assert "захист" in p.parse(_file()).document_title.lower()
def test_date(p):        assert p.parse(_file()).revision_date == date(2024,1,1)
def test_type(p):        assert p.parse(_file()).document_type == "Закон"
def test_complete(p):    assert p.parse(_file()).is_complete
def test_status(p):      assert p.parse(_file()).status == "чинний"

def test_no_date_incomplete(p):
    assert not p.parse(_soup("<html><body><h1>Закон</h1></body></html>")).is_complete

def test_status_vtratyl(p):
    s = _soup("<html><body><h1>Закон</h1>"
              "<p>Чинна редакція від 01.01.2020. Документ втратив чинність.</p></body></html>")
    assert p.parse(s).status == "втратив чинність"

def test_parse_date_ok(p):  assert p._parse_date("від 15.03.2023") == date(2023,3,15)
def test_parse_date_none(p): assert p._parse_date("немає") is None
