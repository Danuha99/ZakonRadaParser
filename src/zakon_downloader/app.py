from __future__ import annotations

import logging
import sys
from pathlib import Path

from PySide6.QtWidgets import QApplication

from zakon_downloader.config.paths import AppPaths
from zakon_downloader.config.settings import SettingsManager
from zakon_downloader.gui.main_window import MainWindow
from zakon_downloader.models.app_settings import AppSettings
from zakon_downloader.repositories.history_repository import HistoryRepository
from zakon_downloader.services.download_service import DownloadService
from zakon_downloader.services.history_service import HistoryService
from zakon_downloader.services.http_client import HttpClient, HttpClientError
from zakon_downloader.services.log_export_service import LogExportService
from zakon_downloader.services.retry_service import RetryService

BASE_DIR = Path.home() / 'ZakonDownloader'
PATHS = AppPaths(BASE_DIR)


def _setup_logging(log_level: str, log_file: Path) -> None:
    log_file.parent.mkdir(parents=True, exist_ok=True)
    level = getattr(logging, log_level.upper(), logging.INFO)
    logging.basicConfig(
        level=level,
        format='%(asctime)s [%(levelname)s] %(name)s: %(message)s',
        handlers=[
            logging.StreamHandler(),
            logging.FileHandler(log_file, encoding='utf-8'),
        ],
    )
    logging.getLogger().setLevel(level)


class _RetryingTextClient:
    """HTTP session adapter used by DownloadService, with live user settings."""

    def __init__(self, settings: AppSettings) -> None:
        self._http: HttpClient
        self._retry: RetryService
        self.configure(settings)

    def configure(self, settings: AppSettings) -> None:
        new_http = HttpClient(
            timeout_connect=settings.request_timeout_connect,
            timeout_read=settings.request_timeout_read,
        )
        new_retry = RetryService(
            new_http,
            max_attempts=settings.max_retry_attempts,
        )
        old_http = getattr(self, '_http', None)
        self._http = new_http
        self._retry = new_retry
        if old_http is not None:
            old_http.close()

    def get(self, url: str) -> str:
        result = self._retry.fetch(url)
        if not result.success or result.response is None:
            raise HttpClientError(result.error, status_code=result.status_code)
        return result.response.text

    def close(self) -> None:
        self._http.close()


def _build_http_client(settings: AppSettings | None = None) -> _RetryingTextClient:
    return _RetryingTextClient(settings or AppSettings())


def run() -> None:
    PATHS.ensure_all()
    settings_manager = SettingsManager(PATHS.settings)
    settings = settings_manager.load()
    if not settings.documents_dir:
        settings.documents_dir = str(PATHS.documents)

    documents_dir = Path(settings.documents_dir).expanduser()
    documents_dir.mkdir(parents=True, exist_ok=True)
    _setup_logging(settings.log_level, PATHS.logs / 'zakon_downloader.log')
    log = logging.getLogger('zakon_downloader.app')
    log.info('Запуск ZakonDownloader')

    repository = HistoryRepository(PATHS.history_db)
    history_service = HistoryService(repository)
    http_client = _build_http_client(settings)
    download_service = DownloadService(
        http_client=http_client,
        documents_dir=documents_dir,
        history_service=history_service,
    )
    log_service = LogExportService(PATHS.logs, PATHS.exported)

    def apply_settings(updated: AppSettings) -> None:
        nonlocal settings
        target_dir = Path(updated.documents_dir).expanduser()
        target_dir.mkdir(parents=True, exist_ok=True)
        network_changed = (
            updated.request_timeout_connect != settings.request_timeout_connect
            or updated.request_timeout_read != settings.request_timeout_read
            or updated.max_retry_attempts != settings.max_retry_attempts
        )
        if network_changed:
            http_client.configure(updated)
        if target_dir != Path(settings.documents_dir).expanduser():
            download_service.set_documents_dir(target_dir)
        settings_manager.save(updated)
        logging.getLogger().setLevel(
            getattr(logging, updated.log_level.upper(), logging.INFO)
        )
        settings = updated
        log.info('Налаштування застосовано.')

    app = QApplication(sys.argv)
    app.setStyle('Fusion')
    window = MainWindow(
        download_service=download_service,
        history_service=history_service,
        log_service=log_service,
        settings=settings,
        documents_dir=documents_dir,
        data_dir=PATHS.data,
        logs_dir=PATHS.logs,
        on_settings_changed=apply_settings,
    )
    window.show()
    try:
        sys.exit(app.exec())
    finally:
        http_client.close()
        repository.close()
