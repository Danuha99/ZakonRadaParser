from __future__ import annotations

import os
from pathlib import Path

from PySide6.QtCore import QUrl, Qt
from PySide6.QtGui import QDesktopServices
from PySide6.QtWidgets import (
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QVBoxLayout,
    QWidget,
)


class AboutTab(QWidget):
    def __init__(
        self,
        version: str,
        data_dir: Path,
        documents_dir: Path,
        parent=None,
    ) -> None:
        super().__init__(parent)
        self._data_dir = data_dir
        self._documents_dir = documents_dir
        self._build_ui(version)

    def _build_ui(self, version: str) -> None:
        root = QVBoxLayout(self)
        root.setContentsMargins(32, 30, 32, 28)
        root.setSpacing(18)

        title = QLabel('ZakonDownloader')
        title.setObjectName('label_title')
        root.addWidget(title)
        subtitle = QLabel(f'Версія {version} · Windows 10/11')
        subtitle.setObjectName('label_hint')
        root.addWidget(subtitle)

        about = QGroupBox('Призначення')
        about_layout = QVBoxLayout(about)
        about_layout.setContentsMargins(18, 22, 18, 18)
        description = QLabel(
            'Програма завантажує публічно доступний текст нормативного документа '
            'з zakon.rada.gov.ua, зберігає його у UTF-8 TXT та веде локальну історію '
            'операцій і технічні журнали.'
        )
        description.setWordWrap(True)
        description.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse)
        about_layout.addWidget(description)
        root.addWidget(about)

        safety = QGroupBox('Безпека та межі роботи')
        safety_layout = QVBoxLayout(safety)
        safety_layout.setContentsMargins(18, 22, 18, 18)
        safety_text = QLabel(
            'Застосунок працює лише з відкритими сторінками. Він не обходить CAPTCHA, '
            'авторизацію, антибот-захист або обмеження доступу. Не вводьте в програму '
            'паролі, токени чи API-ключі.'
        )
        safety_text.setWordWrap(True)
        safety_layout.addWidget(safety_text)
        root.addWidget(safety)

        locations = QGroupBox('Розташування даних')
        locations_layout = QVBoxLayout(locations)
        locations_layout.setContentsMargins(18, 22, 18, 18)
        self._data_label = QLabel()
        self._data_label.setObjectName('label_hint')
        self._data_label.setWordWrap(True)
        locations_layout.addWidget(self._data_label)
        self._documents_label = QLabel()
        self._documents_label.setObjectName('label_hint')
        self._documents_label.setWordWrap(True)
        locations_layout.addWidget(self._documents_label)
        root.addWidget(locations)

        actions = QHBoxLayout()
        open_data = QPushButton('Відкрити папку даних')
        open_data.setMinimumHeight(38)
        open_data.clicked.connect(self._open_data_dir)
        actions.addWidget(open_data)
        open_documents = QPushButton('Відкрити папку документів')
        open_documents.setMinimumHeight(38)
        open_documents.clicked.connect(self._open_documents_dir)
        actions.addWidget(open_documents)
        website = QPushButton('Сайт zakon.rada.gov.ua')
        website.setMinimumHeight(38)
        website.clicked.connect(
            lambda: QDesktopServices.openUrl(QUrl('https://zakon.rada.gov.ua/'))
        )
        actions.addWidget(website)
        actions.addStretch(1)
        root.addLayout(actions)
        root.addStretch(1)
        self.set_documents_dir(self._documents_dir)

    def set_documents_dir(self, documents_dir: Path) -> None:
        self._documents_dir = documents_dir
        self._data_label.setText(f'Дані та журнали: {self._data_dir}')
        self._documents_label.setText(f'Документи: {self._documents_dir}')

    def _open_data_dir(self) -> None:
        self._data_dir.mkdir(parents=True, exist_ok=True)
        os.startfile(str(self._data_dir))

    def _open_documents_dir(self) -> None:
        self._documents_dir.mkdir(parents=True, exist_ok=True)
        os.startfile(str(self._documents_dir))
