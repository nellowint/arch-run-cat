# arch-run-cat

A cute animated cat running on your Arch Linux XFCE taskbar via native panel plugin! 🐾

![demo](resources/demo.gif)

Native XFCE panel plugin animates an adorable cat (5 frames × 2 themes) directly in `xfce4-panel`. Animation speed follows CPU usage (10-20 FPS) and the tooltip shows `CPU Usage: XX%`.

## Requirements

- Arch Linux + XFCE (`xfce4-panel >=4.18`)
- `gtk3`, `libxfconf`, `gdk-pixbuf2`, `cairo`
- Build: `base-devel`, `meson`, `pkgconf`

## Install (AUR — recommended)

```bash
yay -S arch-run-cat      # or paru -S arch-run-cat
xfce4-panel -r
# Then: Panel → Add New Items → Run Cat → Add
```

## Install from source

```bash
git clone https://github.com/nellowint/arch-run-cat.git
cd arch-run-cat
meson setup build --prefix=/usr
meson compile -C build
sudo meson install -C build
xfce4-panel -r
# Then: Panel → Add New Items → Run Cat
```

> To update: `git pull && meson compile -C build && sudo meson install -C build && xfce4-panel -r`

## Add to XFCE Panel

1. Right-click the panel → **Panel** → **Add New Items…**
2. Search **Run Cat** → **Add** → **Close**.
3. The cat appears immediately, animating at CPU-proportional speed.

No `Command` or `Period` configuration needed (handled internally via `g_timeout_add`).

## Theme

Auto-detected from XFCE: `xfconf-query -c xsettings -p /Net/ThemeName` (`*dark*` → `dark_cat`, otherwise `light_cat`). The plugin also watches `xfconf` for live theme changes.

Force a theme if needed:

```bash
RUN_CAT_THEME=dark xfce4-panel -r
RUN_CAT_THEME=light xfce4-panel -r
```

Frames are `resources/cat/dark_cat_0..4.png` and `light_cat_0..4.png` (24×24 PNG) installed to `/usr/share/arch-run-cat/cat/`.

## How it works

- **CPU**: reads `/proc/stat` delta (`Idle = idle+iowait`, `NonIdle = user+nice+system+irq+softirq+steal`) every 1s, `CPU% = (totald-idled)*100/totald` clamped 0–100.
- **Animation**: `g_timeout_add(50-100ms)` drives `GtkImage` with scaled `GdkPixbuf` (16-48px bilinear). Delay is CPU-proportional: `CPU<10→100ms` (10 FPS), `10-20→90ms`, `20-40→80ms`, `40-50→70ms`, `50-70→60ms`, `>=70→50ms` (20 FPS). No Genmon, no reset.
- **Resources**: `DATADIR=/usr/share/arch-run-cat/cat` (meson `prefix/datadir`), fallback `XDG_DATA_HOME` and `GtkSettings` for theme. No `frame.state` needed.
- **Tooltip**: `CPU Usage: XX%` on hover, label ` XX%` beside the cat.

## Troubleshooting

- **Plugin not in list**: check `ls /usr/lib/xfce4/panel/plugins/libruncat.so && ls /usr/share/xfce4/panel/plugins/runcat.desktop` and rebuild with `meson compile -C build && sudo meson install -C build && xfce4-panel -r`.
- **No image / wrong theme**: check `xfconf-query -c xsettings -p /Net/ThemeName` and `RUN_CAT_THEME` env, verify `ls /usr/share/arch-run-cat/cat/`.
- **Build fails**: ensure `base-devel`, `meson`, `pkgconf`, `xfce4-panel` dev headers are installed.

## Migrating from Genmon (run-cat.sh)

Old versions used Genmon with `run-cat.sh` and GIFs:

```bash
# 1. Remove Genmon item from panel (right-click → Remove)
# 2. Clean legacy files (if you cloned before)
rm -rf ~/.local/share/arch-run-cat
rm -rf "${XDG_RUNTIME_DIR:-/tmp}/runcat" /tmp/runcat_frame.state
# 3. Install native plugin (see above)
```

## Uninstall

```bash
# AUR
sudo pacman -R arch-run-cat
# Source build
sudo ninja -C build uninstall  # or: sudo rm /usr/lib/xfce4/panel/plugins/libruncat.so /usr/share/xfce4/panel/plugins/runcat.desktop
sudo rm -rf /usr/share/arch-run-cat
xfce4-panel -r
```

## License

MIT — see [LICENSE](LICENSE).
