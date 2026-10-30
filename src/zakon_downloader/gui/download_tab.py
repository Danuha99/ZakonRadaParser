from __future__ import annotations

import os
from pathlib import Path

from PySide6.QtCore import Signal
from PySide6.QtWidgets import (
    QApplication,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMessageBox,
    QPushButton,
    QProgressBar,
    QTextEdit,
    QVBoxLayout,
    QWidget,
)

from zakon_downloader.gui.workers import DownloadWorker
from zakon_downloader.models.download_result import DownloadResult


class DownloadTab(QWidget):
    busy_changed = Signal(bool)
    status_message = Signal(str)

    def __init__(self, download_service, documents_dir: Path, parent=None) -> None:
        super().__init__(parent)
        self._service = download_service
        self._documents_dir = documents_dir
        self._worker: DownloadWorker | None = None
        self._last_path: Path | None = None
        self._build_ui()

    def _build_ui(self) -> None:
        root = QVBoxLayout(self)
        root.setContentsMargins(28, 26, 28, 24)
        root.setSpacing(16)

        title = QLabel('Завантаження документа')
        title.setObjectName('label_title')
        root.addWidget(title)
        intro = QLabel(
            'Вставте пряме посилання на відкритий документ із zakon.rada.gov.ua. '
            'Завантаження виконується у фоні, інтерфейс залишається доступним.'
        )
        intro.setObjectName('label_hint')
        intro.setWordWrap(True)
        root.addWidget(intro)

        url_group = QGroupBox('Посилання на документ')
        url_layout = QVBoxLayout(url_group)
        url_layout.setContentsMargins(18, 22, 18, 18)
        url_layout.setSpacing(10)
        row = QHBoxLayout()
        row.setSpacing(8)
        self._url_input = QLineEdit()
        self._url_input.setPlaceholderText('https://zakon.rada.gov.ua/laws/show/...')
        self._url_input.setMinimumHeight(42)
        self._url_input.setClearButtonEnabled(True)
        self._url_input.returnPressed.connect(self._start_download)
        row.addWidget(self._url_input, 1)

        paste = QPushButton('Вставити з буфера')
        paste.setMinimumHeight(42)
        paste.clicked.connect(self._paste_url)
        row.addWidget(paste)

        self._btn_download = QPushButton('Завантажити документ')
        self._btn_download.setObjectName('btn_primary')
        self._btn_download.setMinimumHeight(42)
        self._btn_download.clicked.connect(self._start_download)
        row.addWidget(self._btn_download)
        url_layout.addLayout(row)
        root.addWidget(url_group)

        self._progress = QProgressBar()
        self._progress.setRange(0, 0)
        self._progress.setFixedHeight(8)
        self._progress.setTextVisible(False)
        self._progress.setVisible(False)
        root.addWidget(self._progress)

        result_header = QHBoxLayout()
        result_title = QLabel('Результат операції')
        result_title.setObjectName('label_section')
        result_header.addWidget(result_title)
        result_header.addStretch(1)
        clear = QPushButton('Очистити вікно результату')
        clear.clicked.connect(self._clear_result)
        result_header.addWidget(clear)
        root.addLayout(result_header)

        self._log = QTextEdit()
        self._log.setReadOnly(True)
        self._log.setMinimumHeight(220)
        self._log.setPlaceholderText(
            'Тут з’явиться перебіг операції, назва документа, шлях або опис помилки.'
        )
        root.addWidget(self._log, 1)

        actions = QHBoxLayout()
        self._btn_open_file = QPushButton('Відкрити останній файл')
        self._btn_open_file.setMinimumHeight(38)
        self._btn_open_file.setEnabled(False)
        self._btn_open_file.clicked.connect(self._open_file)
        actions.addWidget(self._btn_open_file)

        open_documents = QPushButton('Відкрити папку документів')
        open_documents.setMinimumHeight(38)
        open_documents.clicked.connect(self._open_documents_dir)
        actions.addWidget(open_documents)
        actions.addStretch(1)
        root.addLayout(actions)

    def _paste_url(self) -> None:
        text = QApplication.clipboard().text().strip()
        if text:
            self._url_input.setText(text)
            self._url_input.setFocus()
        else:
            self.status_message.emit('Буфер обміну порожній.')

    def _start_download(self) -> None:
        if self._worker is not None:
            return
        url = self._url_input.text().strip()
        if not url:
            QMessageBox.warning(self, 'Потрібне посилання', 'Вставте URL документа.')
            self._url_input.setFocus()
            return
        self._set_busy(True)
        self._log.clear()
        self._last_path = None
        self._btn_open_file.setEnabled(False)
        self._log.append('Перевірка посилання та підготовка завантаження…')
        self._worker = DownloadWorker(self._service, url, parent=self)
        self._worker.progress.connect(self._on_progress)
        self._worker.result_ready.connect(self._on_finished)
        self._worker.error.connect(self._on_error)
        self._worker.finished.connect(self._on_worker_finished)
        self._worker.start()

    def _on_progress(self, message: str) -> None:
        self._log.append(f'Етап: {message}')
        self.status_message.emit(message)

    def _on_finished(self, result: DownloadResult) -> None:
        if result.success and result.saved_path:
            self._last_path = Path(result.saved_path)
            self._log.append(
                '\nСтатус: документ успішно завантажено.'
                f'\n    Назва: {result.document_title or "—"}'
                f'\n    Файл: {result.saved_path}'
            )
            self._btn_open_file.setEnabled(self._last_path.is_file())
            self.status_message.emit('Документ успішно збережено.')
            return
        message = result.error_message or 'Не вдалося завантажити документ.'
        self._log.append(f'\nСтатус: операцію не завершено. {message}')
        for error in result.errors:
            if error and error != message:
                self._log.append(f'    • {error}')
        self.status_message.emit('Завантаження завершилося помилкою або без файла.')

    def _on_error(self, message: str) -> None:
        self._log.append(f'\nКритична помилка: {message}')
        self.status_message.emit('Сталася критична помилка.')

    def _on_worker_finished(self) -> None:
        self._set_busy(False)
        worker = self.sender()
        if worker is self._worker:
            self._worker = None
            worker.deleteLater()

    def _set_busy(self, busy: bool) -> None:
        self._btn_download.setEnabled(not busy)
        self._url_input.setEnabled(not busy)
        self._progress.setVisible(busy)
        self.busy_changed.emit(busy)

    def _clear_result(self) -> None:
        if self._worker is None:
            self._log.clear()

    def set_documents_dir(self, documents_dir: Path) -> None:
        self._documents_dir = documents_dir

    def _open_file(self) -> None:
        if self._last_path and self._last_path.is_file():
            os.startfile(str(self._last_path))

    def _open_documents_dir(self) -> None:
        self._documents_dir.mkdir(parents=True, exist_ok=True)
        os.startfile(str(self._documents_dir))
