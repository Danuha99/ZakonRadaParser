from __future__ import annotations
from datetime import date, datetime
DFMT, DTFMT = "%d.%m.%Y", "%d.%m.%Y %H:%M:%S"

def format_date(d): return "" if d is None else d.strftime(DFMT)
def format_datetime(dt): return "" if dt is None else dt.strftime(DTFMT)
def parse_date(text):
    for fmt in (DFMT, "%Y-%m-%d"):
        try: return datetime.strptime(text.strip(), fmt).date()
        except: pass
    return None
