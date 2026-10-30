from __future__ import annotations
import logging
import os
import tempfile
from datetime import datetime
from pathlib import Path

log = logging.getLogger('zakon_downloader.log_export_service')


class LogExportService:
    def __init__(self, logs_dir: Path, export_dir: Path) -> None:
        self._logs_dir   = logs_dir
        self._export_dir = export_dir
        self._export_dir.mkdir(parents=True, exist_ok=True)

    def list_log_files(self) -> list[Path]:
        return sorted(
            self._logs_dir.glob('*.log'),
            key=lambda p: p.stat().st_mtime,
        )

    def read_log(self, path: Path, max_bytes: int = 512_000) -> str:
        if not path.exists():
            return ''
        size = path.stat().st_size
        if size > max_bytes:
            with path.open('r', encoding='utf-8', errors='replace') as f:
                f.seek(size - max_bytes)
                return '[... показано останні ' + str(max_bytes // 1024) + ' КБ ...]\n' + f.read()
        return path.read_text(encoding='utf-8', errors='replace')

    def export_combined(self) -> Path:
        files = self.list_log_files()
        if not files:
            raise FileNotFoundError('Немає файлів журналів для експорту.')
        now = datetime.now()
        filename = 'combined_log_' + now.strftime('%d.%m.%Y_%H-%M-%S') + '.txt'
        out_path = self._export_dir / filename
        sep = '=' * 72
        parts: list[str] = [
            'Зведений журнал ZakonDownloader',
            'Сформовано: ' + now.strftime('%d.%m.%Y %H:%M:%S'),
            'Файлів: ' + str(len(files)),
            sep,
            '',
        ]
        for fp in files:
            parts.append('=== ' + fp.name + ' ===')
            parts.append(fp.read_text(encoding='utf-8', errors='replace'))
            parts.append('')
        text = '\n'.join(parts)
        self._safe_write(out_path, text)
        log.info('Зведений журнал збережено: %s', out_path)
        return out_path

    @staticmethod
    def _safe_write(path: Path, text: str) -> None:
        fd, tmp = tempfile.mkstemp(dir=path.parent, suffix='.tmp')
        try:
            with os.fdopen(fd, 'w', encoding='utf-8') as fh:
                fh.write(text)
            os.replace(tmp, path)
        except Exception:
            try:
                os.unlink(tmp)
            except OSError:
                pass
            raise