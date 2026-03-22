#!/usr/bin/env bash
set -euo pipefail

MODE="${1:-all}"
CONFIGURE_PRESET="${CONFIGURE_PRESET:-dev-linux-clang}"
BUILD_PRESET="${BUILD_PRESET:-debug}"
TEST_PRESET="${TEST_PRESET:-test}"

resolve_vcpkg_root() {
  if [[ -n "${VCPKG_ROOT:-}" && -f "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" ]]; then
    echo "${VCPKG_ROOT}"
    return 0
  fi

  local candidates=(
    "${PWD}/vcpkg"
    "/vcpkg"
  )

  local candidate
  for candidate in "${candidates[@]}"; do
    if [[ -f "${candidate}/scripts/buildsystems/vcpkg.cmake" ]]; then
      echo "${candidate}"
      return 0
    fi
  done

  return 1
}

if ! DETECTED_VCPKG_ROOT="$(resolve_vcpkg_root)"; then
  echo "Error: no se encontró vcpkg."
  echo "Define VCPKG_ROOT o instala vcpkg en ./vcpkg o /vcpkg."
  exit 1
fi

export VCPKG_ROOT="${DETECTED_VCPKG_ROOT}"
echo "Usando VCPKG_ROOT=${VCPKG_ROOT}"

case "${MODE}" in
  configure)
    cmake --preset "${CONFIGURE_PRESET}"
    ;;
  build)
    cmake --build --preset "${BUILD_PRESET}"
    ;;
  test)
    ctest --preset "${TEST_PRESET}"
    ;;
  all)
    cmake --preset "${CONFIGURE_PRESET}"
    cmake --build --preset "${BUILD_PRESET}"
    ctest --preset "${TEST_PRESET}"
    ;;
  *)
    echo "Uso: bash scripts/cmake-local.sh [configure|build|test|all]"
    exit 2
    ;;
esac
