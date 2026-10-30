# -*- mode: python ; coding: utf-8 -*-
block_cipher = None
a = Analysis(
    ['src/zakon_downloader/__main__.py'],
    pathex=['src'],
    binaries=[],
    datas=[],
    hiddenimports=[],hookspath=[],runtime_hooks=[],excludes=[],
    cipher=block_cipher,
)
pyz = PYZ(a.pure, cipher=block_cipher)
exe = EXE(pyz,a.scripts,a.binaries,a.datas, [],
    name='ZakonDownloader',debug=False,strip=False,upx=True,
    console=False,
)
