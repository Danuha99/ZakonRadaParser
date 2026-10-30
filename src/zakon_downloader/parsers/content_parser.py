from __future__ import annotations
import re
from bs4 import BeautifulSoup, NavigableString
from zakon_downloader.utils.html_cleaner import clean_text, normalize_whitespace

_REMOVE_TAGS = {
    'script', 'style', 'noscript', 'iframe',
    'header', 'footer', 'nav', 'aside',
    'form', 'button', 'input', 'select',
    'svg', 'img', 'figure', 'figcaption',
}

_REMOVE_PATTERNS = re.compile(
    r'menu|nav|header|footer|sidebar|banner|advert|cookie|'
    r'breadcrumb|pagination|toolbar|search|social|share|'
    r'popup|modal|overlay|tooltip|widget',
    re.IGNORECASE,
)

_CONTENT_SELECTORS = [
    '#article', '.law-text', '.document-text',
    '#content', 'article', 'main', '.entry-content',
]


class ContentParser:
    def parse(self, soup: BeautifulSoup) -> str:
        self._remove_junk(soup)
        block = self._find_content_block(soup)
        raw = self._extract_text(block)
        return clean_text(raw)

    def _remove_junk(self, soup):
        for tag in soup.find_all(True):
            if tag.parent is None:
                continue
            raw_class = tag.get('class', []) or []
            classes = ' '.join(raw_class) if isinstance(raw_class, list) else str(raw_class)
            tag_id = tag.get('id', '') or ''
            if (
                tag.name in _REMOVE_TAGS
                or _REMOVE_PATTERNS.search(classes)
                or _REMOVE_PATTERNS.search(tag_id)
            ):
                tag.decompose()

    def _find_content_block(self, soup):
        for sel in _CONTENT_SELECTORS:
            block = soup.select_one(sel)
            if block:
                return block
        return soup.find('body') or soup

    def _extract_text(self, block) -> str:
        parts = []
        self._walk(block, parts)
        return '\n'.join(parts)

    def _walk(self, node, parts):
        if isinstance(node, NavigableString):
            text = str(node).strip()
            if text:
                parts.append(text)
            return
        tag_name = node.name.lower() if node.name else ''
        if tag_name in ('h1', 'h2', 'h3', 'h4', 'h5', 'h6'):
            text = normalize_whitespace(node.get_text(separator=' '))
            if text:
                parts.extend(['', text, ''])
            return
        if tag_name == 'table':
            parts.append(self._table_to_text(node))
            parts.append('')
            return
        if tag_name in ('p', 'li', 'dt', 'dd', 'blockquote'):
            text = normalize_whitespace(node.get_text(separator=' '))
            if text:
                parts.append(text)
            return
        for child in node.children:
            self._walk(child, parts)

    @staticmethod
    def _table_to_text(table) -> str:
        rows = []
        for tr in table.find_all('tr'):
            cells = [
                normalize_whitespace(c.get_text(separator=' '))
                for c in tr.find_all(['th', 'td'])
            ]
            row = ' | '.join(c for c in cells if c)
            if row:
                rows.append(row)
        return '\n'.join(rows)
