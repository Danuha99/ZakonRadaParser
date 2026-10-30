from __future__ import annotations

from dataclasses import dataclass


@dataclass
class AppSettings:
    request_timeout_connect: int = 10
    request_timeout_read: int = 30
    max_retry_attempts: int = 3
    log_level: str = 'INFO'
    theme: str = 'light'
    documents_dir: str = ''
    ui_mode: str = 'user'
    # Shared with the native C edition; the Python UI intentionally remains Ukrainian.
    language: str = 'uk'
