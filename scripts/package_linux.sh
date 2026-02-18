#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${root_dir}/build-linux"

cmake -B "${build_dir}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${build_dir}" -j

(cd "${build_dir}" && cpack -G TGZ)
(cd "${build_dir}" && cpack -G DEB)

echo "Packages generated in: ${build_dir}"
