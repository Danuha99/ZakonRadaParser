from __future__ import annotations

import os
from pathlib import Path

from PySide6.QtCore import Qt
from PySide6.QtWidgets import (
    QHBoxLayout,
    QLabel,
    QListWidget,
    QListWidgetItem,
    QMessageBox,
    QPushButton,
    QSplitter,
    QTextEdit,
    QVBoxLayout,
    QWidget,
)

from zakon_downloader.services.log_export_service import LogExportService


class LogsTab(QWidget):
    def __init__(
        self,
        log_service: LogExportService,
        logs_dir: Path,
        export_dir: Path,
        parent=None,
    ) -> None:
        super().__init__(parent)
        self._svc = log_service
        self._logs_dir = logs_dir
        self._export_dir = export_dir
        self._files: list[Path] = []
        self._build_ui()
        self.refresh()

    def _build_ui(self) -> None:
        layout = QVBoxLayout(self)
        layout.setContentsMargins(26, 24, 26, 22)
        layout.setSpacing(12)

        title = QLabel('Журнали та діагностика')
        title.setObjectName('label_title')
        layout.addWidget(title)
        subtitle = QLabel('Журнали допомагають з’ясувати результат операцій. Секрети та токени не журналюються.')
        subtitle.setObjectName('label_hint')
        subtitle.setWordWrap(True)
        layout.addWidget(subtitle)

        buttons = QHBoxLayout()
        buttons.setSpacing(8)
        refresh = QPushButton('Оновити список')
        refresh.setMinimumHeight(36)
        refresh.clicked.connect(self.refresh)
        buttons.addWidget(refresh)

        self._btn_export = QPushButton('Створити зведений файл логів')
        self._btn_export.setMinimumHeight(36)
        self._btn_export.setObjectName('btn_primary')
        self._btn_export.clicked.connect(self._export_combined)
        buttons.addWidget(self._btn_export)

        open_logs = QPushButton('Відкрити папку журналів')
        open_logs.setMinimumHeight(36)
        open_logs.clicked.connect(self._open_logs_dir)
        buttons.addWidget(open_logs)

        open_exports = QPushButton('Відкрити експорт')
        open_exports.setMinimumHeight(36)
        open_exports.clicked.connect(self._open_export_dir)
        buttons.addWidget(open_exports)
        buttons.addStretch(1)
        layout.addLayout(buttons)

        splitter = QSplitter(Qt.Orientation.Horizontal)
        left = QWidget()
        left_layout = QVBoxLayout(left)
        left_layout.setContentsMargins(0, 0, 0, 0)
        left_layout.setSpacing(8)
        list_label = QLabel('ФАЙЛИ ЖУРНАЛІВ')
        list_label.setObjectName('label_section')
        left_layout.addWidget(list_label)
        self._list = QListWidget()
        self._list.currentRowChanged.connect(self._on_file_selected)
        left_layout.addWidget(self._list)
        splitter.addWidget(left)

        right = QWidget()
        right_layout = QVBoxLayout(right)
        right_layout.setContentsMargins(0, 0, 0, 0)
        right_layout.setSpacing(8)
        view_label = QLabel('ВМІСТ ФАЙЛА')
        view_label.setObjectName('label_section')
        right_layout.addWidget(view_label)
        self._viewer = QTextEdit()
        self._viewer.setReadOnly(True)
        self._viewer.setLineWrapMode(QTextEdit.LineWrapMode.NoWrap)
        self._viewer.setStyleSheet('font-family:Consolas,monospace; font-size:12px;')
        right_layout.addWidget(self._viewer)
        splitter.addWidget(right)
        splitter.setStretchFactor(0, 1)
        splitter.setStretchFactor(1, 3)
        layout.addWidget(splitter, 1)

        self._status = QLabel('')
        self._status.setObjectName('label_hint')
        self._status.setWordWrap(True)
        self._status.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse)
        layout.addWidget(self._status)

    def refresh(self) -> None:
        error_message = ''
        try:
            self._files = sorted(
                self._svc.list_log_files(),
                key=lambda path: path.stat().st_mtime,
                reverse=True,
            )
        except OSError as exc:
            self._files = []
            error_message = f'Не вдалося прочитати список журналів: {exc}'
        self._list.clear()
        for path in self._files:
            try:
                size_kb = max(1, (path.stat().st_size + 1023) // 1024)
            except OSError:
                size_kb = 0
            item = QListWidgetItem(f'{path.name}  ·  {size_kb} КБ')
            item.setToolTip(str(path))
            self._list.addItem(item)
        self._btn_export.setEnabled(bool(self._files))
        if self._files:
            self._list.setCurrentRow(0)
            self._status.setText(f'Знайдено журналів: {len(self._files)}')
        else:
            self._viewer.clear()
            self._viewer.setPlaceholderText('Файлів журналів поки немає.')
            self._status.setText(error_message or 'Файлів журналів не знайдено.')

    def _on_file_selected(self, row: int) -> None:
        if row < 0 or row >= len(self._files):
            return
        path = self._files[row]
        try:
            self._viewer.setPlainText(self._svc.read_log(path))
            self._status.setText(f'Відкрито: {path}')
        except OSError as exc:
            self._viewer.clear()
            self._status.setText(f'Не вдалося прочитати журнал: {exc}')
        scrollbar = self._viewer.verticalScrollBar()
        scrollbar.setValue(scrollbar.maximum())

    def _export_combined(self) -> None:
        try:
            saved = self._svc.export_combined()
        except Exception as exc:
            QMessageBox.critical(self, 'Помилка експорту', str(exc))
            return
        self._status.setText(f'Зведений журнал збережено: {saved}')
        QMessageBox.information(
            self,
            'Експорт завершено',
            f'Файл збережено у папці data\\exported_logs:\n{saved}',
        )

    def _open_logs_dir(self) -> None:
        self._logs_dir.mkdir(parents=True, exist_ok=True)
        os.startfile(str(self._logs_dir))

    def _open_export_dir(self) -> None:
        self._export_dir.mkdir(parents=True, exist_ok=True)
        os.startfile(str(self._export_dir))
