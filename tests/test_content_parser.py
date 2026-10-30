"""Тести для content_parser.py."""
from pathlib import Path
import pytest
from bs4 import BeautifulSoup
from zakon_downloader.parsers.content_parser import ContentParser

FIXTURES = Path(__file__).parent / "fixtures"

def _soup(html): return BeautifulSoup(html, "lxml")
def _file(): return BeautifulSoup(
    (FIXTURES/"sample_rada_page.html").read_text(encoding="utf-8"), "lxml")

@pytest.fixture()
def p(): return ContentParser()

def test_removes_script(p):
    assert "alert" not in p.parse(_soup("<html><body><p>T</p><script>alert(1)</script></body></html>"))

def test_removes_nav(p):
    r = p.parse(_soup("<html><body><nav>Меню</nav><p>Текст</p></body></html>"))
    assert "Меню" not in r and "Текст" in r

def test_extracts_text(p):    assert "персональних" in p.parse(_file()).lower()
def test_extracts_heading(p): assert "Загальні положення" in p.parse(_file())
def test_table_pipe(p):       assert "|" in p.parse(_file())
def test_no_footer(p):        assert "© Верховна Рада" not in p.parse(_file())

def test_no_triple_newline(p):
    s = _soup("<html><body><p>A</p><p>&nbsp;</p><p>&nbsp;</p><p>&nbsp;</p><p>B</p></body></html>")
    assert "\n\n\n" not in p.parse(s)
