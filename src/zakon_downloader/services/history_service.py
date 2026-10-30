from __future__ import annotations
import logging
from datetime import datetime
from pathlib import Path
from zakon_downloader.models.history_entry import HistoryEntry, OperationStatus
from zakon_downloader.repositories.history_repository import HistoryRepository

log = logging.getLogger('zakon_downloader.history_service')


class HistoryService:
    def __init__(self, repo: HistoryRepository) -> None:
        self._repo = repo

    def start_operation(self, url: str) -> HistoryEntry:
        entry = HistoryEntry(
            url=url,
            status=OperationStatus.STARTED,  # [1]
            started_at=datetime.now(),
        )
        return self._repo.add(entry)

    def mark_success(
        self,
        entry: HistoryEntry,
        document_title: str,
        saved_path: Path,
    ) -> HistoryEntry:
        entry.status         = OperationStatus.SUCCESS
        entry.finished_at    = datetime.now()
        entry.document_title = document_title
        entry.saved_path     = str(saved_path)
        entry.error_message  = None
        self._repo.update(entry)
        log.info('SUCCESS | %s | %s', entry.url, saved_path)
        return entry

    def mark_failed(
        self,
        entry: HistoryEntry,
        error_message: str,
    ) -> HistoryEntry:
        entry.status        = OperationStatus.FAILED
        entry.finished_at   = datetime.now()
        entry.error_message = error_message
        self._repo.update(entry)
        log.warning('FAILED | %s | %s', entry.url, error_message)
        return entry

    def mark_skipped(
        self,
        entry: HistoryEntry,
        reason: str,
    ) -> HistoryEntry:
        entry.status        = OperationStatus.SKIPPED
        entry.finished_at   = datetime.now()
        entry.error_message = reason
        self._repo.update(entry)
        log.info('SKIPPED | %s | %s', entry.url, reason)
        return entry

    def get_all(self, limit: int | None = 200) -> list[HistoryEntry]:
        return self._repo.get_all(limit=limit)

    def get_by_url(self, url: str) -> list[HistoryEntry]:
        return self._repo.get_by_url(url)
