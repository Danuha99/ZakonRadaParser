from __future__ import annotations
import html
import re


def clean_text(text: str) -> str:
    if not text:
        return ''
    text = html.unescape(text)
    text = text.replace('\u00a0', ' ').replace('\xa0', ' ')
    text = text.replace('\t', ' ')
    text = re.sub(r' {2,}', ' ', text)
    lines = [line.strip() for line in text.splitlines()]
    cleaned = []
    empty_count = 0
    for line in lines:
        if line == '':
            empty_count += 1
            if empty_count <= 1:  # максимум один порожній рядок поспіль
                cleaned.append(line)
        else:
            empty_count = 0
            cleaned.append(line)
    return '\n'.join(cleaned).strip()


def normalize_whitespace(text: str) -> str:
    return re.sub(r'\s+', ' ', text).strip()