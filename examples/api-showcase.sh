#!/usr/bin/env bash
set -euo pipefail

PORT="${PORT:-8000}"
BASE_URL="${BASE_URL:-}"
CONTAINER_NAME="${CONTAINER_NAME:-chaos-demo-1}"
LOG_LINES="${LOG_LINES:-25}"
CPU_COUNT="${CPU_COUNT:-3}"
ALLOC_COUNT="${ALLOC_COUNT:-4}"
TIMEOUT="${TIMEOUT:-10}"
SLEEP_BETWEEN="${SLEEP_BETWEEN:-0.15}"

C_RESET="\033[0m"
C_BOLD="\033[1m"
C_GREEN="\033[32m"
C_RED="\033[31m"
C_YELLOW="\033[33m"
C_CYAN="\033[36m"

ok_count=0
fail_count=0
latencies_ok=""

draw_line() {
  printf '%*s\n' "${COLUMNS:-80}" '' | tr ' ' '-'
}

print_header() {
  draw_line
  printf "%bCHAOS API SHOWCASE (live)%b\n" "$C_BOLD$C_CYAN" "$C_RESET"
  draw_line
}

pick_base_url() {
  if [[ -n "$BASE_URL" ]]; then
    return 0
  fi

  local candidates=(
    "http://127.0.0.1:${PORT}"
    "http://localhost:${PORT}"
    "http://host.docker.internal:${PORT}"
  )

  for candidate in "${candidates[@]}"; do
    if curl -fsS --max-time 2 "${candidate}/ping" >/dev/null 2>&1; then
      BASE_URL="$candidate"
      return 0
    fi
  done

  printf "%bNo pude detectar BASE_URL automaticamente.%b\n" "$C_RED" "$C_RESET"
  echo "Ejemplo: BASE_URL=http://host.docker.internal:8001 ./api-showcase.sh"
  exit 1
}

json_pretty() {
  local body="$1"
  if command -v jq >/dev/null 2>&1; then
    echo "$body" | jq . 2>/dev/null || echo "$body"
  else
    echo "$body"
  fi
}

progress_bar() {
  local current="$1"
  local total="$2"
  local width=26
  local filled=$(( current * width / total ))
  local empty=$(( width - filled ))
  printf "\r[%s%s] %d/%d" "$(printf '#%.0s' $(seq 1 "$filled"))" "$(printf '.%.0s' $(seq 1 "$empty"))" "$current" "$total"
}

record_latency() {
  local latency="$1"
  latencies_ok+="${latency}"$'\n'
}

ascii_latency_chart() {
  if [[ -z "$latencies_ok" ]]; then
    echo "No hay latencias OK para graficar."
    return
  fi

  echo ""
  printf "%bLatency Chart (ms)%b\n" "$C_BOLD" "$C_RESET"

  awk '
    BEGIN { max=0; i=0 }
    NF {
      i++
      v=$1*1000
      values[i]=v
      if (v>max) max=v
    }
    END {
      if (i==0) {
        print "(sin datos)"
        exit
      }
      if (max < 1) max = 1
      for (j=1; j<=i; j++) {
        bars=int((values[j]/max)*40)
        if (bars<1) bars=1
        line=""
        for (k=1; k<=bars; k++) line=line"="
        printf("%2d | %-40s %.2f\n", j, line, values[j])
      }
    }
  ' <<< "$latencies_ok"
}

print_container_logs() {
  local phase="$1"

  echo ""
  printf "%bLogs del contenedor (%s)%b\n" "$C_BOLD" "$phase" "$C_RESET"

  if ! command -v docker >/dev/null 2>&1; then
    echo "docker no esta disponible en este entorno."
    return
  fi

  if ! docker ps -a --format '{{.Names}}' | grep -Fxq "$CONTAINER_NAME"; then
    echo "No se encontro el contenedor '$CONTAINER_NAME'."
    return
  fi

  docker logs --tail "$LOG_LINES" "$CONTAINER_NAME" 2>&1 | sed 's/^/  | /'
}

call_api() {
  local endpoint="$1"
  local label="$2"

  local tmp_body
  tmp_body="$(mktemp)"

  local out
  out="$(curl -sS -m "$TIMEOUT" -o "$tmp_body" -w '%{http_code} %{time_total}' "${BASE_URL}${endpoint}" || true)"

  local code=""
  local latency=""
  code="$(awk '{print $1}' <<< "$out")"
  latency="$(awk '{print $2}' <<< "$out")"

  if [[ "$code" =~ ^2[0-9][0-9]$ ]]; then
    ok_count=$((ok_count + 1))
    record_latency "$latency"
    printf "%b[OK]%b   %-24s code=%s time=%ss\n" "$C_GREEN" "$C_RESET" "$label" "$code" "$latency"
  else
    fail_count=$((fail_count + 1))
    printf "%b[FAIL]%b %-24s code=%s time=%s\n" "$C_RED" "$C_RESET" "$label" "${code:-n/a}" "${latency:-n/a}"
  fi

  local body
  body="$(cat "$tmp_body")"
  rm -f "$tmp_body"

  if [[ -n "$body" ]]; then
    printf "%bResponse:%b\n" "$C_YELLOW" "$C_RESET"
    json_pretty "$body" | sed 's/^/  /'
  fi
}

run_batch() {
  local title="$1"
  local endpoint="$2"
  local count="$3"

  echo ""
  printf "%b%s%b\n" "$C_BOLD" "$title" "$C_RESET"
  for i in $(seq 1 "$count"); do
    progress_bar "$i" "$count"
    sleep "$SLEEP_BETWEEN"
    printf "\r"
    call_api "$endpoint" "${title} (${i}/${count})"
  done
}

print_summary() {
  local total
  total=$((ok_count + fail_count))

  local stats
  if [[ -n "$latencies_ok" ]]; then
    stats="$(awk '
      BEGIN {sum=0; n=0}
      NF {sum+=$1; n++; if(n==1 || $1<min) min=$1; if(n==1 || $1>max) max=$1}
      END {
        if (n==0) {print "0 0 0"; exit}
        printf "%.4f %.4f %.4f", sum/n, min, max
      }
    ' <<< "$latencies_ok")"
  else
    stats="0 0 0"
  fi

  local avg min max
  avg="$(awk '{print $1}' <<< "$stats")"
  min="$(awk '{print $2}' <<< "$stats")"
  max="$(awk '{print $3}' <<< "$stats")"

  echo ""
  draw_line
  printf "%bResumen%b\n" "$C_BOLD" "$C_RESET"
  echo "BASE_URL:         $BASE_URL"
  echo "Total llamadas:   $total"
  echo "OK:               $ok_count"
  echo "FAIL:             $fail_count"
  echo "Latencia OK avg:  ${avg}s"
  echo "Latencia OK min:  ${min}s"
  echo "Latencia OK max:  ${max}s"
  draw_line

  ascii_latency_chart
}

main() {
  print_header
  pick_base_url
  echo "Usando BASE_URL=$BASE_URL"
  echo "Contenedor logs: $CONTAINER_NAME (ultimas $LOG_LINES lineas)"
  echo ""

  print_container_logs "inicio"

  call_api "/ping" "Health inicial"
  run_batch "CPU stress" "/cpu" "$CPU_COUNT"
  run_batch "Memory allocation" "/allocate" "$ALLOC_COUNT"
  call_api "/ping" "Health final"

  print_container_logs "final"

  print_summary
}

main
