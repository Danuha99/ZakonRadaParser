"""Тести для retry_service.py."""
from __future__ import annotations
from unittest.mock import MagicMock
import httpx
from zakon_downloader.services.http_client import ForbiddenDomainError, HttpClientError
from zakon_downloader.services.retry_service import FetchResult, RetryService

URL = "https://zakon.rada.gov.ua/laws/show/2297-17"

def _client(*items, text="Текст документа"):
    c = MagicMock()
    vals = []
    for s in items:
        if isinstance(s, Exception):
            vals.append(s)
        else:
            r = MagicMock(spec=httpx.Response)
            r.status_code = s; r.text = text
            vals.append(r)
    def side(url):
        v = vals.pop(0)
        if isinstance(v, Exception): raise v
        return v
    c.get.side_effect = side
    return c

def _svc(client, **kw):
    sleep = MagicMock()
    return RetryService(client, max_attempts=3, delays=(0,0,0), sleep_fn=sleep, **kw), sleep

def test_success_first():
    r, s = _svc(_client(200))
    res = r.fetch(URL)
    assert res.success and res.attempts == 1
    s.assert_not_called()

def test_success_after_retries():
    r, _ = _svc(_client(503, 503, 200))
    assert r.fetch(URL).success

def test_all_exhausted():
    r, _ = _svc(_client(503, 503, 503))
    res = r.fetch(URL)
    assert not res.success and "3 спроб" in res.error

def test_403_no_retry():
    r, s = _svc(_client(403))
    res = r.fetch(URL)
    assert not res.success and res.attempts == 1 and "403" in res.error
    s.assert_not_called()

def test_404_no_retry():
    r, _ = _svc(_client(404))
    assert not r.fetch(URL).success

def test_429_no_retry():
    r, sleep = _svc(_client(429, 200))
    res = r.fetch(URL)
    assert not res.success and res.attempts == 1
    sleep.assert_not_called()

def test_network_error_retries():
    r, _ = _svc(_client(HttpClientError("t"), HttpClientError("t"), 200))
    assert r.fetch(URL).success

def test_forbidden_domain_stops():
    c = MagicMock()
    c.get.side_effect = ForbiddenDomainError("bad")
    r, s = _svc(c)
    res = r.fetch(URL)
    assert not res.success and res.attempts == 1
    s.assert_not_called()

def test_captcha_detected():
    r, _ = _svc(_client(200, text="captcha challenge"))
    res = r.fetch(URL)
    assert not res.success and "CAPTCHA" in res.error

def test_sleep_called():
    sleep = MagicMock()
    svc = RetryService(_client(503, 503, 200), max_attempts=3, delays=(2,5,10), sleep_fn=sleep)
    svc.fetch(URL)
    assert sleep.call_count == 2
    sleep.assert_any_call(2); sleep.assert_any_call(5)

def test_is_captcha_false():
    r = MagicMock(spec=httpx.Response)
    r.text = "Про захист"
    assert not FetchResult(success=True, response=r, status_code=200, attempts=1).is_captcha

def test_with_operation_id():
    r, _ = _svc(_client(200))
    assert r.fetch(URL, operation_id="op-123").success
