from __future__ import annotations
import re

# Таблиця транслітерації КМУ 2010 (спрощена для імен файлів) [1]
_TABLE: dict[str, str] = {
    'а': 'a',  'б': 'b',  'в': 'v',  'г': 'h',  'ґ': 'g',
    'д': 'd',  'е': 'e',  'є': 'ie', 'ж': 'zh', 'з': 'z',
    'и': 'y',  'і': 'i',  'ї': 'i',  'й': 'i',  'к': 'k',
    'л': 'l',  'м': 'm',  'н': 'n',  'о': 'o',  'п': 'p',
    'р': 'r',  'с': 's',  'т': 't',  'у': 'u',  'ф': 'f',
    'х': 'kh', 'ц': 'ts', 'ч': 'ch', 'ш': 'sh', 'щ': 'shch',
    'ь': '',   'ю': 'iu', 'я': 'ia', "'":  '',  '\u2019': '',
}


def transliterate(text: str) -> str:
    result = []
    for ch in text.lower():
        if ch in _TABLE:
            result.append(_TABLE[ch])
        else:
            result.append(ch)
    return ''.join(result)