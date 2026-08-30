# arch-run-cat

A cute animated cat running on your Arch Linux XFCE taskbar via Genmon! 🐾

![demo](resources/demo.gif)

`run-cat.sh` displays an animated cat (5 frames × 2 themes) in the XFCE panel. Animation speed follows CPU usage and the cat tooltip shows `Uso de CPU: XX%`.

## Requirements

- Arch Linux + XFCE (`xfce4-panel`)
- `xfce4-genmon-plugin` (Generic Monitor for the panel)
- `xfconf-query` (`xfconf` package, usually already installed with XFCE)
- `bash`, `procps-ng` (`top`), `grep`, `awk`, `sed` (base system)

```bash
pacman -Qi xfce4-genmon-plugin &>/dev/null || sudo pacman -S xfce4-genmon-plugin
```

## Install

```bash
# 1. clone
git clone https://github.com/nellowint/arch-run-cat.git ~/.local/share/arch-run-cat
cd ~/.local/share/arch-run-cat

# 2. make executable
chmod +x run-cat.sh

# 3. quick test (must print 3 lines: <img>, <txt>, <tool>)
RUN_CAT_THEME=dark ./run-cat.sh
RUN_CAT_THEME=light ./run-cat.sh
./run-cat.sh | cat -A
```

> The script resolves paths via `BASH_SOURCE` (`run-cat.sh:5`), so `resources/cat/{dark,light}_cat_{0..4}.png` must stay next to the script. Move the whole folder, not just `run-cat.sh`.

## Add to XFCE Panel (Genmon)

1. Right-click the panel → **Panel** → **Add New Items…**
2. Search **Generic Monitor** (`xfce4-genmon-plugin`) → **Add**.
3. Right-click the new Genmon item → **Properties**:
   - **Command**: absolute path to the script, e.g. `/home/YOU/.local/share/arch-run-cat/run-cat.sh`
   - **Period (s)**: `0.25` (Genmon minimum)
   - **Label**: off (script already outputs `<txt> XX%</txt>`)
4. Save. The cat should appear immediately and animate.

## Theme

Auto-detected from XFCE: `xfconf-query -c xsettings -p /Net/ThemeName` (`*dark*` → `dark_cat`, otherwise `light_cat`).

Force a theme if needed — use it as the Genmon command:

```bash
RUN_CAT_THEME=dark /home/YOU/.local/share/arch-run-cat/run-cat.sh
RUN_CAT_THEME=light /home/YOU/.local/share/arch-run-cat/run-cat.sh
```

Frames are `resources/cat/dark_cat_0..4.png` and `light_cat_0..4.png` (24×24 PNG; `.ico` fallback is checked but not shipped).

## How it works

- **Genmon contract** (`run-cat.sh:86-88`): stdout must be exactly `<img>PATH</img>`, `<txt> XX%</txt>`, `<tool>Uso de CPU: XX%</tool>` — no extra output.
- **CPU**: `LC_ALL=C top -bn1 | grep -i "Cpu(s)"` → `100 - idle`, clamped 0–100 (`LC_ALL=C` + `grep -i` required for locale/case variants).
- **State**: `${XDG_RUNTIME_DIR:-/tmp}/runcat/frame.state` with atomic write (`> file.$$ && mv`). Legacy `/tmp/runcat_frame.state` is migrated once. Corrupted values reset to `0`.
- **Speed**: Genmon period `0.25s` (minimum). Fixed `0.25s/frame` (4 FPS / 1.25s cycle). The cat always runs.

## Troubleshooting

- **No image / broken icon**: check that `Command` is an absolute path and `resources/cat/` exists next to it. Test manually in a terminal.
- **Wrong theme**: run `xfconf-query -c xsettings -p /Net/ThemeName` and verify. Use `RUN_CAT_THEME` override if needed.
- **Genmon shows command output as text**: ensure Genmon period is `0.25` and the script is executable. Run `bash -n run-cat.sh` for syntax check.

## Uninstall

Remove the Genmon item from the panel, then delete the folder:

```bash
rm -rf ~/.local/share/arch-run-cat
rm -rf "${XDG_RUNTIME_DIR:-/tmp}/runcat"
```

## License

MIT — see [LICENSE](LICENSE).
