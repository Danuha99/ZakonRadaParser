from zakon_downloader.utils.transliteration import transliterate

def test_basic():         assert transliterate('привіт') == 'pryvit'
def test_uppercase():     assert transliterate('ЗАКОН') == 'zakon'
def test_zh():            assert transliterate('захист') == 'zakhyst'
def test_shch():          assert transliterate('щодо') == 'shchodo'
def test_ie():            assert transliterate('єдиний') == 'iedynyi'
def test_apostrophe():    assert "'" not in transliterate("пов'язаний")
def test_latin_passthru(): assert transliterate('abc') == 'abc'
def test_digits():        assert transliterate('2024') == '2024'