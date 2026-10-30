from __future__ import annotations

import json
import os
import tempfile
from dataclasses import fields
from pathlib import Path

from zakon_downloader.models.app_settings import AppSettings


class SettingsManager:
    def __init__(self, settings_path: Path) -> None:
        self._path = settings_path

    def load(self) -> AppSettings:
        if not self._path.exists():
            return AppSettings()
        try:
            data = json.loads(self._path.read_text(encoding='utf-8'))
            if not isinstance(data, dict):
                return AppSettings()
            allowed = {item.name for item in fields(AppSettings)}
            settings = AppSettings(**{key: value for key, value in data.items()
                                      if key in allowed})
            if settings.theme not in {'light', 'dark'}:
                settings.theme = 'light'
            if settings.ui_mode not in {'user', 'developer'}:
                settings.ui_mode = 'user'
            if settings.language not in {'uk', 'en', 'de', 'fr', 'es', 'pt', 'zh'}:
                settings.language = 'uk'
            if settings.log_level not in {'DEBUG', 'INFO', 'WARNING', 'ERROR'}:
                settings.log_level = 'INFO'
            settings.request_timeout_connect = _bounded_int(
                settings.request_timeout_connect, 10, 1, 300
            )
            settings.request_timeout_read = _bounded_int(
                settings.request_timeout_read, 30, 1, 600
            )
            settings.max_retry_attempts = _bounded_int(
                settings.max_retry_attempts, 3, 1, 10
            )
            if not isinstance(settings.documents_dir, str):
                settings.documents_dir = ''
            return settings
        except (OSError, ValueError, TypeError, json.JSONDecodeError):
            return AppSettings()

    def save(self, settings: AppSettings) -> None:
        self._path.parent.mkdir(parents=True, exist_ok=True)
        payload = json.dumps(vars(settings), ensure_ascii=False, indent=2)
        fd, temp_name = tempfile.mkstemp(dir=self._path.parent, suffix='.tmp')
        try:
            with os.fdopen(fd, 'w', encoding='utf-8') as stream:
                stream.write(payload)
            os.replace(temp_name, self._path)
        except Exception:
            try:
                os.unlink(temp_name)
            except OSError:
                pass
            raise


def _bounded_int(value: object, default: int, minimum: int, maximum: int) -> int:
    try:
        return max(minimum, min(maximum, int(value)))
    except (TypeError, ValueError):
        return default
