from __future__ import annotations
from dataclasses import dataclass
from datetime import date, datetime


@dataclass
class Document:
    title:                 str
    content:               str
    source_url:            str
    downloaded_at:         datetime
    document_type:         str | None = None
    document_number:       str | None = None
    adoption_date:         date | None = None
    current_revision_date: date | None = None
    status:                str | None = None