from __future__ import annotations
import re
from urllib.parse import urlparse

_ALLOWED_DOMAIN = 'zakon.rada.gov.ua'
_PATH_RE = re.compile(r'^/laws/show/.+', re.IGNORECASE)


class UrlValidator:
    def validate(self, url: str) -> tuple[bool, str]:
        if not url or not url.strip():
            return False, 'URL не може бути порожнім.'

        # Відкидаємо fragment (#Text) перед перевіркою [1]
        clean_url = url.split('#')[0].strip()

        try:
            parsed = urlparse(clean_url)
        except Exception:
            return False, 'Не вдалося розібрати URL.'

        if parsed.scheme != 'https':
            return False, 'URL повинен починатися з https://.'

        if parsed.netloc != _ALLOWED_DOMAIN:
            return False, (
                f'Дозволені лише посилання з {_ALLOWED_DOMAIN}. '
                f'Отримано: {parsed.netloc}'
            )

        if not _PATH_RE.match(parsed.path):
            return False, (
                'Посилання повинно вести на документ: '
                '/laws/show/<ідентифікатор>.'
            )

        return True, ''