#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${root_dir}/build-linux"

# Release packages link the in-process nazm-api-v1 assembler, so they need the
# pinned Nazm source tree: BAA_NAZM_SOURCE_DIR, else ./Nazm, else ../Nazm.
# That assembler is the packaged default; BAA_EMBEDDED_NAZM_DEFAULT=OFF builds
# the opt-in variant instead.
nazm_source_dir="${BAA_NAZM_SOURCE_DIR:-}"
if [[ -z "${nazm_source_dir}" ]]; then
  for candidate in "${root_dir}/Nazm" "${root_dir}/../Nazm"; do
    if [[ -f "${candidate}/include/nazm.h" ]]; then
      nazm_source_dir="${candidate}"
      break
    fi
  done
fi
if [[ ! -f "${nazm_source_dir}/include/nazm.h" ]]; then
  echo "error: release packages embed Nazm; set BAA_NAZM_SOURCE_DIR to its source tree" >&2
  exit 1
fi
nazm_source_dir="$(cd "${nazm_source_dir}" && pwd)"

cmake -B "${build_dir}" -DCMAKE_BUILD_TYPE=Release \
  -DBAA_ENABLE_EMBEDDED_NAZM=ON \
  -DBAA_EMBEDDED_NAZM_DEFAULT="${BAA_EMBEDDED_NAZM_DEFAULT:-ON}" \
  -DBAA_NAZM_SOURCE_DIR="${nazm_source_dir}"
cmake --build "${build_dir}" -j

(cd "${build_dir}" && cpack -G TGZ)
(cd "${build_dir}" && cpack -G DEB)
(
  cd "${build_dir}"
  for package in baa-*.tar.gz baa-*.deb; do
    sha256sum "${package}" > "${package}.sha256"
  done
)

echo "Packages generated in: ${build_dir}"
