from datetime import date
from zakon_downloader.services.filename_service import build_filename, _make_stem

D = date(2024, 1, 1)

def test_ends_with_txt():
    assert build_filename('Закон України', D).endswith('.txt')

def test_contains_date():
    assert '01.01.2024' in build_filename('Закон України', D)

def test_lowercase():
    assert build_filename('Закон України', D) == build_filename('Закон України', D).lower()

def test_no_spaces():
    assert ' ' not in build_filename('Закон України', D)

def test_no_forbidden_chars():
    import re
    assert not re.search(r'[\\/:*?\"<>|]', build_filename('Закон: тест?', D))

def test_max_length():
    long_title = 'Закон ' * 60
    name = build_filename(long_title, D)
    assert len(name.replace('.txt', '')) <= 180

def test_real_example():
    name = build_filename('Про захист персональних даних', D)
    assert name == 'pro_zakhyst_personalnykh_danykh_01.01.2024.txt'

def test_apostrophe_removed():
    name = build_filename("Про пов'язані питання", D)
    assert "'" not in name