"""Тести для html_cleaner.py."""
from zakon_downloader.utils.html_cleaner import clean_text, normalize_whitespace

def test_html_entities():
    assert clean_text("&amp; &lt; &gt;") == "& < >"

def test_nbsp():
    assert clean_text("a b") == "a b"

def test_multiple_spaces():
    assert clean_text("a   b") == "a b"

def test_tabs():
    assert clean_text("a\tb") == "a b"

def test_max_two_empty_lines():
    assert "\n\n\n" not in clean_text("a\n\n\n\n\nb")

def test_strip():
    assert clean_text("  text  ") == "text"

def test_empty():
    assert clean_text("") == ""

def test_normalize_whitespace():
    assert normalize_whitespace("  a   b  ") == "a b"

def test_normalize_newlines():
    assert normalize_whitespace("a\nb") == "a b"
