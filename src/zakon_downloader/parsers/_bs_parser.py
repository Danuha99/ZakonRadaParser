"""Вибір доступного HTML-парсера."""
from __future__ import annotations


def best_parser() -> str:
    try:
        import lxml  # noqa: F401
        return 'lxml'
    except ImportError:
        return 'html.parser'