#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

BUILD_BINARY="${PROJECT_ROOT}/build/backend/schoolmanager_backend"
RUN_ROOT="${SM_RUN_ROOT:-${PROJECT_ROOT}/.local/schoolmanager}"
BIN_DIR="${RUN_ROOT}/bin"
DATA_DIR="${SM_DATA_ROOT:-${RUN_ROOT}/data}"
OPENAPI_PATH="${SM_OPENAPI_PATH:-${PROJECT_ROOT}/backend/openapi/openapi.json}"
HOST="${SM_HOST:-}"
PORT="${SM_PORT:-}"
DB_POOL_SIZE="${SM_DB_POOL_SIZE:-64}"
TARGET_BINARY="${BIN_DIR}/schoolmanager_backend"

if [[ ! -x "${BUILD_BINARY}" ]]; then
  echo "Backend binary not found: ${BUILD_BINARY}" >&2
  echo "Build it first: cmake -S . -B build && cmake --build build -j2" >&2
  exit 1
fi

if [[ ! -f "${OPENAPI_PATH}" ]]; then
  echo "OpenAPI file not found: ${OPENAPI_PATH}" >&2
  exit 1
fi

mkdir -p "${BIN_DIR}" "${DATA_DIR}"
cp "${BUILD_BINARY}" "${TARGET_BINARY}"
chmod +x "${TARGET_BINARY}"

echo "Backend binary: ${TARGET_BINARY}"
echo "Data root:      ${DATA_DIR}"
echo "OpenAPI path:   ${OPENAPI_PATH}"
if [[ -n "${HOST}" || -n "${PORT}" ]]; then
  echo "Listening:      http://${HOST:-<backend default>}:${PORT:-<backend default>}"
else
  echo "Listening:      backend default"
fi

cd "${RUN_ROOT}"
ENV_ARGS=(
  "SM_DATA_ROOT=${DATA_DIR}"
  "SM_OPENAPI_PATH=${OPENAPI_PATH}"
  "SM_DB_POOL_SIZE=${DB_POOL_SIZE}"
)

if [[ -n "${HOST}" ]]; then
  ENV_ARGS+=("SM_HOST=${HOST}")
fi

if [[ -n "${PORT}" ]]; then
  ENV_ARGS+=("SM_PORT=${PORT}")
fi

exec env \
  "${ENV_ARGS[@]}" \
  "${TARGET_BINARY}"
