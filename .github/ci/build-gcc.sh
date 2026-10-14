#!/usr/bin/env bash
set -euo pipefail

src=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
build=${GCC_BUILD_DIR:-"${RUNNER_TEMP:-"$src/.."}/gcc-ci-build"}
stage=${GCC_STAGE_DIR:-"${RUNNER_TEMP:-"$src/.."}/gcc-ci-stage"}
reports="$src/ci-results"
packages="$src/ci-packages"
mode=${GCC_BUILD_MODE:-quick}
scope=${GCC_TEST_SCOPE:-selected}
c_tests=${GCC_C_TESTS:-'dg.exp=inline*.c'}
cxx_tests=${GCC_CXX_TESTS:-'dg.exp=template/explicit*.C'}
jobs=${GCC_JOBS:-4}
test_jobs=${GCC_TEST_JOBS:-2}

validate() {
  [[ "$mode" == quick || "$mode" == bootstrap ]] || return 1
  [[ "$scope" == selected || "$scope" == full ]] || return 1
  [[ "$jobs" =~ ^[1-9][0-9]?$ && "$test_jobs" =~ ^[1-9][0-9]?$ ]] || return 1
  # A single harness=glob selector, not arbitrary make/shell/Tcl code.
  # Quotes, whitespace, dollars, semicolons and backticks are not accepted.
  local selector
  for selector in "$c_tests" "$cxx_tests"; do
    if [[ ! "$selector" =~ ^[a-zA-Z0-9_+./-]+\.exp=[a-zA-Z0-9_+./*?-]+$ ]]; then
      printf 'Invalid test selector: %s\n' "$selector" >&2
      return 1
    fi
  done
}

validate

case "${1:-}" in
  validate)
    printf 'Mode=%s; tests=%s; C=%s; C++=%s\n' "$mode" "$scope" "$c_tests" "$cxx_tests"
    ;;
  configure)
    mkdir -p "$build" "$reports"
    {
      git -C "$src" rev-parse HEAD
      uname -a
      gcc --version
      g++ --version
      runtest --version
      df -h "$build"
      printf 'Mode=%s\nScope=%s\nC=%s\nC++=%s\n' "$mode" "$scope" "$c_tests" "$cxx_tests"
    } > "$reports/environment.txt"
    bootstrap_option=--disable-bootstrap
    [[ "$mode" != bootstrap ]] || bootstrap_option=--enable-bootstrap
    cd "$build"
    "$src/configure" \
      --prefix=/opt/gcc-uaglbr \
      --enable-languages=c,c++ \
      --disable-multilib \
      --enable-checking=yes \
      "$bootstrap_option" \
      CFLAGS='-O2 -g0' CXXFLAGS='-O2 -g0' \
      CFLAGS_FOR_TARGET='-O2 -g0' CXXFLAGS_FOR_TARGET='-O2 -g0' \
      2>&1 | tee "$reports/configure.log"
    ;;
  build)
    mkdir -p "$reports"
    if [[ "$mode" == bootstrap ]]; then
      make -C "$build" -j"$jobs" bootstrap-lean \
        STAGE1_CFLAGS='-O2 -g0' BOOT_CFLAGS='-O2 -g0' \
        2>&1 | tee "$reports/build.log"
    else
      make -C "$build" -j"$jobs" 2>&1 | tee "$reports/build.log"
    fi
    ;;
  test)
    mkdir -p "$reports"
    status=0
    if [[ "$scope" == selected ]]; then
      make -C "$build/gcc" -k -j"$test_jobs" check-gcc "RUNTESTFLAGS=$c_tests" \
        2>&1 | tee "$reports/check-c.log" || status=1
      make -C "$build/gcc" -k -j"$test_jobs" check-g++ "RUNTESTFLAGS=$cxx_tests" \
        2>&1 | tee "$reports/check-cxx.log" || status=1
    else
      make -C "$build" -k -j"$test_jobs" check-gcc check-target-libstdc++-v3 \
        2>&1 | tee "$reports/check-full.log" || status=1
    fi
    python3 "$src/.github/ci/check_results.py" "$build" --scope "$scope" \
      2>&1 | tee "$reports/test-summary.txt" || status=1
    exit "$status"
    ;;
  package)
    mkdir -p "$stage" "$reports" "$packages"
    make -C "$build" -j"$jobs" install "DESTDIR=$stage" \
      2>&1 | tee "$reports/install.log"
    prefix="$stage/opt/gcc-uaglbr"
    export LD_LIBRARY_PATH="$prefix/lib64:$prefix/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    "$prefix/bin/gcc" --version | tee "$reports/compiler-version.txt"
    for optimization in 0 2; do
      "$prefix/bin/gcc" -O"$optimization" "$src/.github/ci/smoke.c" -o "$build/smoke-c"
      "$build/smoke-c"
      "$prefix/bin/g++" -std=c++17 -O"$optimization" "$src/.github/ci/smoke.cc" -o "$build/smoke-cxx"
      "$build/smoke-cxx"
    done
    commit=$(git -C "$src" rev-parse --short=12 HEAD)
    archive="gcc-uaglbr-linux-x86_64-$commit.tar.gz"
    cp "$reports/environment.txt" "$prefix/BUILD-INFO.txt"
    tar -C "$stage/opt" -czf "$packages/$archive" gcc-uaglbr
    (cd "$packages" && sha256sum "$archive" > "$archive.sha256")
    if [[ -n "${GITHUB_STEP_SUMMARY:-}" ]]; then
      # Backticks below are literal Markdown code markers.
      # shellcheck disable=SC2016
      printf '### Compiled GCC\n\n- Source commit: `%s`\n- Mode: `%s`\n- Tests: `%s`\n- Archive: `%s`\n- Target: Linux x86_64, C/C++, 64-bit only\n' \
        "$commit" "$mode" "$scope" "$archive" >> "$GITHUB_STEP_SUMMARY"
    fi
    ;;
  collect)
    mkdir -p "$reports"
    if [[ -d "$build" ]]; then
      while IFS= read -r -d '' file; do
        relative=${file#"$build/"}
        mkdir -p "$reports/details/$(dirname "$relative")"
        cp "$file" "$reports/details/$relative"
      done < <(find "$build" -type f \( -name '*.sum' -o -name '*.log' \) -print0)
    fi
    ;;
  *)
    echo 'Usage: build-gcc.sh {validate|configure|build|test|package|collect}' >&2
    exit 2
    ;;
esac
