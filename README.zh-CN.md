# ZakonDownloader

[English](README.md) | [Українська](README.uk.md) | [Deutsch](README.de.md) | [Français](README.fr.md) | [Español](README.es.md) | [Português](README.pt.md) | [简体中文](README.zh-CN.md)

ZakonDownloader 是一款开源 Windows 桌面应用，用于从乌克兰最高拉达官方网站 [`zakon.rada.gov.ua`](https://zakon.rada.gov.ua/) 下载公开提供的法律文件。它同时提供 Python/PySide6 应用和原生 C/Win32 应用。两个版本使用相同的设置、文档文件夹和 SQLite 历史记录。

> **范围与安全性：** 本应用处理 `zakon.rada.gov.ua` 上的公开文档。它不会绕过 CAPTCHA、登录、反机器人措施或访问限制。请勿输入密码、API 密钥或令牌。

## 功能

- 在后台操作中通过文档的直接 HTTPS 链接下载文档。
- 将结果保存为 UTF-8 `.txt` 文件，并在 SQLite 中保留本地历史记录。
- 浏览本地文档库，以及搜索或筛选下载历史记录。
- 使用浅色或深色外观，并在用户和开发者界面模式之间切换。
- 在原生 C 应用中，使用内置查看器阅读已保存的文档，浏览检测到的目录，搜索文本，保存书签并恢复阅读进度，调整文本大小，复制带来源信息的引用，以及导出书籍样式的 PDF。
- 在原生 C 应用中将历史记录导出为 CSV。导出文件使用带 BOM 的 UTF-8 编码，并可防止电子表格将内容解释为公式。

下载器接受 `zakon.rada.gov.ua` 上 `/laws/show/<id>` 格式的公开文档 URL。它会验证 HTTPS 重定向是否仍指向同一主机，限制响应大小，并重试临时性失败；它不是通用网站抓取工具。下载的法律文本会保留其源语言——界面语言不会翻译文档。

## GUI 语言

原生 C/Win32 GUI 提供以下七种语言。Python/PySide6 GUI 目前仅提供乌克兰语版本。

| 界面语言 | 原生 C/Win32 | Python/PySide6 |
|---|:---:|:---:|
| 乌克兰语 | 是 | 是 |
| 英语 | 是 | — |
| 德语 | 是 | — |
| 法语 | 是 | — |
| 西班牙语 | 是 | — |
| 葡萄牙语 | 是 | — |
| 简体中文 | 是 | — |

在原生 C 应用中，前往 **设置 → 界面语言** 更改语言。选择会保存在 `settings.json` 中；它不会改变下载文档的语言。上面链接的 README 变体涵盖所有 GUI 语言。

## 要求

- 64 位 Windows 10 或 Windows 11。
- 用于获取文档的互联网连接。
- Python 3.11 或更高版本，以及用于运行 Python 版本的 Python Launcher（`py`）。
- 使用 `zig cc` 构建原生 C 版本所需的 Zig 0.17.0。

## 运行 Python 版本

在 PowerShell 中克隆仓库，并从仓库根目录运行以下命令：

```powershell
git clone https://github.com/Danuha99/ZakonRadaParser.git
Set-Location ZakonRadaParser
py -3 -m venv .venv
.\.venv\Scripts\python.exe -m pip install --upgrade pip
.\.venv\Scripts\python.exe -m pip install -e .
.\.venv\Scripts\python.exe -m zakon_downloader
```

无需激活虚拟环境；如上所示直接调用其中的 Python 可执行文件即可。如果需要指定已安装的具体版本，请替换 Python 版本选择器，例如 `py -3.11`。

### 构建独立的 Python 可执行文件

安装开发依赖，然后从 PowerShell 运行打包脚本：

```powershell
.\.venv\Scripts\python.exe -m pip install -e ".[dev]"
.\scripts\build_exe.ps1
```

可执行文件会写入 `dist\ZakonDownloader.exe`。

## 构建原生 C/Win32 版本

在 PowerShell 中从仓库根目录运行：

```powershell
.\scripts\build_native_c.ps1
.\native_c\dist\ZakonDownloader.exe
```

构建要求 `PATH` 中包含 Zig 0.17.0（`zig cc`）。如果 Zig 安装在其他位置，请提供其路径：

```powershell
.\scripts\build_native_c.ps1 -ZigPath 'C:\Tools\zig\zig.exe'
```

构建脚本会在生成 64 位 GUI 可执行文件前，运行原生解析器、本地化、设置/历史记录及相关检查。输出文件为 `native_c\dist\ZakonDownloader.exe`。

## 本地阅读器和导出功能

在原生 C 应用中，打开 **我的文档**，选择一个已保存的 `.txt` 文件，然后选择 **在应用中阅读**。阅读器可以显示整理后的法律文本或完整的已保存文件，查找文本，浏览检测到的章节，保留书签和阅读位置，并复制包含标题和来源 URL 的引文。**导出 PDF** 会在文本文件旁创建一个单独的书籍样式 PDF；该功能需要 Windows 的 **Microsoft Print to PDF** 打印机，并且不会修改原始 `.txt` 文件。

CSV 历史记录导出文件会写入 `%USERPROFILE%\ZakonDownloader\data\exported_logs\`。这些导出操作不会删除现有文档和历史记录条目。

## 数据位置

默认情况下，两个应用共享以下位置：

| 数据 | 默认路径 |
|---|---|
| 设置 | `%USERPROFILE%\ZakonDownloader\config\settings.json` |
| 文档 | `%USERPROFILE%\ZakonDownloader\data\documents` |
| SQLite 历史记录 | `%USERPROFILE%\ZakonDownloader\data\history\history.db` |
| 原生 C 日志 | `%USERPROFILE%\ZakonDownloader\data\logs\native_c.log` |

可以在应用设置中更改文档文件夹。如果需要保留下载的文件、设置、书签或历史记录，请在移动或删除 `ZakonDownloader` 数据文件夹前进行备份。

## 测试

安装开发依赖，并从仓库根目录运行 Python 测试套件：

```powershell
.\.venv\Scripts\python.exe -m pip install -e ".[dev]"
.\.venv\Scripts\python.exe -m pytest -q
```

也可以在 PowerShell 中运行 `scripts\run_tests.ps1`。原生 C 构建脚本默认会运行自己的测试；虽然可以使用 `-SkipTests`，但不建议在日常构建中使用。

## 仓库结构

- `src/zakon_downloader/` — Python 应用。
- `native_c/` — 原生 C/Win32 应用、翻译、资源和测试。
- `tests/` — Python 测试。
- `scripts/` — 构建、测试和本地化生成脚本。

## 许可证和项目

本项目依据 [MIT License](LICENSE) 发布。该许可证允许使用、修改、再分发和商业使用，但须遵守其条款。

本项目是我的软件作品集的一部分：[Fiverr profile](https://www.fiverr.com/s/aeeRdX7)。
