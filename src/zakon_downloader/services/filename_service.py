from __future__ import annotations
import re
from datetime import date
from zakon_downloader.utils.transliteration import transliterate

# Заборонені символи Windows у іменах файлів [1]
_FORBIDDEN = re.compile(r'[\\/:*?"<>|]')
_MAX_STEM = 180  # символів без розширення [1]


def build_filename(title: str, revision_date: date) -> str:
    stem = _make_stem(title, revision_date)
    return stem + '.txt'


def _make_stem(title: str, revision_date: date) -> str:
    # 1. Транслітерація
    translit = transliterate(title)
    # 2. Лише літери, цифри, пробіли
    clean = re.sub(r'[^a-z0-9 ]', ' ', translit)
    # 3. Пробіли → підкреслення, стискаємо множинні
    slug = re.sub(r' +', '_', clean.strip())
    slug = re.sub(r'_+', '_', slug).strip('_')
    # 4. Дата
    date_str = revision_date.strftime('%d.%m.%Y')
    # 5. Обрізка до MAX_STEM з урахуванням дати [1]
    suffix = '_' + date_str
    max_slug = _MAX_STEM - len(suffix)
    if len(slug) > max_slug:
        slug = slug[:max_slug].rstrip('_')
    return slug + suffix