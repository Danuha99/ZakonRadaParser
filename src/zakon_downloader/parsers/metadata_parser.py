from __future__ import annotations
import re
from dataclasses import dataclass, field
from datetime import date
from bs4 import BeautifulSoup
from zakon_downloader.utils.html_cleaner import normalize_whitespace

_DATE_RE = re.compile(r'(\d{2})\.(\d{2})\.(\d{4})')
_NUMBER_RE = re.compile(r'\b(\d+[-\u2013]\w+)\b')
_TITLE_SELECTORS = ['h1.entry-title', 'h1#title', 'h1.law-title', 'h1']
_REVISION_KEYWORDS = [
    'чинна редакція', 'поточна редакція',
    'редакція від', 'в редакції', 'зі змінами',
]
_STATUS_KEYWORDS = {
    'чинний':             'чинний',
    'втратив чинність':   'втратив чинність',
    'не набрав чинності': 'не набрав чинності',
    'зупинено':           'зупинено',
}


@dataclass
class DocumentMetadata:
    document_title:  str = ''
    document_number: str = ''
    document_type:   str = ''
    adoption_date:   date | None = None
    revision_date:   date | None = None
    status:          str = ''
    raw_errors:      list[str] = field(default_factory=list)

    @property
    def is_complete(self) -> bool:
        return bool(self.document_title and self.revision_date)


class MetadataParser:
    def parse(self, soup: BeautifulSoup, source_url: str = '') -> DocumentMetadata:
        meta = DocumentMetadata()
        meta.document_title  = self._extract_title(soup)
        meta.document_number = self._extract_number(soup)
        meta.document_type   = self._extract_type(meta.document_title)
        meta.adoption_date   = self._extract_adoption_date(soup)
        full_text = soup.get_text(separator=' ')
        meta.revision_date   = self._extract_revision_date(full_text)
        meta.status          = self._extract_status(full_text)
        if not meta.document_title:
            meta.raw_errors.append('Не вдалося витягнути назву документа.')
        if not meta.revision_date:
            meta.raw_errors.append(
                'Не вдалося визначити дату чинної редакції. '
                'Файл не буде збережено [1].'
            )
        return meta

    def _extract_title(self, soup):
        for sel in _TITLE_SELECTORS:
            tag = soup.select_one(sel)
            if tag and tag.get_text(strip=True):
                return normalize_whitespace(tag.get_text(separator=' '))
        title_tag = soup.find('title')
        if title_tag:
            text = re.sub(r'\s*[-|].*zakon.*$', '',
                          title_tag.get_text(strip=True), flags=re.IGNORECASE)
            if text:
                return normalize_whitespace(text)
        return ''

    def _extract_number(self, soup):
        for text in soup.stripped_strings:
            match = _NUMBER_RE.search(text)
            if match:
                number = match.group(1)
                if len(number) < 20:
                    return number
        return ''

    def _extract_type(self, title):
        types = ['Закон','Кодекс','Постанова','Указ','Декрет',
                 'Наказ','Розпорядження','Конституція']
        lower = title.lower()
        for t in types:
            if t.lower() in lower:
                return t
        return 'Документ'

    def _extract_adoption_date(self, soup):
        tag = soup.select_one('.doc-date')
        if tag:
            return self._parse_date(tag.get_text())
        return None

    def _extract_revision_date(self, full_text: str):
        text_lower = full_text.lower()
        for kw in _REVISION_KEYWORDS:
            idx = text_lower.find(kw)
            if idx == -1:
                continue
            d = self._parse_date(full_text[idx: idx + 80])
            if d:
                return d
        dates = [self._parse_date(m.group()) for m in _DATE_RE.finditer(full_text)]
        dates = [d for d in dates if d]
        return max(dates) if dates else None

    def _extract_status(self, full_text: str):
        text_lower = full_text.lower()
        for kw, label in _STATUS_KEYWORDS.items():
            if kw in text_lower:
                return label
        return 'чинний'

    @staticmethod
    def _parse_date(text: str) -> date | None:
        m = _DATE_RE.search(text)
        if not m:
            return None
        try:
            return date(int(m.group(3)), int(m.group(2)), int(m.group(1)))
        except ValueError:
            return None
