from __future__ import annotations
import logging
from dataclasses import dataclass, field
from bs4 import BeautifulSoup
from zakon_downloader.parsers._bs_parser import best_parser
from zakon_downloader.parsers.content_parser import ContentParser
from zakon_downloader.parsers.metadata_parser import DocumentMetadata, MetadataParser

log = logging.getLogger('zakon_downloader.rada_parser')
PARSER = best_parser()


@dataclass
class ParseResult:
    success: bool
    metadata: DocumentMetadata | None = None
    content: str = ''
    errors: list[str] = field(default_factory=list)

    @property
    def has_content(self) -> bool:
        return bool(self.content and self.content.strip())

    @property
    def is_ready_to_save(self) -> bool:
        return (self.success and self.metadata is not None
                and self.metadata.is_complete and self.has_content)


class RadaParser:
    def __init__(self) -> None:
        self._meta    = MetadataParser()
        self._content = ContentParser()

    def parse(self, html: str, source_url: str = '') -> ParseResult:
        if not html or not html.strip():
            return ParseResult(success=False, errors=['Отримано порожній HTML.'])
        try:
            soup = BeautifulSoup(html, PARSER)
        except Exception as exc:
            return ParseResult(success=False, errors=[f'Помилка розбору HTML: {exc}'])
        metadata = self._meta.parse(soup, source_url=source_url)
        content  = self._content.parse(soup)
        errors   = list(metadata.raw_errors)
        if not content.strip():
            errors.append('Не вдалося витягнути текст документа.')
        success = bool(
            metadata.document_title
            and metadata.revision_date
            and content.strip()
        )
        return ParseResult(success=success, metadata=metadata,
                           content=content, errors=errors)