from __future__ import annotations
import re

def normalize_whitespace(text: str) -> str:
    text = re.sub(r"[ \t]+", " ", text)
    text = re.sub(r"\n{3,}", "\n\n", text)
    return text.strip()

def safe_filename_part(text: str, max_len: int = 50) -> str:
    text = re.sub(r'[\\/:*?"<>|]', "_", text).strip(". ")
    return text[:max_len]
