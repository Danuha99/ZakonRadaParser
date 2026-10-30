from __future__ import annotations

from pathlib import Path

from PySide6.QtCore import Signal
from PySide6.QtWidgets import (
    QFileDialog,
    QFormLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMessageBox,
    QPushButton,
    QComboBox,
    QSpinBox,
    QVBoxLayout,
    QWidget,
)

from zakon_downloader.models.app_settings import AppSettings


class SettingsTab(QWidget):
    settings_saved = Signal(object)

    def __init__(
        self,
        settings: AppSettings,
        default_documents_dir: Path,
        parent=None,
    ) -> None:
        super().__init__(parent)
        self._default_documents_dir = default_documents_dir
        self._advanced_groups: list[QGroupBox] = []
        self._build_ui()
        self.set_settings(settings)

    def _build_ui(self) -> None:
        root = QVBoxLayout(self)
        root.setContentsMargins(28, 26, 28, 24)
        root.setSpacing(16)

        title = QLabel('Налаштування')
        title.setObjectName('label_title')
        root.addWidget(title)
        self._intro = QLabel()
        self._intro.setObjectName('label_hint')
        self._intro.setWordWrap(True)
        root.addWidget(self._intro)

        appearance = QGroupBox('Вигляд і збереження')
        appearance_form = QFormLayout(appearance)
        appearance_form.setContentsMargins(18, 22, 18, 18)
        appearance_form.setHorizontalSpacing(18)
        appearance_form.setVerticalSpacing(14)

        self._theme = QComboBox()
        self._theme.addItem('Світла', 'light')
        self._theme.addItem('Темна', 'dark')
        appearance_form.addRow('Тема інтерфейсу', self._theme)

        folder_row = QWidget()
        folder_layout = QHBoxLayout(folder_row)
        folder_layout.setContentsMargins(0, 0, 0, 0)
        folder_layout.setSpacing(8)
        self._documents_dir = QLineEdit()
        self._documents_dir.setMinimumHeight(36)
        self._documents_dir.setToolTip('Папка для нових TXT-документів.')
        folder_layout.addWidget(self._documents_dir, 1)
        browse = QPushButton('Огляд…')
        browse.setMinimumHeight(36)
        browse.clicked.connect(self._choose_documents_dir)
        folder_layout.addWidget(browse)
        appearance_form.addRow('Папка документів', folder_row)
        root.addWidget(appearance)

        network = QGroupBox('Мережа та повторні спроби')
        network_form = QFormLayout(network)
        network_form.setContentsMargins(18, 22, 18, 18)
        network_form.setHorizontalSpacing(18)
        network_form.setVerticalSpacing(14)

        self._connect_timeout = self._make_spin(1, 300, 'секунд')
        self._read_timeout = self._make_spin(1, 600, 'секунд')
        self._max_attempts = self._make_spin(1, 10, 'спроб')
        self._connect_timeout.setValue(10)
        self._read_timeout.setValue(30)
        self._max_attempts.setValue(3)
        network_form.addRow('Таймаут підключення', self._connect_timeout)
        network_form.addRow('Таймаут читання', self._read_timeout)
        network_form.addRow('Максимум спроб', self._max_attempts)
        self._advanced_groups.append(network)
        root.addWidget(network)

        logging_box = QGroupBox('Журналювання')
        logging_form = QFormLayout(logging_box)
        logging_form.setContentsMargins(18, 22, 18, 18)
        logging_form.setHorizontalSpacing(18)
        logging_form.setVerticalSpacing(14)
        self._log_level = QComboBox()
        for label, value in (
            ('Інформація (INFO)', 'INFO'),
            ('Попередження (WARNING)', 'WARNING'),
            ('Помилки (ERROR)', 'ERROR'),
            ('Налагодження (DEBUG)', 'DEBUG'),
        ):
            self._log_level.addItem(label, value)
        logging_form.addRow('Рівень журналу', self._log_level)
        self._advanced_groups.append(logging_box)
        root.addWidget(logging_box)

        root.addStretch(1)
        footer = QHBoxLayout()
        self._status = QLabel('Зміни набудуть чинності після збереження.')
        self._status.setObjectName('label_hint')
        footer.addWidget(self._status, 1)
        defaults = QPushButton('Типові значення')
        defaults.setMinimumHeight(38)
        defaults.clicked.connect(self._restore_defaults)
        footer.addWidget(defaults)
        save = QPushButton('Зберегти налаштування')
        save.setObjectName('btn_primary')
        save.setMinimumHeight(38)
        save.clicked.connect(self._save)
        footer.addWidget(save)
        root.addLayout(footer)

    @staticmethod
    def _make_spin(minimum: int, maximum: int, suffix: str) -> QSpinBox:
        spin = QSpinBox()
        spin.setRange(minimum, maximum)
        spin.setSuffix(f' {suffix}')
        spin.setMinimumHeight(36)
        return spin

    def set_settings(self, settings: AppSettings) -> None:
        self._ui_mode = settings.ui_mode
        self._language = settings.language
        self._connect_timeout.setValue(settings.request_timeout_connect)
        self._read_timeout.setValue(settings.request_timeout_read)
        self._max_attempts.setValue(settings.max_retry_attempts)
        self._select_data(self._theme, settings.theme)
        self._select_data(self._log_level, settings.log_level)
        self._documents_dir.setText(
            settings.documents_dir or str(self._default_documents_dir)
        )

    def set_user_mode(self, enabled: bool) -> None:
        """Hide technical tuning in user mode without discarding its values."""
        self._ui_mode = 'user' if enabled else 'developer'
        for group in self._advanced_groups:
            group.setVisible(not enabled)
        self._intro.setText(
            'Налаштуйте тему та папку, куди зберігати документи.'
            if enabled else
            'Змініть вигляд, папку документів, мережеві параметри та журналювання.'
        )

    @staticmethod
    def _select_data(combo: QComboBox, value: str) -> None:
        index = combo.findData(value)
        combo.setCurrentIndex(index if index >= 0 else 0)

    def _choose_documents_dir(self) -> None:
        current = Path(self._documents_dir.text()).expanduser()
        start = current if current.is_dir() else self._default_documents_dir
        selected = QFileDialog.getExistingDirectory(
            self, 'Оберіть папку документів', str(start)
        )
        if selected:
            self._documents_dir.setText(selected)

    def _restore_defaults(self) -> None:
        self.set_settings(AppSettings(
            documents_dir=str(self._default_documents_dir),
            ui_mode=getattr(self, '_ui_mode', 'user'),
            language=getattr(self, '_language', 'uk'),
        ))
        self._status.setText('Типові значення встановлено; натисніть «Зберегти налаштування».')

    def _save(self) -> None:
        raw_path = self._documents_dir.text().strip()
        if not raw_path:
            QMessageBox.warning(self, 'Перевірте папку', 'Вкажіть папку для документів.')
            return
        documents_dir = Path(raw_path).expanduser().resolve()
        settings = AppSettings(
            request_timeout_connect=self._connect_timeout.value(),
            request_timeout_read=self._read_timeout.value(),
            max_retry_attempts=self._max_attempts.value(),
            log_level=str(self._log_level.currentData()),
            theme=str(self._theme.currentData()),
            documents_dir=str(documents_dir),
            ui_mode=getattr(self, '_ui_mode', 'user'),
            language=getattr(self, '_language', 'uk'),
        )
        self.settings_saved.emit(settings)
