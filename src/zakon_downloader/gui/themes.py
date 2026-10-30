from __future__ import annotations

from string import Template

ACCENT = '#F0B400'
ACCENT_HOVER = '#FFD000'
ACCENT_PRESS = '#C89500'
ACCENT_TEXT = '#1A1A1A'

LIGHT = {
    'bg': '#F4F3EF', 'surface': '#FFFFFF', 'surface2': '#F9F8F5',
    'border': '#E4E3DE', 'text': '#1A1A1A', 'text_muted': '#6B6B6B',
    'accent': ACCENT, 'accent_hover': ACCENT_HOVER, 'accent_press': ACCENT_PRESS,
    'accent_text': ACCENT_TEXT, 'success': '#2D7A4F', 'success_bg': '#EDF7F1',
    'error': '#C0392B', 'error_bg': '#FDECEA', 'warning': '#8A6000',
    'warning_bg': '#FEF8E7', 'shadow': 'rgba(0,0,0,0.07)',
    'shadow2': 'rgba(0,0,0,0.13)', 'input_bg': '#FFFFFF', 'input_focus': '#F0B400',
    'table_alt': '#F9F8F5', 'table_sel': '#FEF3CC', 'scrollbar': '#D4D3CE',
}

DARK = {
    'bg': '#111214', 'surface': '#1C1E22', 'surface2': '#22252A',
    'border': '#2E3138', 'text': '#EEECEA', 'text_muted': '#888890',
    'accent': ACCENT, 'accent_hover': ACCENT_HOVER, 'accent_press': ACCENT_PRESS,
    'accent_text': ACCENT_TEXT, 'success': '#3DBE75', 'success_bg': '#0D2B1A',
    'error': '#E05C4B', 'error_bg': '#2C0E0A', 'warning': '#F0C040',
    'warning_bg': '#2A2000', 'shadow': 'rgba(0,0,0,0.35)',
    'shadow2': 'rgba(0,0,0,0.55)', 'input_bg': '#22252A', 'input_focus': '#F0B400',
    'table_alt': '#22252A', 'table_sel': '#2E2500', 'scrollbar': '#3A3D44',
}

_QSS_TEMPLATE = Template('''
QWidget { background:$bg; color:$text; font-family:"Segoe UI",Inter,Arial,sans-serif; font-size:13px; border:none; outline:none; }
QMainWindow, QDialog { background:$bg; }
QTabWidget::pane { background:$surface; border:1px solid $border; border-radius:12px; margin-top:-1px; }
QTabBar::tab { background:$surface2; color:$muted; border:1px solid $border; border-bottom:none; padding:9px 22px; margin-right:3px; border-radius:8px 8px 0 0; font-weight:500; }
QTabBar::tab:selected { background:$surface; color:$text; border-bottom:2px solid $accent; font-weight:600; }
QTabBar::tab:hover:!selected { color:$text; background:$surface; }
QPushButton { background:$surface; color:$text; border:1.5px solid $border; border-radius:8px; padding:7px 18px; font-weight:500; }
QPushButton:hover { background:$surface2; border-color:$accent; }
QPushButton:pressed { background:$table_sel; border-color:$accent_press; }
QPushButton:disabled { color:$muted; border-color:$border; background:$surface2; }
QPushButton#btn_primary { background:$accent; color:$accent_text; border:none; font-weight:700; font-size:14px; padding:9px 28px; border-radius:10px; }
QPushButton#btn_primary:hover { background:$accent_hover; }
QPushButton#btn_primary:pressed { background:$accent_press; }
QPushButton#btn_primary:disabled { background:$border; color:$muted; }
QLineEdit { background:$input_bg; color:$text; border:1.5px solid $border; border-radius:8px; padding:8px 12px; selection-background-color:$accent; selection-color:$accent_text; }
QLineEdit:focus { border-color:$input_focus; background:$surface; }
QLineEdit:disabled { color:$muted; background:$surface2; }
QComboBox, QSpinBox, QDateEdit { background:$input_bg; color:$text; border:1px solid $border; border-radius:7px; padding:6px 10px; }
QComboBox:focus, QSpinBox:focus, QDateEdit:focus { border-color:$input_focus; }
QComboBox::drop-down { border:none; width:24px; }
QGroupBox { border:1px solid $border; border-radius:11px; margin-top:10px; padding:8px; font-weight:600; }
QGroupBox::title { subcontrol-origin:margin; left:12px; padding:0 6px; color:$muted; }
QCheckBox { spacing:8px; color:$text; }
QCheckBox::indicator { width:16px; height:16px; border:1px solid $border; border-radius:4px; background:$input_bg; }
QCheckBox::indicator:checked { background:$accent; border-color:$accent; }
QTextEdit { background:$surface2; color:$text; border:1.5px solid $border; border-radius:10px; padding:10px 14px; selection-background-color:$accent; }
QProgressBar { background:$surface2; border:1.5px solid $border; border-radius:6px; height:8px; color:transparent; }
QProgressBar::chunk { background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 $accent,stop:1 $accent_hover); border-radius:5px; }
QTableView { background:$surface; alternate-background-color:$table_alt; gridline-color:$border; border:1.5px solid $border; border-radius:10px; selection-background-color:$table_sel; selection-color:$text; }
QHeaderView::section { background:$surface2; color:$muted; border:none; border-bottom:1.5px solid $border; border-right:1px solid $border; padding:8px 12px; font-weight:600; font-size:12px; }
QListWidget { background:$surface; border:1.5px solid $border; border-radius:10px; padding:4px; }
QListWidget::item { padding:8px 12px; border-radius:6px; margin:2px 0; }
QListWidget::item:selected { background:$table_sel; color:$text; border-left:3px solid $accent; }
QListWidget::item:hover:!selected { background:$surface2; }
QLabel#label_title { font-size:20px; font-weight:700; color:$text; }
QLabel#label_section { font-size:11px; font-weight:600; color:$muted; letter-spacing:0.08em; }
QStatusBar { background:$surface2; color:$muted; border-top:1px solid $border; font-size:12px; padding:2px 8px; }
QSplitter::handle { background:$border; width:1px; margin:8px 6px; }
QScrollBar:vertical { background:transparent; width:8px; margin:0; }
QScrollBar::handle:vertical { background:$scrollbar; border-radius:4px; min-height:30px; }
QScrollBar::handle:vertical:hover { background:$muted; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
QScrollBar:horizontal { background:transparent; height:8px; }
QScrollBar::handle:horizontal { background:$scrollbar; border-radius:4px; min-width:30px; }
QMessageBox { background:$surface; }
QToolTip { background:$surface; color:$text; border:1px solid $border; border-radius:6px; padding:5px 10px; font-size:12px; }
'''.strip())


def build_qss(theme: dict[str, str]) -> str:
    """Build the application stylesheet from the selected color palette."""
    return _QSS_TEMPLATE.substitute(
        bg=theme['bg'],
        surface=theme['surface'],
        surface2=theme['surface2'],
        border=theme['border'],
        text=theme['text'],
        muted=theme['text_muted'],
        accent=theme['accent'],
        accent_hover=theme['accent_hover'],
        accent_press=theme['accent_press'],
        accent_text=theme['accent_text'],
        input_bg=theme['input_bg'],
        input_focus=theme['input_focus'],
        table_alt=theme['table_alt'],
        table_sel=theme['table_sel'],
        scrollbar=theme['scrollbar'],
    )


# Sidebar/dashboard components are styled separately so the original palette and
# all existing tab controls retain their exact colors in both themes.
_QSS_TEMPLATE = Template(_QSS_TEMPLATE.template + '''

QFrame#sidebar { background:$surface; border-right:1px solid $border; }
QWidget#content_area { background:$bg; }
QLabel#sidebar_brand_icon { background:$accent; color:$accent_text; border-radius:11px; font-size:21px; font-weight:800; }
QLabel#brand_name { color:$text; font-size:15px; font-weight:700; }
QLabel#sidebar_section_label, QLabel#hero_eyebrow, QLabel#metric_caption { color:$muted; font-size:10px; font-weight:700; letter-spacing:1px; }
QLabel#sidebar_note, QLabel#dashboard_note { color:$muted; font-size:11px; }
QPushButton#nav_button { text-align:left; background:transparent; color:$muted; border:1px solid transparent; border-radius:8px; padding:9px 11px; font-weight:500; }
QPushButton#nav_button:hover { color:$text; background:$surface2; border-color:$border; }
QPushButton#nav_button[active="true"] { background:$accent; color:$accent_text; border-color:$accent; font-weight:700; }
QPushButton#nav_button[active="true"]:hover { background:$accent_hover; }
QPushButton#theme_toggle { background:$surface; color:$text; border:1px solid $border; border-radius:8px; padding:6px 12px; font-weight:600; }
QPushButton#theme_toggle:hover { border-color:$accent; background:$surface2; }
QComboBox#mode_selector { background:$surface; color:$text; min-width:120px; border:1px solid $border; border-radius:8px; padding:6px 11px; font-weight:600; }
QComboBox#mode_selector:focus { border-color:$accent; }
QLabel#page_title { color:$text; font-size:24px; font-weight:700; }
QLabel#page_subtitle { color:$muted; font-size:12px; }
QFrame#overview_hero { background:$surface; border:1px solid $border; border-left:4px solid $accent; border-radius:13px; }
QLabel#hero_heading { color:$text; font-size:23px; font-weight:700; }
QLabel#hero_body { color:$muted; font-size:12px; }
QFrame#metric_card, QFrame#metric_featured, QFrame#dashboard_card { background:$surface; border:1px solid $border; border-radius:11px; }
QFrame#metric_featured { border-top:3px solid $accent; }
QLabel#metric_value { color:$text; font-size:27px; font-weight:700; }
QLabel#card_title { color:$text; font-size:15px; font-weight:700; }
QPushButton#text_button { background:transparent; color:$muted; border:none; padding:4px 7px; font-weight:600; }
QPushButton#text_button:hover { color:$text; }
QTableWidget#recent_table { background:$surface; alternate-background-color:$table_alt; color:$text; border:none; gridline-color:$border; selection-background-color:$table_sel; selection-color:$text; }
QTableWidget#recent_table QHeaderView::section { background:$surface2; color:$muted; border:none; border-bottom:1px solid $border; padding:7px 9px; font-size:11px; font-weight:600; }
QTableWidget#recent_table::item { padding:5px 8px; border-bottom:1px solid $border; }
QLabel#label_hint, QLabel#page_subtitle { color:$muted; }
QLabel#sidebar_section_label, QLabel#sidebar_note, QLabel#page_title, QLabel#page_subtitle,
QLabel#hero_eyebrow, QLabel#hero_heading, QLabel#hero_body, QLabel#metric_caption,
QLabel#metric_value, QLabel#card_title, QLabel#dashboard_note, QLabel#label_hint { background-color:transparent; }
''')
