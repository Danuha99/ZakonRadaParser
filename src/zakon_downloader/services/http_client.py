"""
http_client.py — HTTP-клієнт для zakon.rada.gov.ua [1].
Таймаут: 10с connect / 30с read.
НЕ обходить CAPTCHA, авторизацію чи антибот-захист [1].
"""
from __future__ import annotations
import logging
from typing import Any
import httpx
from zakon_downloader.config.constants import (
    ALLOWED_URL_PREFIX, DEFAULT_HEADERS,
    REQUEST_TIMEOUT_CONNECT, REQUEST_TIMEOUT_READ,
)

log = logging.getLogger("zakon_downloader.http_client")


class ForbiddenDomainError(Exception):
    """URL не належить до дозволеного домену."""

class HttpClientError(Exception):
    """Загальна помилка HTTP-клієнта."""

    def __init__(self, message: str, status_code: int | None = None) -> None:
        super().__init__(message)
        self.status_code = status_code


class HttpClient:
    def __init__(
        self,
        extra_headers: dict[str, str] | None = None,
        timeout_connect: float = REQUEST_TIMEOUT_CONNECT,
        timeout_read: float = REQUEST_TIMEOUT_READ,
    ) -> None:
        headers = {**DEFAULT_HEADERS, **(extra_headers or {})}
        self._client = httpx.Client(
            headers=headers,
            timeout=httpx.Timeout(
                connect=timeout_connect,
                read=timeout_read,
                write=10.0, pool=10.0,
            ),
            follow_redirects=True,
        )

    def __enter__(self) -> "HttpClient":
        return self

    def __exit__(self, *args: Any) -> None:
        self.close()

    def close(self) -> None:
        self._client.close()

    def get(self, url: str) -> httpx.Response:
        self._check_domain(url)
        log.debug("GET %s", url)
        try:
            response = self._client.get(url)
            log.debug("Відповідь %s → %d", url, response.status_code)
            return response
        except httpx.TimeoutException as exc:
            raise HttpClientError(f"Таймаут при зверненні до {url}: {exc}") from exc
        except httpx.NetworkError as exc:
            raise HttpClientError(f"Мережева помилка при зверненні до {url}: {exc}") from exc
        except httpx.HTTPError as exc:
            raise HttpClientError(f"HTTP-помилка при зверненні до {url}: {exc}") from exc

    @staticmethod
    def _check_domain(url: str) -> None:
        if not url.startswith(ALLOWED_URL_PREFIX):
            raise ForbiddenDomainError(
                f"URL не належить до дозволеного домену ({ALLOWED_URL_PREFIX}): {url}"
            )
