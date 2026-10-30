"""Константи застосунку ZakonDownloader."""

APP_NAME    = "ZakonDownloader"
APP_VERSION = "1.0.0"

BASE_DOMAIN        = "zakon.rada.gov.ua"
ALLOWED_URL_PREFIX = f"https://{BASE_DOMAIN}/"

REQUEST_TIMEOUT_CONNECT = 10
REQUEST_TIMEOUT_READ    = 30
MAX_RETRY_ATTEMPTS      = 3
RETRY_DELAYS            = (2, 5, 10)

ENCODING                = "UTF-8"
LOG_DATE_FORMAT         = "%d.%m.%Y_%H-%M-%S"
DISPLAY_DATE_FORMAT     = "%d.%m.%Y"
DISPLAY_DATETIME_FORMAT = "%d.%m.%Y %H:%M:%S"

LOG_LEVELS = ("DEBUG", "INFO", "WARNING", "ERROR")

DEFAULT_HEADERS = {
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
        "AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/124.0.0.0 Safari/537.36"
    ),
    "Accept-Language": "uk-UA,uk;q=0.9,en;q=0.8",
    "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
}
