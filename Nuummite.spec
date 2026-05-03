# -*- mode: python ; coding: utf-8 -*-


a = Analysis(
    ['python\\main.py'],
    pathex=[],
    binaries=[
        ('third_party\\opus\\opus.dll', '.'),
        ('third_party\\rnnoise\\rnnoise.dll', '.'),
        ('third_party\\libportaudio\\libportaudio.dll', '.'),
        ('third_party\\webrtc_audio_processing\\bin\\*.dll', '.'),
        ('third_party\\libsodium\\libsodium.dll', '.'),
        ('third_party\\libsodium\\bin\\libsodium-26.dll', '.'),
        ('third_party\\libsodium\\bin\\libgcc_s_seh-1.dll', '.'),
        ('third_party\\libsodium\\bin\\libwinpthread-1.dll', '.'),
    ],
    datas=[
        ('Nuummite\\ui\\*.ui', 'Nuummite\\ui'),
        ('Nuummite\\technical-support.ico', 'Nuummite'),
    ],
    hiddenimports=[],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    noarchive=False,
    optimize=0,
)
pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=True,
    name='Nuummite',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon=['Nuummite\\technical-support.ico'],
)
coll = COLLECT(
    exe,
    a.binaries,
    a.datas,
    strip=False,
    upx=True,
    upx_exclude=[],
    name='Nuummite',
)
