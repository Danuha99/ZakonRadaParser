"""Тести для http_client.py."""
from __future__ import annotations
from unittest.mock import MagicMock, patch
import httpx
import pytest
from zakon_downloader.services.http_client import ForbiddenDomainError, HttpClient, HttpClientError

VALID   = "https://zakon.rada.gov.ua/laws/show/2297-17"
INVALID = "https://google.com/laws/show/2297-17"

def test_forbidden_domain_raises():
    with pytest.raises(ForbiddenDomainError):
        HttpClient().get(INVALID)

def test_allowed_domain_ok():
    with patch("httpx.Client.get") as m:
        m.return_value = MagicMock(status_code=200, text="OK")
        assert HttpClient().get(VALID).status_code == 200

def test_timeout_raises():
    with patch("httpx.Client.get", side_effect=httpx.TimeoutException("t")):
        with pytest.raises(HttpClientError, match="Таймаут"):
            HttpClient().get(VALID)

def test_network_error_raises():
    with patch("httpx.Client.get", side_effect=httpx.NetworkError("n")):
        with pytest.raises(HttpClientError, match="Мережева"):
            HttpClient().get(VALID)

def test_context_manager():
    with patch("httpx.Client.get") as m:
        m.return_value = MagicMock(status_code=200)
        with HttpClient() as c:
            c.get(VALID)

def test_user_agent_header():
    from zakon_downloader.config.constants import DEFAULT_HEADERS
    assert "Mozilla" in DEFAULT_HEADERS["User-Agent"]
