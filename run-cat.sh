#!/bin/bash
set -euo pipefail

# Diretório do script (resolve caminho absoluto)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
FRAME_BASE="${SCRIPT_DIR}/resources/cat"
TOTAL_FRAMES=5

# Auto-detect tema - dark para tema dark, light para tema claro
# Tema atual: Materia-dark-compact (dark) -> dark_cat
XFCE_THEME=$(xfconf-query -c xsettings -p /Net/ThemeName 2>/dev/null || echo "Adwaita")
if [[ -n "${RUN_CAT_THEME:-}" ]]; then
    # Override manual: RUN_CAT_THEME=dark|light
    PREFIX="${RUN_CAT_THEME}_cat"
elif [[ "$XFCE_THEME" =~ [Dd]ark ]]; then
    PREFIX="dark_cat"
else
    PREFIX="light_cat"
fi

# Calcula o uso da CPU - LC_ALL=C + grep -i + fallback
CPU_USAGE=$(LC_ALL=C top -bn1 2>/dev/null | grep -i "Cpu(s)" | sed "s/.*, *\([0-9.]*\)%* id.*/\1/" | awk '{print 100 - $1}' 2>/dev/null || echo "0")
CPU_USAGE=${CPU_USAGE:-0}
# Converte para inteiro com arredondamento (printf) e valida - LC_ALL=C para ponto decimal
if ! CPU_INT=$(LC_ALL=C printf "%.0f" "$CPU_USAGE" 2>/dev/null); then
    CPU_INT=0
fi
[[ "$CPU_INT" =~ ^[0-9]+$ ]] || CPU_INT=0
# Clamp 0-100
[ "$CPU_INT" -gt 100 ] && CPU_INT=100
[ "$CPU_INT" -lt 0 ] && CPU_INT=0

# Velocidade proporcional à CPU — Genmon mínimo 0.25s, mas GIF anima internamente
# Buckets: <10 → 0.50s (2 FPS), 10-20 → 0.30s (3.3 FPS), 20-40 → 0.20s (5 FPS), 40-60 → 0.10s (10 FPS), >=60 → 0.07s (14 FPS)
# positive_correlation=true: maior CPU = mais rápido (inverso de config.json original)
if [ "$CPU_INT" -lt 10 ]; then DELAY="0.50"
elif [ "$CPU_INT" -lt 20 ]; then DELAY="0.30"
elif [ "$CPU_INT" -lt 40 ]; then DELAY="0.20"
elif [ "$CPU_INT" -lt 60 ]; then DELAY="0.10"
else DELAY="0.07"
fi

# Caminho final da imagem — GIF proporcional (fallback PNG com frame.state se GIF ausente)
IMAGE_PATH="${FRAME_BASE}/${PREFIX}_${DELAY}.gif"
if [ ! -f "$IMAGE_PATH" ]; then
    # Fallback PNG animado via frame.state (legado)
    STATE_DIR="${XDG_RUNTIME_DIR:-/tmp}/runcat"
    mkdir -p "$STATE_DIR"
    STATE_FILE="${STATE_DIR}/frame.state"
    if [ ! -f "$STATE_FILE" ] && [ -f "/tmp/runcat_frame.state" ]; then
        cp "/tmp/runcat_frame.state" "$STATE_FILE" 2>/dev/null || echo "0" > "$STATE_FILE"
    fi
    [ ! -f "$STATE_FILE" ] && echo "0" > "$STATE_FILE"
    CURRENT_FRAME=$(cat "$STATE_FILE" 2>/dev/null || echo "0")
    [[ "$CURRENT_FRAME" =~ ^[0-9]+$ ]] || CURRENT_FRAME=0
    CURRENT_FRAME=$(( CURRENT_FRAME % TOTAL_FRAMES ))
    CURRENT_FRAME=$(( (CURRENT_FRAME + 1) % TOTAL_FRAMES ))
    echo "$CURRENT_FRAME" > "${STATE_FILE}.$$" && mv "${STATE_FILE}.$$" "$STATE_FILE"
    IMAGE_PATH="${FRAME_BASE}/${PREFIX}_${CURRENT_FRAME}.png"
    [ -f "$IMAGE_PATH" ] || IMAGE_PATH="${FRAME_BASE}/${PREFIX}_${CURRENT_FRAME}.ico"
    [ -f "$IMAGE_PATH" ] || IMAGE_PATH="${FRAME_BASE}/dark_cat_0.png"
    [ -f "$IMAGE_PATH" ] || IMAGE_PATH="${FRAME_BASE}/dark_cat_0.ico"
    [ -f "$IMAGE_PATH" ] || IMAGE_PATH="${FRAME_BASE}/light_cat_0.png"
fi

# Saída formatada para o Genmon do XFCE
echo "<img>${IMAGE_PATH}</img>"
echo "<txt> ${CPU_INT}%</txt>"
echo "<tool>Uso de CPU: ${CPU_INT}%</tool>"
