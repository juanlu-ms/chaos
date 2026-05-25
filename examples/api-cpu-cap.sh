#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://127.0.0.1:8000}"
ACTION="${1:-run}"
COUNT="${2:-1}"
SECONDS="${SECONDS:-8}"
WORKERS="${WORKERS:-2}"

if ! [[ "$COUNT" =~ ^[0-9]+$ ]] || [[ "$COUNT" -lt 1 ]]; then
  echo "Uso: $0 [run|start|stop] [count>=1]"
  exit 1
fi

case "$ACTION" in
  run)
    for ((i = 1; i <= COUNT; i++)); do
      printf "[%d/%d] " "$i" "$COUNT"
      curl -fsS "$BASE_URL/cpu?seconds=$SECONDS&workers=$WORKERS"
      echo
    done
    ;;
  start)
    curl -fsS "$BASE_URL/cpu/start?workers=$WORKERS"
    echo
    ;;
  stop)
    curl -fsS "$BASE_URL/cpu/stop"
    echo
    ;;
  *)
    echo "Uso: $0 [run|start|stop] [count>=1]"
    exit 1
    ;;
esac
