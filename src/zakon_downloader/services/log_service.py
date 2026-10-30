from __future__ import annotations
import logging
import os
import tempfile
from datetime import datetime
from pathlib import Path

log = logging.getLogger('zakon_downloader.log_service')


class LogService:
    def __init__(self, logs_dir: Path) -> None:
        self._dir = logs_dir
        self._dir.mkdir(parents=True, exist_ok=True)

    def list_log_files(self) -> list[Path]:
        files = sorted(
            self._dir.glob('*.log'),
            key=lambda p: p.stat().st_mtime,
            reverse=True,
        )
        return files

    def read_log(self, path: Path, max_bytes: int = 512_000) -> str:
        if not path.exists():
            return ''
        size = path.stat().st_size
        if size > max_bytes:
            with path.open('r', encoding='utf-8', errors='replace') as f:
                f.seek(size - max_bytes)
                return f'[... показано останні {max_bytes // 1024} КБ ...]\n' + f.read()
        return path.read_text(encoding='utf-8', errors='replace')

    def export_combined(self, out_path: Path) -> Path:
        files = self.list_log_files()
        if not files:
            raise FileNotFoundError('Немає файлів журналів для експорту.')
        sep = '=' * 72
        parts: list[str] = [
            f'Зведений журнал ZakonDownloader',
            f'Сформовано: {datetime.now().strftime("%d.%m.%Y %H:%M:%S")}',
            f'Файлів: {len(files)}',
            sep,
            '',
        ]
        for fp in files:
            parts.append(f'=== {fp.name} ===')
            parts.append(fp.read_text(encoding='utf-8', errors='replace'))
            parts.append('')
        text = '\n'.join(parts)
        fd, tmp = tempfile.mkstemp(dir=out_path.parent, suffix='.tmp')
        try:
            with os.fdopen(fd, 'w', encoding='utf-8') as fh:
                fh.write(text)
            os.replace(tmp, out_path)
        except Exception:
            try:
                os.unlink(tmp)
            except OSError:
                pass
            raise
        log.info('Зведений журнал збережено: %s', out_path)
        return out_path