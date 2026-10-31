# ZakonDownloader

[English](README.md) | [Українська](README.uk.md) | [Deutsch](README.de.md) | [Français](README.fr.md) | [Español](README.es.md) | [Português](README.pt.md) | [简体中文](README.zh-CN.md)

ZakonDownloader is an open-source Windows desktop application for downloading publicly available legal documents from the official website of the Verkhovna Rada of Ukraine, [`zakon.rada.gov.ua`](https://zakon.rada.gov.ua/). It provides both a Python/PySide6 application and a native C/Win32 application. Both versions use the same settings, document folder, and SQLite history.

> **Scope and safety:** The application works with public documents on `zakon.rada.gov.ua`. It does not bypass CAPTCHA, sign-in, anti-bot measures, or access restrictions. Do not enter passwords, API keys, or tokens.

## Features

- Download a document from its direct HTTPS link in a background operation.
- Save the result as a UTF-8 `.txt` file and keep a local history in SQLite.
- Browse the local document library and search or filter download history.
- Use light or dark appearance and switch between User and Developer interface modes.
- In the native C application, read saved documents in the built-in viewer, navigate a detected table of contents, search text, save bookmarks and resume reading, adjust text size, copy a source-aware citation, and export a book-style PDF.
- In the native C application, export history to CSV. The export uses UTF-8 with BOM and is protected against spreadsheet formula interpretation.

The downloader accepts public document URLs on `zakon.rada.gov.ua` in the `/laws/show/<id>` format. It validates HTTPS redirects to the same host, limits response size, and retries temporary failures; it is not a general-purpose website scraper. Downloaded legal texts remain in their source language—the interface language does not translate documents.

## GUI languages

The native C/Win32 GUI is available in all seven languages below. The Python/PySide6 GUI is currently available in Ukrainian only.

| Interface language | Native C/Win32 | Python/PySide6 |
|---|:---:|:---:|
| Ukrainian | Yes | Yes |
| English | Yes | — |
| German | Yes | — |
| French | Yes | — |
| Spanish | Yes | — |
| Portuguese | Yes | — |
| Simplified Chinese | Yes | — |

In the native C application, change the language in **Settings → Interface language**. The selection is saved in `settings.json`; it does not change the language of downloaded documents. The README variants linked above cover every GUI language.

## Requirements

- Windows 10 or Windows 11, 64-bit.
- Internet access to retrieve documents.
- Python 3.11 or newer and the Python Launcher (`py`) to run the Python version.
- Zig 0.17.0 with `zig cc` to build the native C version.

## Run the Python version

In PowerShell, clone the repository and run the following commands from its root:

```powershell
git clone https://github.com/Danuha99/ZakonRadaParser.git
Set-Location ZakonRadaParser
py -3 -m venv .venv
.\.venv\Scripts\python.exe -m pip install --upgrade pip
.\.venv\Scripts\python.exe -m pip install -e .
.\.venv\Scripts\python.exe -m zakon_downloader
```

You do not need to activate the virtual environment; invoke its Python executable as shown. Replace the Python version selector if you need a specific installed version, for example `py -3.11`.

### Build a standalone Python executable

Install the development dependencies, then run the packaging script from PowerShell:

```powershell
.\.venv\Scripts\python.exe -m pip install -e ".[dev]"
.\scripts\build_exe.ps1
```

The executable is written to `dist\ZakonDownloader.exe`.

## Build the native C/Win32 version

From the repository root in PowerShell:

```powershell
.\scripts\build_native_c.ps1
.\native_c\dist\ZakonDownloader.exe
```

The build requires Zig 0.17.0 (`zig cc`) in `PATH`. If Zig is installed elsewhere, provide its path:

```powershell
.\scripts\build_native_c.ps1 -ZigPath 'C:\Tools\zig\zig.exe'
```

The build script runs the native parser, localization, settings/history, and related checks before producing the 64-bit GUI executable. The output is `native_c\dist\ZakonDownloader.exe`.

## Local reader and exports

In the native C application, open **My Documents**, select a saved `.txt` file, and choose **Read in app**. The reader can show the clean legal text or the full saved file, find text, navigate detected sections, preserve bookmarks and reading position, and copy a quotation with its title and source URL. **Export PDF** creates a separate book-style PDF next to the text file; it requires the Windows **Microsoft Print to PDF** printer and does not modify the original `.txt` file.

CSV history exports are written to `%USERPROFILE%\ZakonDownloader\data\exported_logs\`. Existing documents and history entries are not removed by these exports.

## Data locations

Both applications share these locations by default:

| Data | Default path |
|---|---|
| Settings | `%USERPROFILE%\ZakonDownloader\config\settings.json` |
| Documents | `%USERPROFILE%\ZakonDownloader\data\documents` |
| SQLite history | `%USERPROFILE%\ZakonDownloader\data\history\history.db` |
| Native C log | `%USERPROFILE%\ZakonDownloader\data\logs\native_c.log` |

The document folder can be changed in the application settings. Back up the `ZakonDownloader` data folder before moving or deleting it if you need to keep downloaded files, settings, bookmarks, or history.

## Tests

Install the development dependencies and run the Python test suite from the repository root:

```powershell
.\.venv\Scripts\python.exe -m pip install -e ".[dev]"
.\.venv\Scripts\python.exe -m pytest -q
```

Alternatively, run `scripts\run_tests.ps1` in PowerShell. The native C build script runs its own tests by default; `-SkipTests` is available but is not recommended for routine builds.

## Repository layout

- `src/zakon_downloader/` — Python application.
- `native_c/` — native C/Win32 application, translations, assets, and tests.
- `tests/` — Python tests.
- `scripts/` — build, test, and localization-generation scripts.

## License and project

Released under the [MIT License](LICENSE). The license permits use, modification, redistribution, and commercial use subject to its terms.

This project is part of my software portfolio: [Fiverr profile](https://www.fiverr.com/s/aeeRdX7).