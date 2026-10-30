from __future__ import annotations

import os
from datetime import date
from pathlib import Path

from PySide6.QtCore import QDate, Qt, QSortFilterProxyModel
from PySide6.QtGui import QStandardItem, QStandardItemModel
from PySide6.QtWidgets import (
    QAbstractItemView,
    QCheckBox,
    QComboBox,
    QDateEdit,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QPushButton,
    QTableView,
    QHeaderView,
    QVBoxLayout,
    QWidget,
)

from zakon_downloader.services.history_service import HistoryService

_HEADERS = ['ID', 'Запущено', 'Завершено', 'URL', 'Назва документа', 'Статус', 'Файл']


class _HistoryFilterProxy(QSortFilterProxyModel):
    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self._status = ''
        self._date_from: date | None = None
        self._date_to: date | None = None

    def set_filters(
        self,
        status: str = '',
        date_from: date | None = None,
        date_to: date | None = None,
    ) -> None:
        modern_filter_api = hasattr(self, 'beginFilterChange')
        if modern_filter_api:
            self.beginFilterChange()
        self._status = status
        self._date_from = date_from
        self._date_to = date_to
        if modern_filter_api:
            self.endFilterChange(QSortFilterProxyModel.Direction.Rows)
        else:
            self.invalidateFilter()

    def filterAcceptsRow(self, source_row: int, source_parent) -> bool:
        if not super().filterAcceptsRow(source_row, source_parent):
            return False
        model = self.sourceModel()
        if model is None:
            return False
        status = str(model.data(model.index(source_row, 5, source_parent)) or '')
        if self._status and status != self._status:
            return False
        started = str(model.data(model.index(source_row, 1, source_parent), Qt.UserRole) or '')
        if started:
            day = started[:10]
            if self._date_from and day < self._date_from.isoformat():
                return False
            if self._date_to and day > self._date_to.isoformat():
                return False
        return True


class HistoryTab(QWidget):
    def __init__(
        self,
        history_service: HistoryService,
        logs_dir: Path | None = None,
        parent=None,
    ) -> None:
        super().__init__(parent)
        self._history = history_service
        self._logs_dir = logs_dir
        self._entries_by_id = {}
        self._build_ui()
        self.refresh()

    def _build_ui(self) -> None:
        layout = QVBoxLayout(self)
        layout.setContentsMargins(26, 24, 26, 22)
        layout.setSpacing(12)

        title = QLabel('Історія операцій')
        title.setObjectName('label_title')
        layout.addWidget(title)
        subtitle = QLabel('Записи зберігаються локально й не видаляються з інтерфейсу.')
        subtitle.setObjectName('label_hint')
        layout.addWidget(subtitle)

        filters = QHBoxLayout()
        filters.setSpacing(8)
        self._search = QLineEdit()
        self._search.setPlaceholderText('Пошук за URL, назвою або статусом…')
        self._search.setClearButtonEnabled(True)
        self._search.setMinimumHeight(36)
        self._search.textChanged.connect(self._apply_filters)
        filters.addWidget(self._search, 1)

        self._status_filter = QComboBox()
        self._status_filter.addItem('Усі статуси', '')
        for status in ('Розпочато', 'Успішно', 'Помилка', 'Пропущено'):
            self._status_filter.addItem(status, status)
        self._status_filter.setMinimumHeight(36)
        self._status_filter.currentIndexChanged.connect(self._apply_filters)
        filters.addWidget(self._status_filter)

        self._date_filter = QCheckBox('Період')
        self._date_filter.toggled.connect(self._on_date_filter_toggled)
        filters.addWidget(self._date_filter)
        self._date_from = self._make_date_edit(QDate.currentDate().addDays(-30))
        self._date_to = self._make_date_edit(QDate.currentDate())
        self._date_from.setEnabled(False)
        self._date_to.setEnabled(False)
        self._date_from.dateChanged.connect(self._apply_filters)
        self._date_to.dateChanged.connect(self._apply_filters)
        filters.addWidget(self._date_from)
        filters.addWidget(QLabel('—'))
        filters.addWidget(self._date_to)

        refresh = QPushButton('Оновити')
        refresh.setMinimumHeight(36)
        refresh.clicked.connect(self.refresh)
        filters.addWidget(refresh)
        layout.addLayout(filters)

        self._model = QStandardItemModel(self)
        self._model.setHorizontalHeaderLabels(_HEADERS)
        self._proxy = _HistoryFilterProxy(self)
        self._proxy.setSourceModel(self._model)
        self._proxy.setFilterCaseSensitivity(Qt.CaseInsensitive)
        self._proxy.setFilterKeyColumn(-1)
        self._proxy.setSortRole(Qt.UserRole)

        self._table = QTableView()
        self._table.setModel(self._proxy)
        self._table.setSelectionBehavior(QAbstractItemView.SelectRows)
        self._table.setSelectionMode(QAbstractItemView.SingleSelection)
        self._table.setEditTriggers(QAbstractItemView.NoEditTriggers)
        self._table.setSortingEnabled(True)
        self._table.setAlternatingRowColors(True)
        self._table.setShowGrid(False)
        self._table.verticalHeader().setVisible(False)
        header = self._table.horizontalHeader()
        header.setSectionResizeMode(QHeaderView.Interactive)
        header.setSectionResizeMode(0, QHeaderView.ResizeToContents)
        header.setSectionResizeMode(1, QHeaderView.ResizeToContents)
        header.setSectionResizeMode(2, QHeaderView.ResizeToContents)
        header.setSectionResizeMode(3, QHeaderView.Stretch)
        header.setSectionResizeMode(5, QHeaderView.ResizeToContents)
        self._table.doubleClicked.connect(self._open_selected_file)
        self._table.selectionModel().selectionChanged.connect(self._update_actions)
        layout.addWidget(self._table, 1)

        self._details = QLabel('Виберіть запис, щоб переглянути його результат або помилку.')
        self._details.setObjectName('label_hint')
        self._details.setWordWrap(True)
        self._details.setTextInteractionFlags(Qt.TextSelectableByMouse)
        layout.addWidget(self._details)

        actions = QHBoxLayout()
        self._btn_open_file = QPushButton('Відкрити файл')
        self._btn_open_folder = QPushButton('Відкрити папку файла')
        self._btn_open_logs = QPushButton('Відкрити журнали')
        self._btn_open_file.clicked.connect(self._open_selected_file)
        self._btn_open_folder.clicked.connect(self._open_selected_folder)
        self._btn_open_logs.clicked.connect(self._open_logs_dir)
        actions.addWidget(self._btn_open_file)
        actions.addWidget(self._btn_open_folder)
        actions.addWidget(self._btn_open_logs)
        actions.addStretch(1)
        layout.addLayout(actions)

    def set_user_mode(self, enabled: bool) -> None:
        """Keep log-folder diagnostics out of the streamlined user view."""
        self._btn_open_logs.setVisible(not enabled)

    @staticmethod
    def _make_date_edit(initial: QDate) -> QDateEdit:
        edit = QDateEdit(initial)
        edit.setCalendarPopup(True)
        edit.setDisplayFormat('dd.MM.yyyy')
        edit.setMinimumWidth(116)
        edit.setMinimumHeight(34)
        return edit

    def _on_date_filter_toggled(self, enabled: bool) -> None:
        self._date_from.setEnabled(enabled)
        self._date_to.setEnabled(enabled)
        self._apply_filters()

    def _as_date(self, value: QDate) -> date:
        return date(value.year(), value.month(), value.day())

    def _apply_filters(self, *_args) -> None:
        status = str(self._status_filter.currentData() or '')
        date_from = self._as_date(self._date_from.date()) if self._date_filter.isChecked() else None
        date_to = self._as_date(self._date_to.date()) if self._date_filter.isChecked() else None
        if date_from and date_to and date_from > date_to:
            date_from, date_to = date_to, date_from
        self._proxy.setFilterFixedString(self._search.text().strip())
        self._proxy.set_filters(status, date_from, date_to)
        self._update_actions()

    def refresh(self) -> None:
        self._model.setRowCount(0)
        self._entries_by_id.clear()
        for entry in self._history.get_all(limit=None):
            started = entry.started_at
            finished = entry.finished_at
            status = entry.status.value if entry.status else ''
            row = [
                QStandardItem(str(entry.id or '')),
                QStandardItem(started.strftime('%d.%m.%Y %H:%M') if started else ''),
                QStandardItem(finished.strftime('%d.%m.%Y %H:%M') if finished else ''),
                QStandardItem(entry.url),
                QStandardItem(entry.document_title or ''),
                QStandardItem(status),
                QStandardItem(entry.saved_path or ''),
            ]
            sort_values = [
                int(entry.id or 0),
                started.isoformat() if started else '',
                finished.isoformat() if finished else '',
                entry.url.casefold(),
                (entry.document_title or '').casefold(),
                status.casefold(),
                (entry.saved_path or '').casefold(),
            ]
            for item, sort_value in zip(row, sort_values):
                item.setEditable(False)
                item.setData(sort_value, Qt.UserRole)
                if entry.error_message:
                    item.setToolTip(entry.error_message)
            self._model.appendRow(row)
            if entry.id is not None:
                self._entries_by_id[entry.id] = entry
        self._apply_filters()

    def _selected_entry(self):
        indexes = self._table.selectedIndexes()
        if not indexes:
            return None
        row = indexes[0].row()
        entry_id = self._proxy.data(self._proxy.index(row, 0))
        try:
            return self._entries_by_id.get(int(entry_id))
        except (TypeError, ValueError):
            return None

    def _update_actions(self, *_args) -> None:
        entry = self._selected_entry()
        path = Path(entry.saved_path) if entry and entry.saved_path else None
        self._btn_open_file.setEnabled(bool(path and path.is_file()))
        self._btn_open_folder.setEnabled(bool(path and path.parent.is_dir()))
        self._btn_open_logs.setEnabled(bool(self._logs_dir and self._logs_dir.is_dir()))
        if entry is None:
            self._details.setText('Виберіть запис, щоб переглянути його результат або помилку.')
            return
        details = [f'URL: {entry.url}', f'Статус: {entry.status.value}']
        if entry.error_message:
            details.append(f'Деталі: {entry.error_message}')
        if entry.saved_path:
            details.append(f'Файл: {entry.saved_path}')
        self._details.setText('   ·   '.join(details))

    def _open_selected_file(self, *_args) -> None:
        entry = self._selected_entry()
        if entry and entry.saved_path and Path(entry.saved_path).is_file():
            os.startfile(entry.saved_path)

    def _open_selected_folder(self) -> None:
        entry = self._selected_entry()
        if entry and entry.saved_path:
            folder = Path(entry.saved_path).parent
            if folder.is_dir():
                os.startfile(str(folder))

    def _open_logs_dir(self) -> None:
        if self._logs_dir and self._logs_dir.is_dir():
            os.startfile(str(self._logs_dir))
