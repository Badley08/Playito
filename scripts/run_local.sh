#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail

project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_directory="${project_root}/build"

cmake -S "${project_root}" -B "${build_directory}" \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build "${build_directory}" --parallel 2
ctest --test-dir "${build_directory}" --output-on-failure
exec "${build_directory}/playito" "$@"
