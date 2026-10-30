"""validators.py — Валідація URL для zakon.rada.gov.ua."""
from __future__ import annotations
from dataclasses import dataclass
from urllib.parse import urlparse

@dataclass(frozen=True)
class ValidationResult:
    is_valid: bool
    error: str = ""

    @classmethod
    def ok(cls): return cls(is_valid=True)

    @classmethod
    def fail(cls, reason: str): return cls(is_valid=False, error=reason)

_ALLOWED_DOMAIN = "zakon.rada.gov.ua"
_MAX_URL_LENGTH = 2048

def validate_rada_url(url: str) -> ValidationResult:
    if not url or not isinstance(url, str):
        return ValidationResult.fail("Поле URL не може бути порожнім.")
    url = url.strip()
    if not url:
        return ValidationResult.fail("Поле URL не може бути порожнім.")
    if len(url) > _MAX_URL_LENGTH:
        return ValidationResult.fail(f"URL надто довгий (максимум {_MAX_URL_LENGTH} символів).")
    if url.startswith("http://"):
        return ValidationResult.fail(
            "URL має починатися з https://. "
            "Сайт zakon.rada.gov.ua підтримує лише захищений протокол.")
    if not url.startswith("https://"):
        return ValidationResult.fail("URL має починатися з https://zakon.rada.gov.ua/…")
    try:
        parsed = urlparse(url)
    except Exception:
        return ValidationResult.fail("Некоректний формат URL.")
    if parsed.scheme != "https":
        return ValidationResult.fail(f"Непідтримувана схема '{parsed.scheme}'. Дозволено: https.")
    if parsed.netloc != _ALLOWED_DOMAIN:
        entered = parsed.netloc or "(невідомо)"
        return ValidationResult.fail(
            f"Домен '{entered}' не підтримується. "
            f"Програма працює лише з {_ALLOWED_DOMAIN}.")
    if not parsed.path.rstrip("/"):
        return ValidationResult.fail(
            "URL вказує на головну сторінку сайту, а не на конкретний документ.")
    return ValidationResult.ok()

def is_valid_rada_url(url: str) -> bool:
    return validate_rada_url(url).is_valid
