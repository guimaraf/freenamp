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

# 1. Official yt-dlp nightly and Deno for YouTube JavaScript challenges.
if [ ! -x "${BIN_DIR}/yt-dlp" ] || [ ! -x "${BIN_DIR}/deno" ]; then
    work_dir=$(mktemp -d "${BIN_DIR}/extractors.XXXXXXXX")
    trap 'rm -f -- "${work_dir}/yt-dlp" "${work_dir}/SHA2-256SUMS" "${work_dir}/${deno_zip}" "${work_dir}/${deno_zip}.sha256sum" "${work_dir}/deno"; rmdir -- "${work_dir}"' EXIT
    yt_url='https://github.com/yt-dlp/yt-dlp-nightly-builds/releases/latest/download'
    deno_url='https://github.com/denoland/deno/releases/latest/download'
    deno_zip='deno-x86_64-unknown-linux-gnu.zip'

    echo '[Freenamp] Baixando yt-dlp nightly e Deno...'
    curl -fsSL --retry 2 -o "${work_dir}/yt-dlp" "${yt_url}/yt-dlp"
    curl -fsSL --retry 2 -o "${work_dir}/SHA2-256SUMS" "${yt_url}/SHA2-256SUMS"
    curl -fsSL --retry 2 -o "${work_dir}/${deno_zip}" "${deno_url}/${deno_zip}"
    curl -fsSL --retry 2 -o "${work_dir}/${deno_zip}.sha256sum" "${deno_url}/${deno_zip}.sha256sum"
    (
        cd "${work_dir}"
        awk '$2 == "yt-dlp" || $2 == "*yt-dlp" { print; found = 1 } END { if (!found) exit 1 }' SHA2-256SUMS | sha256sum -c -
        read -r deno_hash _ < "${deno_zip}.sha256sum"
        [[ "${deno_hash}" =~ ^[[:xdigit:]]{64}$ ]]
        printf '%s  %s\n' "${deno_hash}" "${deno_zip}" | sha256sum -c -
        unzip -q "${deno_zip}" deno
        chmod +x yt-dlp deno
        ./yt-dlp --ignore-config --version
        ./deno --version
    )
    mv -f -- "${work_dir}/yt-dlp" "${work_dir}/deno" "${BIN_DIR}/"
fi

# 2. nlohmann/json.hpp (header-only)
JSON_DIR="${LIBS_DIR}/nlohmann"
mkdir -p "${JSON_DIR}"
JSON_DEST="${JSON_DIR}/json.hpp"
if [ ! -f "${JSON_DEST}" ]; then
    echo "[Freenamp] Baixando nlohmann/json.hpp..."
    curl -L -o "${JSON_DEST}" "https://github.com/nlohmann/json/releases/latest/download/json.hpp"
fi

echo "[Freenamp] Dependências de compile/ prontas para Linux!"
