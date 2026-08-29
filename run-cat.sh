#!/bin/bash
set -euo pipefail

# Diretório do script (resolve caminho absoluto) - FASE1 B3
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
FRAME_BASE="${SCRIPT_DIR}/resources/cat"
TOTAL_FRAMES=5

# Auto-detect tema - dark para tema dark, light para tema claro (usuário solicitou)
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

# Arquivos de estado - FASE2: XDG_RUNTIME_DIR com fallback /tmp + escrita atômica
STATE_DIR="${XDG_RUNTIME_DIR:-/tmp}/runcat"
mkdir -p "$STATE_DIR"
STATE_FILE="${STATE_DIR}/frame.state"
COUNT_FILE="${STATE_DIR}/count.state"

# Migração: se arquivos legados existem em /tmp e novos não existem, migra
if [ ! -f "$STATE_FILE" ] && [ -f "/tmp/runcat_frame.state" ]; then
    cp "/tmp/runcat_frame.state" "$STATE_FILE" 2>/dev/null || echo "0" > "$STATE_FILE"
fi
if [ ! -f "$COUNT_FILE" ] && [ -f "/tmp/runcat_count.state" ]; then
    cp "/tmp/runcat_count.state" "$COUNT_FILE" 2>/dev/null || echo "0" > "$COUNT_FILE"
fi
[ ! -f "$STATE_FILE" ] && echo "0" > "$STATE_FILE"
[ ! -f "$COUNT_FILE" ] && echo "0" > "$COUNT_FILE"

CURRENT_FRAME=$(cat "$STATE_FILE" 2>/dev/null || echo "0")
COUNT=$(cat "$COUNT_FILE" 2>/dev/null || echo "0")

# Validação de numéricos (evita erro com arquivo corrompido)
[[ "$CURRENT_FRAME" =~ ^[0-9]+$ ]] || CURRENT_FRAME=0
[[ "$COUNT" =~ ^[0-9]+$ ]] || COUNT=0
# Garante frame dentro do range (corrige migração legada com valor 99)
CURRENT_FRAME=$(( CURRENT_FRAME % TOTAL_FRAMES ))

# Calcula o uso da CPU - FASE1 B1/B2/B4: LC_ALL=C + grep -i + fallback
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

# Define a velocidade de troca de frames - Opção D: sempre correndo
# Period Genmon 0.10s: LIMIT 0 = 0.10s/frame (0.5s/ciclo 10 FPS), LIMIT 1 = 0.20s/frame (1.0s/ciclo 5 FPS)
if [ "$CPU_INT" -gt 60 ]; then LIMIT=0;    # Super rápido (>60% CPU)
else LIMIT=1;                            # Correndo (sempre animando, idle = 5 FPS)
fi

if [ "$COUNT" -ge "$LIMIT" ]; then
    CURRENT_FRAME=$(( (CURRENT_FRAME + 1) % TOTAL_FRAMES ))
    echo "0" > "${COUNT_FILE}.$$" && mv "${COUNT_FILE}.$$" "$COUNT_FILE"
else
    echo $((COUNT + 1)) > "${COUNT_FILE}.$$" && mv "${COUNT_FILE}.$$" "$COUNT_FILE"
fi

echo "$CURRENT_FRAME" > "${STATE_FILE}.$$" && mv "${STATE_FILE}.$$" "$STATE_FILE"

# Caminho final da imagem - PNG convertido de ICO (magick), fallback ICO
IMAGE_PATH="${FRAME_BASE}/${PREFIX}_${CURRENT_FRAME}.png"
if [ ! -f "$IMAGE_PATH" ]; then
    IMAGE_PATH="${FRAME_BASE}/${PREFIX}_${CURRENT_FRAME}.ico"
fi
if [ ! -f "$IMAGE_PATH" ]; then
    # Fallback genérico (tenta dark, depois light)
    IMAGE_PATH="${FRAME_BASE}/dark_cat_0.png"
    [ -f "$IMAGE_PATH" ] || IMAGE_PATH="${FRAME_BASE}/dark_cat_0.ico"
    [ -f "$IMAGE_PATH" ] || IMAGE_PATH="${FRAME_BASE}/light_cat_0.png"
fi

# Saída formatada para o Genmon do XFCE
echo "<img>${IMAGE_PATH}</img>"
echo "<txt> ${CPU_INT}%</txt>"
echo "<tool>Uso de CPU: ${CPU_INT}%</tool>"
