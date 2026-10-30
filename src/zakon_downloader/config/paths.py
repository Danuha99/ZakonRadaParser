"""paths.py — Визначення та створення всіх робочих папок."""
from __future__ import annotations
import sys
from pathlib import Path


def _resolve_base() -> Path:
    if getattr(sys, "frozen", False):
        return Path(sys.executable).parent
    return Path(__file__).resolve().parents[4]


class AppPaths:
    def __init__(self, base: Path | None = None) -> None:
        self.base        = base or _resolve_base()
        self.data        = self.base / "data"
        self.documents   = self.data / "documents"
        self.logs        = self.data / "logs"
        self.exported    = self.data / "exported_logs"
        self.history_dir = self.data / "history"
        self.temp        = self.data / "temp"
        self.config_dir  = self.base / "config"
        self.settings    = self.config_dir / "settings.json"
        self.history_db  = self.history_dir / "history.db"

    def ensure_all(self) -> None:
        for folder in (
            self.documents, self.logs, self.exported,
            self.history_dir, self.temp, self.config_dir,
        ):
            folder.mkdir(parents=True, exist_ok=True)

    def log_file(self, operation_id: str, timestamp: str) -> Path:
        return self.logs / f"op_{operation_id}_{timestamp}.log"

    def combined_log_file(self, timestamp: str) -> Path:
        return self.exported / f"combined_log_{timestamp}.txt"
