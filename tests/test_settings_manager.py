import json

from zakon_downloader.config.settings import SettingsManager
from zakon_downloader.models.app_settings import AppSettings


def test_load_defaults_when_file_missing(tmp_path):
    settings = SettingsManager(tmp_path / 'config.json').load()
    assert settings == AppSettings()
    assert settings.language == 'uk'


def test_save_and_load_all_settings(tmp_path):
    manager = SettingsManager(tmp_path / 'config.json')
    expected = AppSettings(
        request_timeout_connect=15,
        request_timeout_read=45,
        max_retry_attempts=4,
        log_level='DEBUG',
        theme='dark',
        documents_dir=str(tmp_path / 'Документи'),
        ui_mode='developer',
        language='fr',
    )
    manager.save(expected)
    assert manager.load() == expected


def test_load_legacy_settings_without_documents_dir_or_language(tmp_path):
    path = tmp_path / 'config.json'
    path.write_text('{"theme":"dark","max_retry_attempts":2}', encoding='utf-8')
    settings = SettingsManager(path).load()
    assert settings.theme == 'dark'
    assert settings.max_retry_attempts == 2
    assert settings.documents_dir == ''
    assert settings.ui_mode == 'user'
    assert settings.language == 'uk'


def test_load_ignores_unknown_fields_and_bounds_values(tmp_path):
    path = tmp_path / 'config.json'
    path.write_text(json.dumps({
        'request_timeout_connect': 0,
        'request_timeout_read': 9999,
        'max_retry_attempts': 99,
        'ui_mode': 'admin',
        'language': 'not-a-locale',
        'not_a_setting': 'ignored',
    }), encoding='utf-8')
    settings = SettingsManager(path).load()
    assert settings.request_timeout_connect == 1
    assert settings.request_timeout_read == 600
    assert settings.max_retry_attempts == 10
    assert settings.ui_mode == 'user'
    assert settings.language == 'uk'


def test_invalid_json_returns_defaults(tmp_path):
    path = tmp_path / 'config.json'
    path.write_text('{broken', encoding='utf-8')
    assert SettingsManager(path).load() == AppSettings()
