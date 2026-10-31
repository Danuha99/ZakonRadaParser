# ZakonDownloader

[English](README.md) | [Українська](README.uk.md) | [Deutsch](README.de.md) | [Français](README.fr.md) | [Español](README.es.md) | [Português](README.pt.md) | [简体中文](README.zh-CN.md)

ZakonDownloader es una aplicación de escritorio de código abierto para Windows que permite descargar documentos jurídicos disponibles públicamente desde el sitio web oficial de la Rada Suprema de Ucrania, [`zakon.rada.gov.ua`](https://zakon.rada.gov.ua/). Ofrece una aplicación Python/PySide6 y una aplicación nativa C/Win32. Ambas versiones utilizan la misma configuración, carpeta de documentos e historial de SQLite.

> **Alcance y seguridad:** La aplicación funciona con documentos públicos de `zakon.rada.gov.ua`. No elude CAPTCHA, inicio de sesión, medidas contra bots ni restricciones de acceso. No introduzca contraseñas, claves API ni tokens.

## Funciones

- Descargar un documento desde su enlace HTTPS directo mediante una operación en segundo plano.
- Guardar el resultado como archivo `.txt` codificado en UTF-8 y mantener un historial local en SQLite.
- Explorar la biblioteca local de documentos y buscar o filtrar el historial de descargas.
- Utilizar una apariencia clara u oscura y cambiar entre los modos de interfaz Usuario y Desarrollador.
- En la aplicación nativa C, leer los documentos guardados en el visor integrado, desplazarse por una tabla de contenido detectada, buscar texto, guardar marcadores y reanudar la lectura, ajustar el tamaño del texto, copiar una cita con información de origen y exportar un PDF con formato de libro.
- En la aplicación nativa C, exportar el historial a CSV. La exportación utiliza UTF-8 con BOM y está protegida contra la interpretación de fórmulas por parte de las hojas de cálculo.

El descargador acepta URLs de documentos públicos de `zakon.rada.gov.ua` con el formato `/laws/show/<id>`. Valida las redirecciones HTTPS al mismo host, limita el tamaño de la respuesta y reintenta los fallos temporales; no es un raspador de sitios web de propósito general. Los textos jurídicos descargados se mantienen en su idioma de origen; el idioma de la interfaz no traduce los documentos.

## Idiomas de la interfaz gráfica

La interfaz gráfica nativa C/Win32 está disponible en los siete idiomas siguientes. Actualmente, la interfaz gráfica Python/PySide6 solo está disponible en ucraniano.

| Idioma de la interfaz | C/Win32 nativo | Python/PySide6 |
|---|:---:|:---:|
| Ucraniano | Sí | Sí |
| Inglés | Sí | — |
| Alemán | Sí | — |
| Francés | Sí | — |
| Español | Sí | — |
| Portugués | Sí | — |
| Chino simplificado | Sí | — |

En la aplicación nativa C, cambie el idioma en **Configuración → Idioma de la interfaz**. La selección se guarda en `settings.json`; no cambia el idioma de los documentos descargados. Las variantes del README enlazadas arriba cubren todos los idiomas de la interfaz gráfica.

## Requisitos

- Windows 10 o Windows 11, de 64 bits.
- Acceso a Internet para recuperar documentos.
- Python 3.11 o posterior y Python Launcher (`py`) para ejecutar la versión de Python.
- Zig 0.17.0 con `zig cc` para compilar la versión nativa C.

## Ejecutar la versión de Python

En PowerShell, clone el repositorio y ejecute los siguientes comandos desde su raíz:

```powershell
git clone https://github.com/Danuha99/ZakonRadaParser.git
Set-Location ZakonRadaParser
py -3 -m venv .venv
.\.venv\Scripts\python.exe -m pip install --upgrade pip
.\.venv\Scripts\python.exe -m pip install -e .
.\.venv\Scripts\python.exe -m zakon_downloader
```

No es necesario activar el entorno virtual; invoque el ejecutable de Python como se muestra. Sustituya el selector de versión de Python si necesita una versión específica instalada, por ejemplo, `py -3.11`.

### Compilar un ejecutable independiente de Python

Instale las dependencias de desarrollo y, a continuación, ejecute el script de empaquetado desde PowerShell:

```powershell
.\.venv\Scripts\python.exe -m pip install -e ".[dev]"
.\scripts\build_exe.ps1
```

El ejecutable se escribe en `dist\ZakonDownloader.exe`.

## Compilar la versión nativa C/Win32

Desde la raíz del repositorio en PowerShell:

```powershell
.\scripts\build_native_c.ps1
.\native_c\dist\ZakonDownloader.exe
```

La compilación requiere Zig 0.17.0 (`zig cc`) en `PATH`. Si Zig está instalado en otra ubicación, proporcione su ruta:

```powershell
.\scripts\build_native_c.ps1 -ZigPath 'C:\Tools\zig\zig.exe'
```

El script de compilación ejecuta el analizador nativo, la localización, la configuración y el historial, además de las comprobaciones relacionadas, antes de producir el ejecutable de la interfaz gráfica de 64 bits. La salida es `native_c\dist\ZakonDownloader.exe`.

## Lector local y exportaciones

En la aplicación nativa C, abra **Mis documentos**, seleccione un archivo `.txt` guardado y elija **Leer en la aplicación**. El lector puede mostrar el texto jurídico limpio o el archivo guardado completo, buscar texto, desplazarse por las secciones detectadas, conservar los marcadores y la posición de lectura, y copiar una cita con su título y URL de origen. **Exportar PDF** crea un PDF independiente con formato de libro junto al archivo de texto; requiere la impresora de Windows **Microsoft Print to PDF** y no modifica el archivo `.txt` original.

Las exportaciones del historial a CSV se escriben en `%USERPROFILE%\ZakonDownloader\data\exported_logs\`. Estas exportaciones no eliminan los documentos ni las entradas del historial existentes.

## Ubicaciones de los datos

Ambas aplicaciones comparten estas ubicaciones de forma predeterminada:

| Datos | Ruta predeterminada |
|---|---|
| Configuración | `%USERPROFILE%\ZakonDownloader\config\settings.json` |
| Documentos | `%USERPROFILE%\ZakonDownloader\data\documents` |
| Historial de SQLite | `%USERPROFILE%\ZakonDownloader\data\history\history.db` |
| Registro de C nativo | `%USERPROFILE%\ZakonDownloader\data\logs\native_c.log` |

La carpeta de documentos se puede cambiar en la configuración de la aplicación. Haga una copia de seguridad de la carpeta de datos de `ZakonDownloader` antes de moverla o eliminarla si necesita conservar los archivos descargados, la configuración, los marcadores o el historial.

## Pruebas

Instale las dependencias de desarrollo y ejecute el conjunto de pruebas de Python desde la raíz del repositorio:

```powershell
.\.venv\Scripts\python.exe -m pip install -e ".[dev]"
.\.venv\Scripts\python.exe -m pytest -q
```

Como alternativa, ejecute `scripts\run_tests.ps1` en PowerShell. El script de compilación de C nativo ejecuta sus propias pruebas de forma predeterminada; `-SkipTests` está disponible, pero no se recomienda para las compilaciones habituales.

## Estructura del repositorio

- `src/zakon_downloader/` — aplicación Python.
- `native_c/` — aplicación nativa C/Win32, traducciones, recursos y pruebas.
- `tests/` — pruebas de Python.
- `scripts/` — scripts de compilación, pruebas y generación de localizaciones.

## Licencia y proyecto

Publicado bajo la [Licencia MIT](LICENSE). La licencia permite el uso, la modificación, la redistribución y el uso comercial conforme a sus términos.

Este proyecto forma parte de mi portafolio de software: [perfil de Fiverr](https://www.fiverr.com/s/aeeRdX7).
