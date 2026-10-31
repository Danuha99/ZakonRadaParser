# ZakonDownloader

[English](README.md) | [Українська](README.uk.md) | [Deutsch](README.de.md) | [Français](README.fr.md) | [Español](README.es.md) | [Português](README.pt.md) | [简体中文](README.zh-CN.md)

O ZakonDownloader é uma aplicação de código aberto para Windows que permite descarregar documentos jurídicos disponíveis publicamente no site oficial da Verkhovna Rada da Ucrânia, [`zakon.rada.gov.ua`](https://zakon.rada.gov.ua/). Disponibiliza uma aplicação Python/PySide6 e uma aplicação nativa em C/Win32. Ambas as versões utilizam as mesmas definições, pasta de documentos e histórico SQLite.

> **Âmbito e segurança:** A aplicação funciona com documentos públicos em `zakon.rada.gov.ua`. Não contorna CAPTCHA, início de sessão, medidas anti-bot nem restrições de acesso. Não introduza palavras-passe, chaves de API nem tokens.

## Funcionalidades

- Descarregar um documento a partir da sua ligação HTTPS direta numa operação em segundo plano.
- Guardar o resultado como ficheiro `.txt` UTF-8 e manter um histórico local em SQLite.
- Procurar na biblioteca de documentos local e pesquisar ou filtrar o histórico de transferências.
- Utilizar uma aparência clara ou escura e alternar entre os modos de interface Utilizador e Programador.
- Na aplicação nativa em C, ler documentos guardados no visualizador integrado, navegar por um índice detetado, pesquisar texto, guardar marcadores e retomar a leitura, ajustar o tamanho do texto, copiar uma citação com referência à fonte e exportar um PDF em formato de livro.
- Na aplicação nativa em C, exportar o histórico para CSV. A exportação utiliza UTF-8 com BOM e está protegida contra a interpretação de fórmulas por folhas de cálculo.

O descarregador aceita URLs de documentos públicos em `zakon.rada.gov.ua` no formato `/laws/show/<id>`. Valida redirecionamentos HTTPS para o mesmo anfitrião, limita o tamanho da resposta e repete tentativas em caso de falhas temporárias; não é um scraper de sites de uso geral. Os textos jurídicos descarregados permanecem na sua língua de origem — o idioma da interface não traduz os documentos.

## Idiomas da interface gráfica

A interface gráfica nativa C/Win32 está disponível nos sete idiomas abaixo. A interface gráfica Python/PySide6 está atualmente disponível apenas em ucraniano.

| Idioma da interface | C/Win32 nativo | Python/PySide6 |
|---|:---:|:---:|
| Ucraniano | Sim | Sim |
| Inglês | Sim | — |
| Alemão | Sim | — |
| Francês | Sim | — |
| Espanhol | Sim | — |
| Português | Sim | — |
| Chinês simplificado | Sim | — |

Na aplicação nativa em C, altere o idioma em **Configurações → Idioma da interface**. A seleção é guardada em `settings.json`; não altera o idioma dos documentos descarregados. As variantes do README ligadas acima abrangem todos os idiomas da interface gráfica.

## Requisitos

- Windows 10 ou Windows 11, 64 bits.
- Acesso à Internet para obter documentos.
- Python 3.11 ou mais recente e o Python Launcher (`py`) para executar a versão Python.
- Zig 0.17.0 com `zig cc` para compilar a versão nativa em C.

## Executar a versão Python

No PowerShell, clone o repositório e execute os seguintes comandos a partir da sua raiz:

```powershell
git clone https://github.com/Danuha99/ZakonRadaParser.git
Set-Location ZakonRadaParser
py -3 -m venv .venv
.\.venv\Scripts\python.exe -m pip install --upgrade pip
.\.venv\Scripts\python.exe -m pip install -e .
.\.venv\Scripts\python.exe -m zakon_downloader
```

Não é necessário ativar o ambiente virtual; invoque o executável Python conforme apresentado. Substitua o seletor de versão do Python se precisar de uma versão específica instalada, por exemplo, `py -3.11`.

### Compilar um executável Python autónomo

Instale as dependências de desenvolvimento e execute o script de empacotamento a partir do PowerShell:

```powershell
.\.venv\Scripts\python.exe -m pip install -e ".[dev]"
.\scripts\build_exe.ps1
```

O executável é gravado em `dist\ZakonDownloader.exe`.

## Compilar a versão nativa em C/Win32

A partir da raiz do repositório no PowerShell:

```powershell
.\scripts\build_native_c.ps1
.\native_c\dist\ZakonDownloader.exe
```

A compilação requer Zig 0.17.0 (`zig cc`) no `PATH`. Se o Zig estiver instalado noutro local, forneça o respetivo caminho:

```powershell
.\scripts\build_native_c.ps1 -ZigPath 'C:\Tools\zig\zig.exe'
```

O script de compilação executa o analisador nativo, a localização, as definições/histórico e verificações relacionadas antes de produzir o executável de interface gráfica de 64 bits. O resultado é `native_c\dist\ZakonDownloader.exe`.

## Leitor local e exportações

Na aplicação nativa em C, abra **Os meus documentos**, selecione um ficheiro `.txt` guardado e escolha **Ler na aplicação**. O leitor pode apresentar o texto jurídico limpo ou o ficheiro guardado completo, localizar texto, navegar pelas secções detetadas, preservar marcadores e a posição de leitura e copiar uma citação com o respetivo título e URL da fonte. **Exportar PDF** cria um PDF separado em formato de livro junto ao ficheiro de texto; requer a impressora do Windows **Microsoft Print to PDF** e não modifica o ficheiro `.txt` original.

As exportações do histórico para CSV são gravadas em `%USERPROFILE%\ZakonDownloader\data\exported_logs\`. Os documentos e as entradas do histórico existentes não são removidos por estas exportações.

## Localizações dos dados

Por predefinição, ambas as aplicações partilham estas localizações:

| Dados | Caminho predefinido |
|---|---|
| Definições | `%USERPROFILE%\ZakonDownloader\config\settings.json` |
| Documentos | `%USERPROFILE%\ZakonDownloader\data\documents` |
| Histórico SQLite | `%USERPROFILE%\ZakonDownloader\data\history\history.db` |
| Registo C nativo | `%USERPROFILE%\ZakonDownloader\data\logs\native_c.log` |

A pasta de documentos pode ser alterada nas definições da aplicação. Faça uma cópia de segurança da pasta de dados `ZakonDownloader` antes de a mover ou eliminar se precisar de manter ficheiros descarregados, definições, marcadores ou histórico.

## Testes

Instale as dependências de desenvolvimento e execute o conjunto de testes Python a partir da raiz do repositório:

```powershell
.\.venv\Scripts\python.exe -m pip install -e ".[dev]"
.\.venv\Scripts\python.exe -m pytest -q
```

Como alternativa, execute `scripts\run_tests.ps1` no PowerShell. O script de compilação nativa em C executa os seus próprios testes por predefinição; `-SkipTests` está disponível, mas não é recomendado para compilações de rotina.

## Estrutura do repositório

- `src/zakon_downloader/` — aplicação Python.
- `native_c/` — aplicação nativa em C/Win32, traduções, recursos e testes.
- `tests/` — testes Python.
- `scripts/` — scripts de compilação, testes e geração de localizações.

## Licença e projeto

Disponibilizado sob a [Licença MIT](LICENSE). A licença permite a utilização, modificação, redistribuição e utilização comercial, sujeitas aos seus termos.

Este projeto faz parte do meu portefólio de software: [perfil do Fiverr](https://www.fiverr.com/s/aeeRdX7).
