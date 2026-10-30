"""Тести для validators.py."""
from __future__ import annotations
import pytest
from zakon_downloader.utils.validators import ValidationResult, is_valid_rada_url, validate_rada_url

@pytest.mark.parametrize("url", [
    "https://zakon.rada.gov.ua/laws/show/2297-17",
    "https://zakon.rada.gov.ua/laws/show/2297-17#Text",
    "https://zakon.rada.gov.ua/laws/main",
])
def test_valid_urls(url):
    assert validate_rada_url(url).is_valid

@pytest.mark.parametrize("url", ["", "   ", None])
def test_empty_url(url):
    r = validate_rada_url(url)
    assert not r.is_valid and "порожнім" in r.error

def test_http_gives_hint():
    r = validate_rada_url("http://zakon.rada.gov.ua/laws/show/2297-17")
    assert not r.is_valid and "https://" in r.error

def test_no_scheme():
    assert not validate_rada_url("zakon.rada.gov.ua/laws/show/2297-17").is_valid

@pytest.mark.parametrize("url", [
    "https://zakon.gov.ua/laws/show/2297-17",
    "https://google.com/test",
    "https://evil.zakon.rada.gov.ua/laws",
])
def test_wrong_domain(url):
    r = validate_rada_url(url)
    assert not r.is_valid and "zakon.rada.gov.ua" in r.error

def test_root_url():
    r = validate_rada_url("https://zakon.rada.gov.ua/")
    assert not r.is_valid and "головну сторінку" in r.error

def test_too_long_url():
    r = validate_rada_url("https://zakon.rada.gov.ua/laws/" + "a"*2100)
    assert not r.is_valid and "довгий" in r.error

def test_is_valid_true():
    assert is_valid_rada_url("https://zakon.rada.gov.ua/laws/show/2297-17") is True

def test_is_valid_false():
    assert is_valid_rada_url("https://google.com") is False

def test_result_ok():
    r = ValidationResult.ok()
    assert r.is_valid and r.error == ""

def test_result_fail():
    r = ValidationResult.fail("помилка")
    assert not r.is_valid and r.error == "помилка"
