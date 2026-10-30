from __future__ import annotations
from dataclasses import dataclass, field
from pathlib import Path
from zakon_downloader.models.history_entry import HistoryEntry


@dataclass
class DownloadResult:
    success:      bool
    url:          str
    history_entry: HistoryEntry | None = None
    saved_path:   Path | None          = None
    document_title: str | None         = None
    error_message:  str | None         = None
    errors:       list[str]            = field(default_factory=list)

    @property
    def has_file(self) -> bool:
        return self.saved_path is not None and self.saved_path.exists()