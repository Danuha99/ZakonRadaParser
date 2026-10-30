from __future__ import annotations
import logging
import os
import tempfile
from datetime import date
from pathlib import Path
from zakon_downloader.models.document import Document
from zakon_downloader.services.filename_service import build_filename

log = logging.getLogger('zakon_downloader.document_service')

_HEADER_TEMPLATE = '''
Назва:      {title}
Тип:        {doc_type}
Номер:      {number}
Прийнятий:  {adoption}
Редакція:   {revision}
Статус:     {status}
Джерело:    {url}
Завантажено:{downloaded_at}
{'=' * 72}
'''


class DocumentService:
    def __init__(self, documents_dir: Path) -> None:
        self._dir = documents_dir
        self._dir.mkdir(parents=True, exist_ok=True)

    def save(self, doc: Document) -> Path:
        if not doc.content or not doc.content.strip():
            raise ValueError('Текст документа порожній — збереження скасовано [1].')
        if doc.current_revision_date is None:
            raise ValueError('Дата редакції невідома — збереження скасовано [1].')
        filename = build_filename(doc.title, doc.current_revision_date)
        out_path = self._dir / filename
        text = self._build_text(doc)
        self._safe_write(out_path, text)
        log.info('Збережено: %s (%d байт)', out_path, len(text.encode('utf-8')))
        return out_path

    def _build_text(self, doc: Document) -> str:
        header = (
            f'Назва:       {doc.title}\n'
            f'Тип:         {doc.document_type or "-"}\n'
            f'Номер:       {doc.document_number or "-"}\n'
            f'Прийнятий:   {_fmt_date(doc.adoption_date)}\n'
            f'Редакція:    {_fmt_date(doc.current_revision_date)}\n'
            f'Статус:      {doc.status or "-"}\n'
            f'Джерело:     {doc.source_url}\n'
            f'Завантажено: {doc.downloaded_at.strftime("%d.%m.%Y %H:%M")}\n'
            + '=' * 72
        )
        return header + '\n\n' + doc.content.strip() + '\n'

    @staticmethod
    def _safe_write(path: Path, text: str) -> None:
        dir_ = path.parent
        fd, tmp = tempfile.mkstemp(dir=dir_, suffix='.tmp')
        try:
            with os.fdopen(fd, 'w', encoding='utf-8') as fh:
                fh.write(text)
            os.replace(tmp, path)  # атомарний перезапис [1]
        except Exception:
            try:
                os.unlink(tmp)
            except OSError:
                pass
            raise


def _fmt_date(d: date | None) -> str:
    return d.strftime('%d.%m.%Y') if d else '-'