from __future__ import annotations
from dataclasses import dataclass, field
from datetime import datetime
from enum import Enum


class OperationStatus(str, Enum):
    STARTED   = 'Розпочато'   # [1]
    SUCCESS   = 'Успішно'
    FAILED    = 'Помилка'
    SKIPPED   = 'Пропущено'


@dataclass
class HistoryEntry:
    url:          str
    status:       OperationStatus
    started_at:   datetime
    finished_at:  datetime | None          = None
    document_title: str | None             = None
    saved_path:   str | None               = None
    error_message: str | None              = None
    id:           int | None               = None