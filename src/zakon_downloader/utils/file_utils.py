from __future__ import annotations
import os
from pathlib import Path

def atomic_write(path: Path, content: str, encoding: str = "utf-8") -> None:
    if not content or not content.strip():
        raise ValueError("Вміст файла порожній — запис скасовано.")
    tmp = path.with_suffix(".tmp")
    tmp.write_text(content, encoding=encoding)
    os.replace(tmp, path)
