#!/usr/bin/env bash
# Script portátil para baixar e restaurar dependências Linux em compile/
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
COMPILE_DIR="${BASE_DIR}/compile"
BIN_DIR="${COMPILE_DIR}/bin"
LIBS_DIR="${COMPILE_DIR}/libs"

mkdir -p "${BIN_DIR}" "${LIBS_DIR}"

echo "[Freenamp] Verificando dependências em compile/..."

# 1. yt-dlp Linux x86_64 standalone
YTDLP_DEST="${BIN_DIR}/yt-dlp"
if [ ! -f "${YTDLP_DEST}" ]; then
    echo "[Freenamp] Baixando yt-dlp standalone para Linux..."
    curl -L -o "${YTDLP_DEST}" "https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp"
fi
chmod +x "${YTDLP_DEST}"

# 2. nlohmann/json.hpp (header-only)
JSON_DIR="${LIBS_DIR}/nlohmann"
mkdir -p "${JSON_DIR}"
JSON_DEST="${JSON_DIR}/json.hpp"
if [ ! -f "${JSON_DEST}" ]; then
    echo "[Freenamp] Baixando nlohmann/json.hpp..."
    curl -L -o "${JSON_DEST}" "https://github.com/nlohmann/json/releases/latest/download/json.hpp"
fi

echo "[Freenamp] Dependências de compile/ prontas para Linux!"
