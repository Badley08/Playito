#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail

if [[ $# -ne 1 || ! -f "$1" ]]; then
  printf 'Usage: bash %s /path/to/Playito-x86_64.AppImage\n' "$0" >&2
  exit 2
fi

project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
appimage="$(realpath -- "$1")"
data_home="${XDG_DATA_HOME:-${HOME}/.local/share}"
bin_directory="${HOME}/.local/bin"
applications_directory="${data_home}/applications"
icon_directory="${data_home}/icons/hicolor/512x512/apps"
desktop_file="${applications_directory}/playito.desktop"

mkdir -p "${bin_directory}" "${applications_directory}" "${icon_directory}"
ln -sfn "${appimage}" "${bin_directory}/playito"
cp "${project_root}/playito.png" "${icon_directory}/playito.png"

escaped_executable="${bin_directory}/playito"
escaped_executable="${escaped_executable//\\/\\\\}"
escaped_executable="${escaped_executable//\"/\\\"}"
escaped_executable="${escaped_executable//%/%%}"
while IFS= read -r line; do
  if [[ "${line}" == "Exec=playito %F" ]]; then
    printf 'Exec="%s" %%F\n' "${escaped_executable}"
  else
    printf '%s\n' "${line}"
  fi
done < "${project_root}/packaging/playito.desktop" > "${desktop_file}"

if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database "${applications_directory}"
fi

printf 'Playito is registered for audio and video files.\n'
