from __future__ import annotations
import sqlite3
import logging
from datetime import datetime
from pathlib import Path
from zakon_downloader.models.history_entry import HistoryEntry, OperationStatus

log = logging.getLogger('zakon_downloader.history_repository')

_DDL = '''
CREATE TABLE IF NOT EXISTS history (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    url             TEXT    NOT NULL,
    status          TEXT    NOT NULL,
    started_at      TEXT    NOT NULL,
    finished_at     TEXT,
    document_title  TEXT,
    saved_path      TEXT,
    error_message   TEXT
);
'''

_DT_FMT = '%Y-%m-%dT%H:%M:%S'


def _to_dt(s: str | None) -> datetime | None:
    return datetime.strptime(s, _DT_FMT) if s else None


def _from_dt(dt: datetime | None) -> str | None:
    return dt.strftime(_DT_FMT) if dt else None


def _row_to_entry(row: sqlite3.Row) -> HistoryEntry:
    return HistoryEntry(
        id=row['id'],
        url=row['url'],
        status=OperationStatus(row['status']),
        started_at=_to_dt(row['started_at']),
        finished_at=_to_dt(row['finished_at']),
        document_title=row['document_title'],
        saved_path=row['saved_path'],
        error_message=row['error_message'],
    )


class HistoryRepository:
    def __init__(self, db_path: Path | str = ':memory:') -> None:
        self._conn = sqlite3.connect(str(db_path), check_same_thread=False)
        self._conn.row_factory = sqlite3.Row
        self._conn.executescript(_DDL)
        self._conn.commit()

    # ── CREATE ────────────────────────────────────────────────────────────
    def add(self, entry: HistoryEntry) -> HistoryEntry:
        cur = self._conn.execute(
            'INSERT INTO history (url, status, started_at, finished_at,'
            ' document_title, saved_path, error_message)'
            ' VALUES (?,?,?,?,?,?,?)',
            (
                entry.url,
                entry.status.value,
                _from_dt(entry.started_at),
                _from_dt(entry.finished_at),
                entry.document_title,
                entry.saved_path,
                entry.error_message,
            ),
        )
        self._conn.commit()
        entry.id = cur.lastrowid
        return entry

    # ── UPDATE ────────────────────────────────────────────────────────────
    def update(self, entry: HistoryEntry) -> None:
        self._conn.execute(
            'UPDATE history SET status=?, finished_at=?, document_title=?,'
            ' saved_path=?, error_message=? WHERE id=?',
            (
                entry.status.value,
                _from_dt(entry.finished_at),
                entry.document_title,
                entry.saved_path,
                entry.error_message,
                entry.id,
            ),
        )
        self._conn.commit()

    # ── READ ──────────────────────────────────────────────────────────────
    def get_by_id(self, entry_id: int) -> HistoryEntry | None:
        row = self._conn.execute(
            'SELECT * FROM history WHERE id=?', (entry_id,)
        ).fetchone()
        return _row_to_entry(row) if row else None

    def get_all(self, limit: int | None = 200) -> list[HistoryEntry]:
        if limit is None:
            rows = self._conn.execute(
                'SELECT * FROM history ORDER BY started_at DESC'
            ).fetchall()
        else:
            rows = self._conn.execute(
                'SELECT * FROM history ORDER BY started_at DESC LIMIT ?', (limit,)
            ).fetchall()
        return [_row_to_entry(r) for r in rows]

    def get_by_url(self, url: str) -> list[HistoryEntry]:
        rows = self._conn.execute(
            'SELECT * FROM history WHERE url=? ORDER BY started_at DESC', (url,)
        ).fetchall()
        return [_row_to_entry(r) for r in rows]

    def count(self) -> int:
        return self._conn.execute('SELECT COUNT(*) FROM history').fetchone()[0]

    # ── DELETE ────────────────────────────────────────────────────────────
    def clear(self) -> None:
        self._conn.execute('DELETE FROM history')
        self._conn.commit()

    def close(self) -> None:
        self._conn.close()
