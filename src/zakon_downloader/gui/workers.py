from __future__ import annotations
from PySide6.QtCore import QThread, Signal
from zakon_downloader.models.download_result import DownloadResult
from zakon_downloader.services.download_service import DownloadService


class DownloadWorker(QThread):
    result_ready = Signal(object)  # DownloadResult
    progress  = Signal(str)      # рядок прогресу
    error     = Signal(str)      # критична помилка

    def __init__(
        self,
        service: DownloadService,
        url: str,
        parent=None,
    ) -> None:
        super().__init__(parent)
        self._service = service
        self._url     = url

    def run(self) -> None:
        try:
            self.progress.emit('Валідація URL...')
            self.progress.emit('Завантаження сторінки...')
            result: DownloadResult = self._service.download(self._url)
            self.progress.emit(
                'Завантаження завершено.' if result.success
                else 'Операцію завершено з помилкою.'
            )
            self.result_ready.emit(result)
        except Exception as exc:
            self.error.emit(str(exc))
