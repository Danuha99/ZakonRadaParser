from __future__ import annotations

import os
from pathlib import Path
from typing import Callable

from PySide6.QtCore import QEvent, Qt
from PySide6.QtWidgets import (
    QApplication,
    QComboBox,
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QHeaderView,
    QLabel,
    QMainWindow,
    QMessageBox,
    QPushButton,
    QStatusBar,
    QStackedWidget,
    QTableWidget,
    QTableWidgetItem,
    QTabWidget,
    QVBoxLayout,
    QWidget,
)

from zakon_downloader.gui.about_tab import AboutTab
from zakon_downloader.gui.download_tab import DownloadTab
from zakon_downloader.gui.history_tab import HistoryTab
from zakon_downloader.gui.logs_tab import LogsTab
from zakon_downloader.gui.settings_tab import SettingsTab
from zakon_downloader.gui.themes import DARK, LIGHT, build_qss
from zakon_downloader.models.app_settings import AppSettings
from zakon_downloader.models.history_entry import OperationStatus
from zakon_downloader.services.log_export_service import LogExportService

APP_TITLE = 'ZakonDownloader'
APP_VERSION = '1.0.0'

_PAGE_INFO = (
    ('Огляд', 'Зведення останніх операцій та швидкий доступ до завантаження.'),
    ('Завантаження', 'Отримання відкритих нормативних документів із zakon.rada.gov.ua.'),
    ('Історія', 'Пошук, фільтри й результати попередніх завантажень.'),
    ('Журнали', 'Технічні події та зведений експорт журналів.'),
    ('Налаштування', 'Тема, папка документів і параметри мережевих запитів.'),
    ('Про програму', 'Інформація про ZakonDownloader, дані та правила безпеки.'),
)
_NAV_ITEMS = (
    ('⌂', 'Огляд'),
    ('↓', 'Завантаження'),
    ('◷', 'Історія'),
    ('≡', 'Журнали'),
    ('⚙', 'Налаштування'),
    ('ⓘ', 'Про програму'),
)


class MainWindow(QMainWindow):
    def __init__(
        self,
        download_service,
        history_service,
        log_service: LogExportService,
        settings: AppSettings | None = None,
        documents_dir: Path | None = None,
        data_dir: Path | None = None,
        logs_dir: Path | None = None,
        on_settings_changed: Callable[[AppSettings], None] | None = None,
        parent=None,
    ) -> None:
        super().__init__(parent)
        self._documents_dir = documents_dir or (Path.home() / 'ZakonDownloader' / 'data' / 'documents')
        self._data_dir = data_dir or (Path.home() / 'ZakonDownloader' / 'data')
        self._logs_dir = logs_dir or (self._data_dir / 'logs')
        self._settings = settings or AppSettings(documents_dir=str(self._documents_dir))
        if not self._settings.documents_dir:
            self._settings.documents_dir = str(self._documents_dir)
        self._on_settings_changed_callback = on_settings_changed
        self._operation_busy = False
        self._history_service = history_service
        self._nav_buttons: list[QPushButton] = []

        self.setWindowTitle(f'{APP_TITLE}  ·  {APP_VERSION}')
        self.setMinimumSize(1020, 680)
        self.resize(1240, 800)
        self._build_ui(download_service, history_service, log_service)
        self._apply_theme()
        self._apply_mode()
        self._show_overview()

    def _build_ui(self, download_service, history_service, log_service) -> None:
        shell = QWidget(self)
        shell_layout = QHBoxLayout(shell)
        shell_layout.setContentsMargins(0, 0, 0, 0)
        shell_layout.setSpacing(0)

        sidebar = QFrame()
        sidebar.setObjectName('sidebar')
        sidebar.setFixedWidth(224)
        side_layout = QVBoxLayout(sidebar)
        side_layout.setContentsMargins(16, 22, 16, 18)
        side_layout.setSpacing(5)

        brand = QHBoxLayout()
        brand.setContentsMargins(7, 4, 4, 20)
        mark = QLabel('Z')
        mark.setObjectName('sidebar_brand_icon')
        mark.setAlignment(Qt.AlignmentFlag.AlignCenter)
        mark.setFixedSize(38, 38)
        name = QLabel('ZakonDownloader')
        name.setObjectName('brand_name')
        brand.addWidget(mark)
        brand.addWidget(name, 1)
        side_layout.addLayout(brand)

        section = QLabel('РОБОЧИЙ ПРОСТІР')
        section.setObjectName('sidebar_section_label')
        section.setContentsMargins(9, 7, 0, 4)
        self._sidebar_section = section
        side_layout.addWidget(section)

        self._nav_buttons = []
        for index, (icon, label) in enumerate(_NAV_ITEMS):
            button = QPushButton(f'{icon}    {label}')
            button.setObjectName('nav_button')
            button.setProperty('active', False)
            button.setCheckable(True)
            button.setMinimumHeight(42)
            button.setCursor(Qt.CursorShape.PointingHandCursor)
            button.setToolTip(label)
            button.clicked.connect(lambda _checked=False, i=index: self._navigate(i))
            side_layout.addWidget(button)
            self._nav_buttons.append(button)

        side_layout.addStretch(1)
        side_note = QLabel('ПУБЛІЧНІ ДАНІ\nzakon.rada.gov.ua')
        side_note.setObjectName('sidebar_note')
        side_note.setContentsMargins(9, 12, 0, 5)
        self._sidebar_note = side_note
        side_layout.addWidget(side_note)
        shell_layout.addWidget(sidebar)

        content = QWidget()
        content.setObjectName('content_area')
        content_layout = QVBoxLayout(content)
        content_layout.setContentsMargins(28, 22, 28, 16)
        content_layout.setSpacing(15)

        header = QHBoxLayout()
        header.setSpacing(12)
        title_stack = QVBoxLayout()
        title_stack.setSpacing(3)
        self._page_title = QLabel('Огляд')
        self._page_title.setObjectName('page_title')
        self._page_subtitle = QLabel(_PAGE_INFO[0][1])
        self._page_subtitle.setObjectName('page_subtitle')
        self._page_subtitle.setWordWrap(True)
        title_stack.addWidget(self._page_title)
        title_stack.addWidget(self._page_subtitle)
        header.addLayout(title_stack, 1)
        self._mode_selector = QComboBox()
        self._mode_selector.setObjectName('mode_selector')
        self._mode_selector.addItem('Користувач', 'user')
        self._mode_selector.addItem('Розробник', 'developer')
        self._mode_selector.setToolTip(
            'Режим змінює лише видимість інструментів, а не доступ чи безпеку даних.'
        )
        self._mode_selector.setMinimumHeight(36)
        self._mode_selector.setCurrentIndex(
            max(0, self._mode_selector.findData(self._settings.ui_mode))
        )
        self._mode_selector.currentIndexChanged.connect(self._on_mode_changed)
        header.addWidget(self._mode_selector)
        self._btn_theme = QPushButton()
        self._btn_theme.setObjectName('theme_toggle')
        self._btn_theme.setMinimumHeight(36)
        self._btn_theme.setCursor(Qt.CursorShape.PointingHandCursor)
        self._btn_theme.clicked.connect(self._toggle_theme)
        header.addWidget(self._btn_theme, 0, Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignVCenter)
        content_layout.addLayout(header)

        self._pages = QStackedWidget()
        self._overview = self._build_overview()
        self._pages.addWidget(self._overview)

        # Keep the original tab widget and indexes intact for compatibility, but
        # drive it using the more discoverable sidebar navigation.
        self._tabs = QTabWidget()
        self._tabs.setObjectName('content_tabs')
        self._tabs.setDocumentMode(True)
        self._tabs.setMovable(False)
        self._download_tab = DownloadTab(download_service, self._documents_dir)
        self._download_index = self._tabs.addTab(self._download_tab, 'Завантаження')
        self._history_tab = HistoryTab(history_service, self._logs_dir)
        self._history_index = self._tabs.addTab(self._history_tab, 'Історія')
        self._logs_tab = LogsTab(log_service, self._logs_dir, self._data_dir / 'exported_logs')
        self._logs_index = self._tabs.addTab(self._logs_tab, 'Журнали')
        self._settings_tab = SettingsTab(self._settings, self._documents_dir)
        self._settings_index = self._tabs.addTab(self._settings_tab, 'Налаштування')
        self._about_tab = AboutTab(
            APP_VERSION, self._data_dir, Path(self._settings.documents_dir)
        )
        self._about_index = self._tabs.addTab(self._about_tab, 'Про програму')
        self._tabs.tabBar().hide()
        self._tabs.currentChanged.connect(self._on_tab_changed)
        self._pages.addWidget(self._tabs)
        content_layout.addWidget(self._pages, 1)

        self._settings_tab.settings_saved.connect(self._commit_settings)
        self._download_tab.busy_changed.connect(self._set_operation_busy)
        self._download_tab.status_message.connect(self._show_status)
        shell_layout.addWidget(content, 1)
        self.setCentralWidget(shell)

        self._status = QStatusBar()
        self._status.showMessage('Готово.', 3000)
        self._status.addPermanentWidget(QLabel(f'v{APP_VERSION}'))
        self.setStatusBar(self._status)

    def _build_overview(self) -> QWidget:
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(16)

        hero = QFrame()
        hero.setObjectName('overview_hero')
        hero_layout = QHBoxLayout(hero)
        hero_layout.setContentsMargins(23, 18, 22, 18)
        hero_layout.setSpacing(18)
        copy = QVBoxLayout()
        copy.setSpacing(5)
        eyebrow = QLabel('НОРМАТИВНІ ДОКУМЕНТИ УКРАЇНИ')
        eyebrow.setObjectName('hero_eyebrow')
        heading = QLabel('Усі потрібні документи —\nв одному місці.')
        heading.setObjectName('hero_heading')
        heading.setWordWrap(True)
        body = QLabel('Збереження відкритих чинних редакцій у зручному текстовому форматі.')
        body.setObjectName('hero_body')
        body.setWordWrap(True)
        copy.addWidget(eyebrow)
        copy.addWidget(heading)
        copy.addWidget(body)
        hero_layout.addLayout(copy, 1)
        self._btn_new_download = QPushButton('↓   Нове завантаження')
        self._btn_new_download.setObjectName('btn_primary')
        self._btn_new_download.setMinimumHeight(42)
        self._btn_new_download.setCursor(Qt.CursorShape.PointingHandCursor)
        self._btn_new_download.clicked.connect(lambda: self._navigate(1))
        hero_layout.addWidget(self._btn_new_download, 0, Qt.AlignmentFlag.AlignBottom)
        layout.addWidget(hero)

        metrics = QGridLayout()
        metrics.setSpacing(12)
        self._metric_total = self._metric_card('УСЬОГО ОПЕРАЦІЙ', '0', 0)
        self._metric_success = self._metric_card('УСПІШНО', '0', 1)
        self._metric_issues = self._metric_card('ПОТРЕБУЮТЬ УВАГИ', '0', 2)
        metrics.addWidget(self._metric_total, 0, 0)
        metrics.addWidget(self._metric_success, 0, 1)
        metrics.addWidget(self._metric_issues, 0, 2)
        for column in range(3):
            metrics.setColumnStretch(column, 1)
        layout.addLayout(metrics)

        recent_card = QFrame()
        recent_card.setObjectName('dashboard_card')
        recent_layout = QVBoxLayout(recent_card)
        recent_layout.setContentsMargins(16, 14, 16, 13)
        recent_layout.setSpacing(9)
        row = QHBoxLayout()
        recent_title = QLabel('Останні операції')
        recent_title.setObjectName('card_title')
        row.addWidget(recent_title)
        row.addStretch(1)
        history_button = QPushButton('Уся історія  ›')
        history_button.setObjectName('text_button')
        history_button.setCursor(Qt.CursorShape.PointingHandCursor)
        history_button.clicked.connect(lambda: self._navigate(2))
        row.addWidget(history_button)
        recent_layout.addLayout(row)

        self._recent_table = QTableWidget(0, 4)
        self._recent_table.setObjectName('recent_table')
        self._recent_table.setHorizontalHeaderLabels(['Дата', 'Документ / адреса', 'Статус', 'Результат'])
        self._recent_table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
        self._recent_table.setSelectionBehavior(QTableWidget.SelectionBehavior.SelectRows)
        self._recent_table.setSelectionMode(QTableWidget.SelectionMode.SingleSelection)
        self._recent_table.setShowGrid(False)
        self._recent_table.verticalHeader().setVisible(False)
        self._recent_table.setAlternatingRowColors(True)
        self._recent_table.setMinimumHeight(190)
        self._recent_table.setMaximumHeight(280)
        self._recent_table.doubleClicked.connect(self._open_recent_record)
        header = self._recent_table.horizontalHeader()
        header.setSectionResizeMode(0, QHeaderView.ResizeMode.ResizeToContents)
        header.setSectionResizeMode(1, QHeaderView.ResizeMode.Stretch)
        header.setSectionResizeMode(2, QHeaderView.ResizeMode.ResizeToContents)
        header.setSectionResizeMode(3, QHeaderView.ResizeMode.Stretch)
        recent_layout.addWidget(self._recent_table)
        layout.addWidget(recent_card, 1)

        footer = QLabel('Завантажуються лише загальнодоступні сторінки. Захисні механізми сайтів не обходяться.')
        footer.setObjectName('dashboard_note')
        footer.setWordWrap(True)
        layout.addWidget(footer)
        return page

    @staticmethod
    def _metric_card(label: str, value: str, index: int) -> QFrame:
        card = QFrame()
        card.setObjectName('metric_featured' if index == 0 else 'metric_card')
        layout = QVBoxLayout(card)
        layout.setContentsMargins(16, 12, 16, 11)
        layout.setSpacing(5)
        caption = QLabel(label)
        caption.setObjectName('metric_caption')
        number = QLabel(value)
        number.setObjectName('metric_value')
        layout.addWidget(caption)
        layout.addWidget(number)
        card.metric_value = number
        return card

    def _show_overview(self) -> None:
        self._pages.setCurrentWidget(self._overview)
        self._page_title.setText(_PAGE_INFO[0][0])
        self._page_subtitle.setText(_PAGE_INFO[0][1])
        self._set_active_nav(0)
        self._refresh_overview()

    def _refresh_overview(self) -> None:
        try:
            entries = self._history_service.get_all(limit=None)
        except Exception as exc:
            self._status.showMessage(f'Не вдалося оновити огляд: {exc}', 7000)
            entries = []
        successful = sum(entry.status == OperationStatus.SUCCESS for entry in entries)
        issues = sum(entry.status in (OperationStatus.FAILED, OperationStatus.SKIPPED) for entry in entries)
        self._metric_total.metric_value.setText(str(len(entries)))
        self._metric_success.metric_value.setText(str(successful))
        self._metric_issues.metric_value.setText(str(issues))
        self._recent_table.setRowCount(0)
        for entry in entries[:6]:
            row = self._recent_table.rowCount()
            self._recent_table.insertRow(row)
            started = entry.started_at.strftime('%d.%m.%Y %H:%M') if entry.started_at else '—'
            title = entry.document_title or entry.url
            status = entry.status.value if entry.status else '—'
            outcome = 'Збережено' if entry.saved_path else (entry.error_message or '—')
            for column, value in enumerate((started, title, status, outcome)):
                item = QTableWidgetItem(str(value))
                item.setToolTip(str(value))
                if column == 3 and entry.saved_path:
                    item.setData(Qt.ItemDataRole.UserRole, entry.saved_path)
                    item.setToolTip(
                        f'{value}\nФайл: {entry.saved_path}\nДвічі клацніть, щоб відкрити.'
                    )
                self._recent_table.setItem(row, column, item)
        if not entries:
            self._recent_table.setRowCount(1)
            empty = QTableWidgetItem('Історія поки порожня. Розпочніть із першого завантаження.')
            empty.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
            self._recent_table.setItem(0, 1, empty)
            self._recent_table.setSpan(0, 1, 1, 3)

    def _navigate(self, index: int) -> None:
        if self._settings.ui_mode != 'developer' and index in (3, 5):
            return
        if index == 0:
            self._show_overview()
            return
        tab_index = index - 1
        self._pages.setCurrentWidget(self._tabs)
        self._tabs.setCurrentIndex(tab_index)
        self._set_active_nav(index)
        self._update_page_header(index)
        if tab_index == self._history_index:
            self._history_tab.refresh()
        elif tab_index == self._logs_index:
            self._logs_tab.refresh()

    def _open_recent_record(self, index) -> None:
        result_item = self._recent_table.item(index.row(), 3)
        path_text = result_item.data(Qt.ItemDataRole.UserRole) if result_item else None
        if not path_text:
            self._status.showMessage('Для цієї операції немає збереженого файла.', 5000)
            return
        path = Path(str(path_text))
        if not path.is_file():
            self._status.showMessage('Збережений файл не знайдено на диску.', 5000)
            return
        try:
            os.startfile(str(path))
            self._status.showMessage('Файл відкрито.', 3000)
        except OSError as exc:
            self._status.showMessage(f'Не вдалося відкрити файл: {exc}', 7000)

    def _set_active_nav(self, index: int) -> None:
        for item_index, button in enumerate(self._nav_buttons):
            active = item_index == index
            button.setChecked(active)
            button.setProperty('active', active)
            button.style().unpolish(button)
            button.style().polish(button)

    def _update_page_header(self, index: int) -> None:
        if 0 <= index < len(_PAGE_INFO):
            title, subtitle = _PAGE_INFO[index]
            self._page_title.setText(title)
            self._page_subtitle.setText(subtitle)

    def _apply_theme(self) -> None:
        application = QApplication.instance()
        if application is not None:
            theme = DARK if self._settings.theme == 'dark' else LIGHT
            application.setStyleSheet(build_qss(theme))
        self._btn_theme.setText(
            '☼  Світла тема' if self._settings.theme == 'dark' else '☾  Темна тема'
        )
        self._btn_theme.setToolTip(
            'Перемкнути на світлу тему' if self._settings.theme == 'dark'
            else 'Перемкнути на темну тему'
        )

    def _apply_mode(self) -> None:
        developer_mode = self._settings.ui_mode == 'developer'
        for index, button in enumerate(self._nav_buttons):
            button.setVisible(developer_mode or index not in (3, 5))
        self._sidebar_section.setText(
            'ІНСТРУМЕНТИ РОЗРОБНИКА' if developer_mode else 'РОБОЧИЙ ПРОСТІР'
        )
        self._sidebar_note.setText(
            'ТЕХНІЧНИЙ РЕЖИМ\nУсі розділи доступні' if developer_mode
            else 'ДОКУМЕНТИ\nЗБЕРІГАЮТЬСЯ ЛОКАЛЬНО'
        )
        self._history_tab.set_user_mode(not developer_mode)
        self._settings_tab.set_user_mode(not developer_mode)
        selected = self._mode_selector.findData(self._settings.ui_mode)
        self._mode_selector.blockSignals(True)
        self._mode_selector.setCurrentIndex(max(0, selected))
        self._mode_selector.blockSignals(False)
        if not developer_mode and self._tabs.currentIndex() in (2, 4):
            self._show_overview()

    def _on_mode_changed(self, _index: int) -> None:
        mode = str(self._mode_selector.currentData() or 'user')
        if mode != self._settings.ui_mode:
            self._commit_settings(self._copy_settings(ui_mode=mode))

    def _toggle_theme(self) -> None:
        theme = 'light' if self._settings.theme == 'dark' else 'dark'
        self._commit_settings(self._copy_settings(theme=theme))

    def _copy_settings(self, **changes) -> AppSettings:
        values = dict(vars(self._settings))
        values.update(changes)
        return AppSettings(**values)

    def _commit_settings(self, settings: AppSettings) -> None:
        try:
            if self._on_settings_changed_callback is not None:
                self._on_settings_changed_callback(settings)
        except Exception as exc:
            self._settings_tab.set_settings(self._settings)
            self._apply_mode()
            QMessageBox.critical(
                self,
                'Не вдалося застосувати налаштування',
                f'Зміни не застосовано.\n{exc}',
            )
            return

        self._settings = settings
        self._documents_dir = Path(settings.documents_dir)
        self._download_tab.set_documents_dir(self._documents_dir)
        self._about_tab.set_documents_dir(self._documents_dir)
        self._settings_tab.set_settings(settings)
        self._apply_theme()
        self._apply_mode()
        self._status.showMessage('Налаштування збережено й застосовано.', 5000)

    def _set_operation_busy(self, busy: bool) -> None:
        self._operation_busy = busy
        self._tabs.setTabEnabled(self._settings_index, not busy)
        self._nav_buttons[4].setEnabled(not busy)
        self._mode_selector.setEnabled(not busy)
        self._btn_theme.setEnabled(not busy)
        if busy:
            self._status.showMessage('Виконується завантаження…')
        else:
            self._status.showMessage('Операцію завершено.', 4000)

    def _show_status(self, message: str) -> None:
        self._status.showMessage(message, 6000)

    def _on_tab_changed(self, index: int) -> None:
        if self._settings.ui_mode != 'developer' and index in (2, 4):
            self._tabs.setCurrentIndex(self._download_index)
            return
        sidebar_index = index + 1
        self._set_active_nav(sidebar_index)
        self._update_page_header(sidebar_index)
        self._pages.setCurrentWidget(self._tabs)
        if index == self._history_index:
            self._history_tab.refresh()
        elif index == self._logs_index:
            self._logs_tab.refresh()

    def closeEvent(self, event: QEvent) -> None:
        if self._operation_busy:
            self._status.showMessage('Дочекайтеся завершення завантаження, перш ніж закривати програму.')
            event.ignore()
            return
        event.accept()
