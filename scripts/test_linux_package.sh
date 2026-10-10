#!/usr/bin/env bash
# Verify the Baa Linux packages the way test_installer.ps1 verifies the Windows
# installer: digest, install, compile and run through the embedded Nazm with no
# Nazm executable reachable, the --nazm-path and GAS rollbacks, the host linker,
# removal, and nothing left behind.
#
# Run it as root on a machine without Baa or a C toolchain. CI runs it in a
# fresh ubuntu:24.04 container, so whatever Baa needs must come from the
# package's own dependencies.
set -euo pipefail

usage() {
  echo "usage: scripts/test_linux_package.sh --deb FILE --tgz FILE --nazm-dir DIR" \
       "[--nazm-revision REV] [--embedded-mode opt-in|default]" >&2
  exit 2
}

deb=""
tgz=""
nazm_dir=""
nazm_revision=""
embedded_mode="default"
while [[ $# -gt 0 ]]; do
  case "$1" in
    --deb) deb="$2"; shift 2 ;;
    --tgz) tgz="$2"; shift 2 ;;
    --nazm-dir) nazm_dir="$2"; shift 2 ;;
    --nazm-revision) nazm_revision="$2"; shift 2 ;;
    --embedded-mode) embedded_mode="$2"; shift 2 ;;
    *) usage ;;
  esac
done
[[ -f "$deb" && -f "$tgz" && -d "$nazm_dir" ]] || usage
[[ "$embedded_mode" == "opt-in" || "$embedded_mode" == "default" ]] || usage

fail() { echo "FAIL: $*" >&2; exit 1; }

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
deb="$(cd "$(dirname "$deb")" && pwd)/$(basename "$deb")"
tgz="$(cd "$(dirname "$tgz")" && pwd)/$(basename "$tgz")"
nazm_dir="$(cd "$nazm_dir" && pwd)"
[[ -x "$nazm_dir/نظم" ]] || fail "Nazm Arabic command was not found in $nazm_dir"
[[ "$(id -u)" == 0 ]] || fail "run as root; the test installs and removes a system package"
command -v baa >/dev/null && fail "Baa is already installed on this machine"
command -v gcc >/dev/null && fail "a C toolchain is already installed; this is not a clean machine"

echo "== digest"
for package in "$deb" "$tgz"; do
  (cd "$(dirname "$package")" && sha256sum --check "$(basename "$package").sha256")
done

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
project="$work/مشروع تجريبي"
mkdir -p "$project"
cp "$root/examples/hello_world.باء" "$project/مرحبا.باء"
cp "$root/examples/math_and_format.باء" "$project/مكتبة.باء"
base_path="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"

# Build one program and require a non-executable stack: the linker must not
# have had to assume one, and the program's GNU_STACK segment must be RW.
build_program() {
  local program="${*: -1}"
  "$@" 2>&1 | tee "$program.log"
  if grep -E -i 'executable stack|\.note\.GNU-stack' "$program.log" >/dev/null; then
    fail "the linker warned about an executable stack for $(basename "$program")"
  fi
  readelf -l -W "$program" | grep -E 'GNU_STACK .* RW +0x' >/dev/null \
    || fail "$(basename "$program") was not linked with a non-executable stack"
}

# Compile, link, and run the two programs with the given compiler. The second
# one includes the standard library, which must be found without BAA_HOME.
build_and_run() {
  local baa="$1" out="$2"
  mkdir -p "$out"
  (
    cd "$project"
    export PATH="$nazm_dir:$base_path"
    unset BAA_HOME BAA_STDLIB BAA_NAZM
    "$baa" --version
    build_program "$baa" مرحبا.باء -o "$out/مرحبا"
    "$out/مرحبا"
    build_program "$baa" مكتبة.باء -o "$out/مكتبة"
    "$out/مكتبة"
    build_program "$baa" --assembler=gas مرحبا.باء -o "$out/مرحبا-gas"
    "$out/مرحبا-gas"
    # --nazm-path is the per-invocation rollback to the separate Nazm process.
    # A missing executable there is exit 4, never a fallback.
    build_program "$baa" --nazm-path="$nazm_dir/نظم" مرحبا.باء -o "$out/مرحبا-نظم"
    "$out/مرحبا-نظم"
    local status=0
    "$baa" --nazm-path="$out/نظم-غائب" مرحبا.باء -o "$out/غائب" \
      >"$out/absent-nazm.log" 2>&1 || status=$?
    [[ "$status" == 4 ]] || { cat "$out/absent-nazm.log" >&2; fail "Baa returned $status instead of 4 for a missing --nazm-path"; }
    [[ ! -e "$out/غائب" ]] || fail "Baa produced output although --nazm-path named a missing Nazm"
  )
}

echo "== install the .deb"
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq "$deb" >/dev/null
version="$(dpkg-query --show --showformat='${Version}' baa)"
[[ "$(basename "$deb")" == "baa-$version-"* ]] || fail "installed version $version does not match $(basename "$deb")"
for required in \
  /usr/bin/baa \
  /usr/lib/baa/libbaa_runtime.a \
  /usr/share/baa/stdlib/baalib.baahd \
  /usr/share/baa/stdlib/المكتبة_القياسية.رأسباء; do
  [[ -f "$required" ]] || fail "the package did not install $required"
done
# The assembler is linked into baa; Nazm's own files belong to Nazm's package.
if dpkg -L baa | grep -E -i 'nazm|نظم' | grep -v '^/usr/share/baa/' >/dev/null; then
  fail "the package installs Nazm files outside Baa's own documentation"
fi
command -v gcc >/dev/null || fail "the package did not bring the host linker it needs"

# No Nazm executable is reachable in this section: only the assembler linked
# into the installed compiler can produce objects.
echo "== the embedded Nazm is identified and works without a Nazm executable"
embedded_line="$(env -u BAA_NAZM PATH="$base_path" baa --version \
  | grep -E '^Embedded Nazm [^ ]+ \([A-Za-z0-9._-]+\), (opt-in|default)$' || true)"
[[ -n "$embedded_line" ]] || fail "Baa does not report an embedded Nazm in --version"
echo "$embedded_line"
[[ "$embedded_line" == *", $embedded_mode" ]] \
  || fail "Baa reports '$embedded_line'; expected the $embedded_mode mode"
if [[ -n "$nazm_revision" ]]; then
  [[ "$embedded_line" == *"($nazm_revision), "* ]] \
    || fail "Baa reports '$embedded_line'; expected Nazm $nazm_revision"
fi
(
  cd "$project"
  export PATH="$base_path"
  unset BAA_HOME BAA_STDLIB BAA_NAZM
  build_program baa --نظم-داخل-العملية مرحبا.باء -o "$work/مضمن"
  "$work/مضمن"
)

status=0
(cd "$project" && env -u BAA_NAZM PATH="$base_path" baa مرحبا.باء -o "$work/بدون-نظم") \
  >"$work/missing-nazm.log" 2>&1 || status=$?
if [[ "$embedded_mode" == "default" ]]; then
  echo "== the default build needs no Nazm executable"
  [[ "$status" == 0 ]] || { cat "$work/missing-nazm.log" >&2; fail "Baa returned $status without a Nazm executable although its embedded Nazm is the default"; }
  "$work/بدون-نظم"
else
  echo "== missing Nazm is reported, not worked around"
  [[ "$status" == 4 ]] || { cat "$work/missing-nazm.log" >&2; fail "Baa returned $status instead of 4 when Nazm was missing"; }
  [[ -s "$work/missing-nazm.log" ]] || fail "Baa did not explain that Nazm was missing"
  [[ ! -e "$work/بدون-نظم" ]] || fail "Baa produced output even though Nazm was unavailable"
fi

echo "== compile and run with the installed package"
build_and_run /usr/bin/baa "$work/من الحزمة"

echo "== remove the .deb"
apt-get remove -y -qq baa >/dev/null
for leftover in /usr/bin/baa /usr/lib/baa /usr/share/baa; do
  [[ ! -e "$leftover" ]] || fail "removing the package left $leftover"
done
if dpkg-query --show --showformat='${db:Status-Status}' baa 2>/dev/null | grep -q '^installed$'; then
  fail "the package is still registered as installed"
fi

echo "== compile and run from the relocated .tar.gz"
prefix="$work/بادئة باء"
mkdir -p "$prefix"
tar -xzf "$tgz" -C "$prefix"
relocated="$(find "$prefix" -type f -path '*/bin/baa' -print -quit)"
[[ -n "$relocated" ]] || fail "the archive contains no bin/baa"
echo "archive layout: ${relocated#"$prefix/"}"
build_and_run "$relocated" "$work/من الأرشيف"

echo "Baa Linux package contract passed."
