# ZakonDownloader

[English](README.md) | [Українська](README.uk.md) | [Deutsch](README.de.md) | [Français](README.fr.md) | [Español](README.es.md) | [Português](README.pt.md) | [简体中文](README.zh-CN.md)

ZakonDownloader est une application de bureau Windows open source permettant de télécharger des documents juridiques accessibles au public depuis le site officiel de la Verkhovna Rada d’Ukraine, [`zakon.rada.gov.ua`](https://zakon.rada.gov.ua/). Elle fournit une application Python/PySide6 ainsi qu’une application native C/Win32. Les deux versions utilisent les mêmes paramètres, le même dossier de documents et le même historique SQLite.

> **Portée et sécurité :** l’application fonctionne avec les documents publics de `zakon.rada.gov.ua`. Elle ne contourne ni CAPTCHA, ni connexion, ni mesures anti-bot, ni restrictions d’accès. N’y saisissez pas de mots de passe, de clés d’API ou de jetons.

## Fonctionnalités

- Télécharger un document à partir de son lien HTTPS direct en tâche de fond.
- Enregistrer le résultat dans un fichier `.txt` encodé en UTF-8 et conserver un historique local dans SQLite.
- Parcourir la bibliothèque locale de documents et rechercher ou filtrer l’historique des téléchargements.
- Utiliser une apparence claire ou sombre et basculer entre les modes d’interface Utilisateur et Développeur.
- Dans l’application native C, lire les documents enregistrés dans la visionneuse intégrée, parcourir une table des matières détectée, rechercher du texte, enregistrer des signets et reprendre la lecture, ajuster la taille du texte, copier une citation tenant compte de la source et exporter un PDF au format livre.
- Dans l’application native C, exporter l’historique au format CSV. L’export utilise l’UTF-8 avec BOM et est protégé contre l’interprétation des formules par les tableurs.

Le téléchargeur accepte les URL de documents publics de `zakon.rada.gov.ua` au format `/laws/show/<id>`. Il valide les redirections HTTPS vers le même hôte, limite la taille des réponses et réessaie en cas d’échecs temporaires ; il ne s’agit pas d’un outil général d’extraction de sites web. Les textes juridiques téléchargés restent dans leur langue source : la langue de l’interface ne traduit pas les documents.

## Langues de l’interface

L’interface graphique native C/Win32 est disponible dans les sept langues ci-dessous. L’interface graphique Python/PySide6 est actuellement disponible uniquement en ukrainien.

| Langue de l’interface | Native C/Win32 | Python/PySide6 |
|---|:---:|:---:|
| Ukrainien | Oui | Oui |
| Anglais | Oui | — |
| Allemand | Oui | — |
| Français | Oui | — |
| Espagnol | Oui | — |
| Portugais | Oui | — |
| Chinois simplifié | Oui | — |

Dans l’application native C, changez la langue via **Paramètres → Langue de l’interface**. La sélection est enregistrée dans `settings.json` ; elle ne modifie pas la langue des documents téléchargés. Les variantes du README liées ci-dessus couvrent toutes les langues de l’interface graphique.

## Prérequis

- Windows 10 ou Windows 11, 64 bits.
- Un accès Internet pour récupérer les documents.
- Python 3.11 ou une version ultérieure et le Python Launcher (`py`) pour exécuter la version Python.
- Zig 0.17.0 avec `zig cc` pour compiler la version native C.

## Exécuter la version Python

Dans PowerShell, clonez le dépôt et exécutez les commandes suivantes depuis sa racine :

```powershell
git clone https://github.com/Danuha99/ZakonRadaParser.git
Set-Location ZakonRadaParser
py -3 -m venv .venv
.\.venv\Scripts\python.exe -m pip install --upgrade pip
.\.venv\Scripts\python.exe -m pip install -e .
.\.venv\Scripts\python.exe -m zakon_downloader
```

Il n’est pas nécessaire d’activer l’environnement virtuel ; appelez son exécutable Python comme indiqué. Remplacez le sélecteur de version Python si vous avez besoin d’une version précise installée, par exemple `py -3.11`.

### Créer un exécutable Python autonome

Installez les dépendances de développement, puis exécutez le script d’empaquetage depuis PowerShell :

```powershell
.\.venv\Scripts\python.exe -m pip install -e ".[dev]"
.\scripts\build_exe.ps1
```

L’exécutable est écrit dans `dist\ZakonDownloader.exe`.

## Compiler la version native C/Win32

Depuis la racine du dépôt dans PowerShell :

```powershell
.\scripts\build_native_c.ps1
.\native_c\dist\ZakonDownloader.exe
```

La compilation nécessite Zig 0.17.0 (`zig cc`) dans le `PATH`. Si Zig est installé ailleurs, indiquez son chemin :

```powershell
.\scripts\build_native_c.ps1 -ZigPath 'C:\Tools\zig\zig.exe'
```

Le script de compilation exécute l’analyseur natif, la localisation, les vérifications des paramètres et de l’historique ainsi que les vérifications associées avant de produire l’exécutable graphique 64 bits. La sortie est `native_c\dist\ZakonDownloader.exe`.

## Lecteur local et exports

Dans l’application native C, ouvrez **Mes documents**, sélectionnez un fichier `.txt` enregistré et choisissez **Lire dans l’application**. Le lecteur peut afficher le texte juridique nettoyé ou le fichier enregistré complet, rechercher du texte, parcourir les sections détectées, conserver les signets et la position de lecture, et copier une citation avec son titre et son URL source. **Exporter en PDF** crée un PDF distinct au format livre à côté du fichier texte ; cette fonction nécessite l’imprimante Windows **Microsoft Print to PDF** et ne modifie pas le fichier `.txt` original.

Les exports CSV de l’historique sont écrits dans `%USERPROFILE%\ZakonDownloader\data\exported_logs\`. Les documents et les entrées d’historique existants ne sont pas supprimés par ces exports.

## Emplacements des données

Les deux applications partagent par défaut les emplacements suivants :

| Données | Chemin par défaut |
|---|---|
| Paramètres | `%USERPROFILE%\ZakonDownloader\config\settings.json` |
| Documents | `%USERPROFILE%\ZakonDownloader\data\documents` |
| Historique SQLite | `%USERPROFILE%\ZakonDownloader\data\history\history.db` |
| Journal natif C | `%USERPROFILE%\ZakonDownloader\data\logs\native_c.log` |

Le dossier de documents peut être modifié dans les paramètres de l’application. Sauvegardez le dossier de données `ZakonDownloader` avant de le déplacer ou de le supprimer si vous devez conserver les fichiers téléchargés, les paramètres, les signets ou l’historique.

## Tests

Installez les dépendances de développement et exécutez la suite de tests Python depuis la racine du dépôt :

```powershell
.\.venv\Scripts\python.exe -m pip install -e ".[dev]"
.\.venv\Scripts\python.exe -m pytest -q
```

Vous pouvez également exécuter `scripts\run_tests.ps1` dans PowerShell. Le script de compilation native C lance ses propres tests par défaut ; `-SkipTests` est disponible, mais n’est pas recommandé pour les compilations de routine.

## Organisation du dépôt

- `src/zakon_downloader/` — application Python.
- `native_c/` — application native C/Win32, traductions, ressources et tests.
- `tests/` — tests Python.
- `scripts/` — scripts de compilation, de test et de génération des traductions.

## Licence et projet

Publié sous la [licence MIT](LICENSE). La licence autorise l’utilisation, la modification, la redistribution et l’utilisation commerciale, sous réserve de ses conditions.

Ce projet fait partie de mon portfolio logiciel : [profil Fiverr](https://www.fiverr.com/s/aeeRdX7).
