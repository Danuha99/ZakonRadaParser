import os
from types import SimpleNamespace

os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')

import pytest
from PySide6.QtTest import QSignalSpy
from PySide6.QtWidgets import QApplication

from zakon_downloader.gui.history_tab import HistoryTab
from zakon_downloader.gui.main_window import MainWindow
from zakon_downloader.gui.settings_tab import SettingsTab
from zakon_downloader.models.app_settings import AppSettings
from zakon_downloader.models.history_entry import OperationStatus
from zakon_downloader.repositories.history_repository import HistoryRepository
from zakon_downloader.services.history_service import HistoryService
from zakon_downloader.services.log_export_service import LogExportService


@pytest.fixture(scope='session')
def qapp():
    return QApplication.instance() or QApplication([])


def test_settings_form_emits_all_user_changes(qapp, tmp_path):
    tab = SettingsTab(AppSettings(language='zh'), tmp_path / 'documents')
    spy = QSignalSpy(tab.settings_saved)
    tab._connect_timeout.setValue(22)
    tab._read_timeout.setValue(47)
    tab._max_attempts.setValue(5)
    tab._theme.setCurrentIndex(tab._theme.findData('dark'))
    tab._log_level.setCurrentIndex(tab._log_level.findData('DEBUG'))
    tab._documents_dir.setText(str(tmp_path / 'custom-docs'))

    tab._save()

    assert spy.count() == 1
    saved = spy.at(0)[0]
    assert saved.request_timeout_connect == 22
    assert saved.request_timeout_read == 47
    assert saved.max_retry_attempts == 5
    assert saved.theme == 'dark'
    assert saved.log_level == 'DEBUG'
    assert saved.documents_dir == str((tmp_path / 'custom-docs').resolve())
    assert saved.language == 'zh'


def test_settings_reset_preserves_native_language(qapp, tmp_path):
    tab = SettingsTab(AppSettings(theme='dark', language='pt'), tmp_path / 'documents')
    saved_settings = []
    tab.settings_saved.connect(saved_settings.append)

    tab._restore_defaults()
    tab._save()

    assert len(saved_settings) == 1
    assert saved_settings[0].theme == 'light'
    assert saved_settings[0].language == 'pt'


def test_main_window_exposes_all_five_tabs_and_persists_theme(qapp, tmp_path):
    class EmptyHistory:
        def get_all(self, limit=200):
            return []

    logs = tmp_path / 'logs'
    exports = tmp_path / 'exported_logs'
    docs = tmp_path / 'documents'
    applied = []
    window = MainWindow(
        object(),
        EmptyHistory(),
        LogExportService(logs, exports),
        AppSettings(documents_dir=str(docs)),
        docs,
        tmp_path / 'data',
        logs,
        applied.append,
    )
    assert [window._tabs.tabText(i) for i in range(window._tabs.count())] == [
        'Завантаження', 'Історія', 'Журнали', 'Налаштування', 'Про програму'
    ]

    window._toggle_theme()

    assert window._settings.theme == 'dark'
    assert len(applied) == 1 and applied[0].theme == 'dark'
    window.close()


def test_history_tab_filters_by_status_and_text(qapp, tmp_path):
    service = HistoryService(HistoryRepository(':memory:'))
    failed = service.start_operation('https://zakon.rada.gov.ua/laws/show/1')
    service.mark_failed(failed, 'Таймаут мережі')
    succeeded = service.start_operation('https://zakon.rada.gov.ua/laws/show/2')
    service.mark_success(succeeded, 'Тестовий закон', tmp_path / 'document.txt')

    tab = HistoryTab(service, tmp_path / 'logs')
    assert tab._proxy.rowCount() == 2

    tab._status_filter.setCurrentIndex(tab._status_filter.findData(OperationStatus.FAILED.value))
    assert tab._proxy.rowCount() == 1
    assert tab._proxy.data(tab._proxy.index(0, 5)) == OperationStatus.FAILED.value

    tab._status_filter.setCurrentIndex(0)
    tab._search.setText('Тестовий закон')
    assert tab._proxy.rowCount() == 1
    assert tab._proxy.data(tab._proxy.index(0, 4)) == 'Тестовий закон'


def test_sidebar_navigation_and_overview_preserve_original_tabs(qapp, tmp_path):
    class EmptyHistory:
        def get_all(self, limit=None):
            return []

    docs = tmp_path / 'documents'
    logs = tmp_path / 'logs'
    window = MainWindow(
        object(),
        EmptyHistory(),
        LogExportService(logs, tmp_path / 'exported_logs'),
        AppSettings(documents_dir=str(docs), ui_mode='developer'),
        docs,
        tmp_path / 'data',
        logs,
    )

    assert window._tabs.count() == 5
    assert window._pages.currentWidget() is window._overview
    assert window._metric_total.metric_value.text() == '0'

    for nav_index in range(1, 6):
        window._navigate(nav_index)
        assert window._pages.currentWidget() is window._tabs
        assert window._tabs.currentIndex() == nav_index - 1
        assert window._nav_buttons[nav_index].property('active') is True

    window._navigate(0)
    assert window._pages.currentWidget() is window._overview
    assert window._nav_buttons[0].property('active') is True
    window.close()


def test_user_and_developer_modes_filter_sections_and_persist(qapp, tmp_path):
    class EmptyHistory:
        def get_all(self, limit=None):
            return []

    docs = tmp_path / 'documents'
    logs = tmp_path / 'logs'
    saved = []
    window = MainWindow(
        object(),
        EmptyHistory(),
        LogExportService(logs, tmp_path / 'exported_logs'),
        AppSettings(documents_dir=str(docs)),
        docs,
        tmp_path / 'data',
        logs,
        saved.append,
    )

    assert window._settings.ui_mode == 'user'
    assert window._nav_buttons[3].isHidden()
    assert window._nav_buttons[5].isHidden()
    assert window._history_tab._btn_open_logs.isHidden()
    assert all(group.isHidden() for group in window._settings_tab._advanced_groups)
    assert 'журнал' not in window._sidebar_note.text().lower()
    assert 'журнал' not in window._settings_tab._intro.text().lower()

    for nav_index, tab_index in ((1, 0), (2, 1), (4, 3)):
        window._navigate(nav_index)
        assert window._tabs.currentIndex() == tab_index

    window._mode_selector.setCurrentIndex(window._mode_selector.findData('developer'))
    assert window._settings.ui_mode == 'developer'
    assert not window._nav_buttons[3].isHidden()
    assert not window._nav_buttons[5].isHidden()
    assert not window._history_tab._btn_open_logs.isHidden()
    assert all(not group.isHidden() for group in window._settings_tab._advanced_groups)
    assert saved[-1].ui_mode == 'developer'

    window._navigate(3)
    assert window._tabs.currentIndex() == 2
    window._mode_selector.setCurrentIndex(window._mode_selector.findData('user'))
    assert window._settings.ui_mode == 'user'
    assert window._pages.currentWidget() is window._overview
    assert window._nav_buttons[3].isHidden()
    assert saved[-1].ui_mode == 'user'
    window.close()


def test_dashboard_double_click_opens_saved_document(qapp, tmp_path, monkeypatch):
    saved_file = tmp_path / 'saved-document.txt'
    saved_file.write_text('Тестовий документ', encoding='utf-8')
    entry = SimpleNamespace(
        id=1,
        url='https://zakon.rada.gov.ua/laws/show/1',
        document_title='Тестовий документ',
        status=OperationStatus.SUCCESS,
        started_at=None,
        finished_at=None,
        saved_path=str(saved_file),
        error_message=None,
    )

    class History:
        def get_all(self, limit=None):
            return [entry]

    opened = []
    monkeypatch.setattr(os, 'startfile', lambda path: opened.append(path), raising=False)
    docs = tmp_path / 'documents'
    logs = tmp_path / 'logs'
    window = MainWindow(
        object(),
        History(),
        LogExportService(logs, tmp_path / 'exported_logs'),
        AppSettings(documents_dir=str(docs)),
        docs,
        tmp_path / 'data',
        logs,
    )

    result = window._recent_table.model().index(0, 0)
    window._recent_table.doubleClicked.emit(result)
    assert opened == [str(saved_file)]
    window.close()
