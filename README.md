# wf-panel-pi swaytitle

A wf-panel-pi widget that shows the focused window title from sway.

Features:
- updates from sway IPC events (window and workspace)
- configurable title length limit
- default maximum visible characters: 20

## Build

```bash
meson setup builddir --prefix=/usr --libdir=lib/aarch64-linux-gnu
meson compile -C builddir
```

## Install

```bash
sudo meson install -C builddir
```

Installs:
- /usr/lib/aarch64-linux-gnu/wf-panel-pi/libswaytitle.so
- /usr/share/wf-panel-pi/metadata/swaytitle.xml

## wf-panel-pi config

Add `swaytitle` to your widget list in wf-panel-pi config.
