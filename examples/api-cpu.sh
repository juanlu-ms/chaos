#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://127.0.0.1:8000}"
COUNT="${1:-1}"

if ! [[ "$COUNT" =~ ^[0-9]+$ ]] || [[ "$COUNT" -lt 1 ]]; then
  echo "Uso: $0 [count>=1]"
  exit 1
fi

for ((i = 1; i <= COUNT; i++)); do
  printf "[%d/%d] " "$i" "$COUNT"
  curl -fsS "$BASE_URL/cpu"
  echo
done
