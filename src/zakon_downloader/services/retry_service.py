"""
retry_service.py — Повторні спроби з backoff [1].
Паузи: 2с → 5с → 10с.
Без повтору: 400, 401, 403, 404, 429, CAPTCHA [1].
"""
from __future__ import annotations
import logging
import time
from dataclasses import dataclass
import httpx
from zakon_downloader.config.constants import MAX_RETRY_ATTEMPTS, RETRY_DELAYS
from zakon_downloader.services.http_client import ForbiddenDomainError, HttpClient, HttpClientError

log = logging.getLogger("zakon_downloader.retry_service")

_NO_RETRY_CODES = frozenset({400, 401, 403, 404, 429})


@dataclass
class FetchResult:
    success: bool
    response: httpx.Response | None = None
    error: str = ""
    attempts: int = 0
    status_code: int | None = None

    @property
    def is_captcha(self) -> bool:
        if self.response is None:
            return False
        return any(kw in self.response.text.lower()
                   for kw in ("captcha", "recaptcha", "challenge"))


class RetryService:
    def __init__(self, client: HttpClient,
                 max_attempts: int = MAX_RETRY_ATTEMPTS,
                 delays: tuple[int, ...] = RETRY_DELAYS,
                 sleep_fn=time.sleep) -> None:
        self._client = client
        self._max    = max_attempts
        self._delays = delays
        self._sleep  = sleep_fn

    def fetch(self, url: str, operation_id: str = "") -> FetchResult:
        prefix     = f"[{operation_id}] " if operation_id else ""
        last_error = ""

        for attempt in range(1, self._max + 1):
            log.info("%sСпроба %d/%d → %s", prefix, attempt, self._max, url)
            try:
                response = self._client.get(url)
            except ForbiddenDomainError as exc:
                return FetchResult(success=False, error=str(exc), attempts=attempt)
            except HttpClientError as exc:
                last_error = str(exc)
                log.warning("%sМережева помилка (спроба %d): %s", prefix, attempt, exc)
                self._maybe_sleep(attempt)
                continue

            code = response.status_code

            if code == 200:
                result = FetchResult(success=True, response=response,
                                     attempts=attempt, status_code=code)
                if result.is_captcha:
                    log.warning("%sВиявлено CAPTCHA — зупиняємось [1].", prefix)
                    return FetchResult(
                        success=False, response=response,
                        error="Сервер повернув сторінку з CAPTCHA. "
                              "Програма не обходить такі обмеження [1].",
                        attempts=attempt, status_code=code,
                    )
                return result

            if code in _NO_RETRY_CODES:
                msg = self._describe_no_retry(code)
                log.warning("%s%s", prefix, msg)
                return FetchResult(success=False, response=response,
                                   error=msg, attempts=attempt, status_code=code)

            last_error = f"HTTP {code}"
            log.warning("%sHTTP %d — повторюємо (%d/%d).", prefix, code, attempt, self._max)
            self._maybe_sleep(attempt)

        return FetchResult(
            success=False,
            error=f"Не вдалося завантажити після {self._max} спроб. Остання помилка: {last_error}",
            attempts=self._max,
        )

    def _maybe_sleep(self, attempt: int) -> None:
        if attempt < self._max:
            delay = self._delays[min(attempt - 1, len(self._delays) - 1)]
            log.info("Пауза %dс…", delay)
            self._sleep(delay)

    @staticmethod
    def _describe_no_retry(code: int) -> str:
        return {
            403: "Сервер заборонив доступ (HTTP 403 Forbidden). "
                 "Програма не обходить захист ресурсу [1].",
            404: "Документ не знайдено (HTTP 404 Not Found).",
            400: "Некоректний запит (HTTP 400 Bad Request).",
            401: "Потрібна авторизація (HTTP 401). Програма не підтримує авторизований доступ.",
            429: "Сервер обмежив частоту запитів (HTTP 429). Повторні спроби не виконуються.",
        }.get(code, f"HTTP {code} — повтор недоцільний.")
