# ZakonDownloader

[English](README.md) | [Українська](README.uk.md) | [Deutsch](README.de.md) | [Français](README.fr.md) | [Español](README.es.md) | [Português](README.pt.md) | [简体中文](README.zh-CN.md)

ZakonDownloader ist eine Open-Source-Windows-Desktopanwendung zum Herunterladen öffentlich verfügbarer Rechtsdokumente von der offiziellen Website der Werchowna Rada der Ukraine, [`zakon.rada.gov.ua`](https://zakon.rada.gov.ua/). Sie bietet sowohl eine Python/PySide6-Anwendung als auch eine native C-Anwendung für Win32. Beide Versionen verwenden dieselben Einstellungen, denselben Dokumentordner und denselben SQLite-Verlauf.

> **Umfang und Sicherheit:** Die Anwendung arbeitet mit öffentlichen Dokumenten auf `zakon.rada.gov.ua`. Sie umgeht weder CAPTCHA, Anmeldung, Anti-Bot-Maßnahmen noch Zugriffsbeschränkungen. Geben Sie keine Passwörter, API-Schlüssel oder Token ein.

## Funktionen

- Ein Dokument über seinen direkten HTTPS-Link im Hintergrund herunterladen.
- Das Ergebnis als UTF-8-`.txt`-Datei speichern und einen lokalen Verlauf in SQLite führen.
- Die lokale Dokumentbibliothek durchsuchen sowie den Downloadverlauf durchsuchen oder filtern.
- Eine helle oder dunkle Darstellung verwenden und zwischen den Oberflächenmodi „Benutzer“ und „Entwickler“ wechseln.
- In der nativen C-Anwendung gespeicherte Dokumente im integrierten Viewer lesen, durch ein erkanntes Inhaltsverzeichnis navigieren, Text suchen, Lesezeichen speichern und das Lesen fortsetzen, die Textgröße anpassen, ein quellenbezogenes Zitat kopieren und ein buchähnliches PDF exportieren.
- In der nativen C-Anwendung den Verlauf als CSV exportieren. Der Export verwendet UTF-8 mit BOM und ist gegen die Interpretation als Tabellenkalkulationsformel geschützt.

Der Downloader akzeptiert öffentliche Dokument-URLs auf `zakon.rada.gov.ua` im Format `/laws/show/<id>`. Er validiert HTTPS-Weiterleitungen auf denselben Host, begrenzt die Antwortgröße und wiederholt Anfragen bei vorübergehenden Fehlern; er ist kein allgemeiner Website-Scraper. Heruntergeladene Rechtstexte bleiben in ihrer Ausgangssprache – die Oberflächensprache übersetzt keine Dokumente.

## GUI-Sprachen

Die native C/Win32-GUI ist in allen sieben unten aufgeführten Sprachen verfügbar. Die Python/PySide6-GUI ist derzeit nur auf Ukrainisch verfügbar.

| Oberflächensprache | Native C/Win32 | Python/PySide6 |
|---|:---:|:---:|
| Ukrainisch | Ja | Ja |
| Englisch | Ja | — |
| Deutsch | Ja | — |
| Französisch | Ja | — |
| Spanisch | Ja | — |
| Portugiesisch | Ja | — |
| Vereinfachtes Chinesisch | Ja | — |

Ändern Sie in der nativen C-Anwendung die Sprache unter **Einstellungen → Oberflächensprache**. Die Auswahl wird in `settings.json` gespeichert; sie ändert nicht die Sprache der heruntergeladenen Dokumente. Die oben verlinkten README-Varianten decken alle GUI-Sprachen ab.

## Voraussetzungen

- Windows 10 oder Windows 11, 64-Bit.
- Internetzugang zum Abrufen von Dokumenten.
- Python 3.11 oder neuer sowie der Python Launcher (`py`) zum Ausführen der Python-Version.
- Zig 0.17.0 mit `zig cc` zum Erstellen der nativen C-Version.

## Python-Version ausführen

Klonen Sie das Repository in PowerShell und führen Sie die folgenden Befehle aus dem Stammverzeichnis aus:

```powershell
git clone https://github.com/Danuha99/ZakonRadaParser.git
Set-Location ZakonRadaParser
py -3 -m venv .venv
.\.venv\Scripts\python.exe -m pip install --upgrade pip
.\.venv\Scripts\python.exe -m pip install -e .
.\.venv\Scripts\python.exe -m zakon_downloader
```

Sie müssen die virtuelle Umgebung nicht aktivieren; rufen Sie die Python-Executable wie gezeigt auf. Ersetzen Sie den Python-Versionsselektor, wenn Sie eine bestimmte installierte Version benötigen, zum Beispiel `py -3.11`.

### Eigenständige Python-Executable erstellen

Installieren Sie die Entwicklungsabhängigkeiten und führen Sie anschließend das Paketierungsskript in PowerShell aus:

```powershell
.\.venv\Scripts\python.exe -m pip install -e ".[dev]"
.\scripts\build_exe.ps1
```

Die Executable wird in `dist\ZakonDownloader.exe` geschrieben.

## Native C/Win32-Version erstellen

Führen Sie die folgenden Befehle im Stammverzeichnis des Repositorys in PowerShell aus:

```powershell
.\scripts\build_native_c.ps1
.\native_c\dist\ZakonDownloader.exe
```

Für den Build muss sich Zig 0.17.0 (`zig cc`) in `PATH` befinden. Wenn Zig an einem anderen Ort installiert ist, geben Sie den Pfad an:

```powershell
.\scripts\build_native_c.ps1 -ZigPath 'C:\Tools\zig\zig.exe'
```

Das Buildskript führt vor der Erstellung der 64-Bit-GUI-Executable den nativen Parser, die Lokalisierung, die Einstellungen und den Verlauf sowie zugehörige Prüfungen aus. Die Ausgabe befindet sich in `native_c\dist\ZakonDownloader.exe`.

## Lokaler Reader und Exporte

Öffnen Sie in der nativen C-Anwendung **Meine Dokumente**, wählen Sie eine gespeicherte `.txt`-Datei aus und wählen Sie **In App lesen**. Der Reader kann den bereinigten Rechtstext oder die vollständig gespeicherte Datei anzeigen, Text suchen, durch erkannte Abschnitte navigieren, Lesezeichen und die Leseposition beibehalten sowie ein Zitat mit Titel und Quell-URL kopieren. **PDF exportieren** erstellt neben der Textdatei ein separates buchähnliches PDF; dafür ist der Windows-Drucker **Microsoft Print to PDF** erforderlich. Die ursprüngliche `.txt`-Datei wird dabei nicht verändert.

CSV-Exporte des Verlaufs werden in `%USERPROFILE%\ZakonDownloader\data\exported_logs\` geschrieben. Vorhandene Dokumente und Verlaufseinträge werden durch diese Exporte nicht entfernt.

## Datenspeicherorte

Beide Anwendungen verwenden standardmäßig diese Speicherorte:

| Daten | Standardspeicherort |
|---|---|
| Einstellungen | `%USERPROFILE%\ZakonDownloader\config\settings.json` |
| Dokumente | `%USERPROFILE%\ZakonDownloader\data\documents` |
| SQLite-Verlauf | `%USERPROFILE%\ZakonDownloader\data\history\history.db` |
| Protokoll der nativen C-Anwendung | `%USERPROFILE%\ZakonDownloader\data\logs\native_c.log` |

Der Dokumentordner kann in den Anwendungseinstellungen geändert werden. Sichern Sie den Datenordner `ZakonDownloader`, bevor Sie ihn verschieben oder löschen, wenn Sie heruntergeladene Dateien, Einstellungen, Lesezeichen oder den Verlauf behalten möchten.

## Tests

Installieren Sie die Entwicklungsabhängigkeiten und führen Sie die Python-Testsuite aus dem Stammverzeichnis des Repositorys aus:

```powershell
.\.venv\Scripts\python.exe -m pip install -e ".[dev]"
.\.venv\Scripts\python.exe -m pytest -q
```

Alternativ können Sie `scripts\run_tests.ps1` in PowerShell ausführen. Das Buildskript für die native C-Version führt standardmäßig eigene Tests aus; `-SkipTests` ist verfügbar, wird für routinemäßige Builds jedoch nicht empfohlen.

## Repository-Aufbau

- `src/zakon_downloader/` — Python-Anwendung.
- `native_c/` — native C/Win32-Anwendung, Übersetzungen, Assets und Tests.
- `tests/` — Python-Tests.
- `scripts/` — Skripte zum Erstellen, Testen und Generieren der Lokalisierung.

## Lizenz und Projekt

Veröffentlicht unter der [MIT-Lizenz](LICENSE). Die Lizenz gestattet Nutzung, Änderung, Weitergabe und kommerzielle Nutzung vorbehaltlich ihrer Bedingungen.

Dieses Projekt ist Teil meines Software-Portfolios: [Fiverr-Profil](https://www.fiverr.com/s/aeeRdX7).
