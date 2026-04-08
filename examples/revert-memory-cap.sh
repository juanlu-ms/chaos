#!/usr/bin/env bash
set -euo pipefail

CONTAINER_NAME="${1:-${CONTAINER_NAME:-chaos-demo-1}}"

if ! command -v docker >/dev/null 2>&1; then
  echo "Error: docker no esta disponible en PATH." >&2
  exit 1
fi

if ! docker inspect "$CONTAINER_NAME" >/dev/null 2>&1; then
  echo "Error: el contenedor '$CONTAINER_NAME' no existe." >&2
  exit 1
fi

show_limits() {
  local label="$1"
  docker inspect "$CONTAINER_NAME" \
    --format "$label -> Memory={{.HostConfig.Memory}} bytes | MemorySwap={{.HostConfig.MemorySwap}} bytes | State={{.State.Status}}"
}

write_max_value() {
  local path="$1"
  if [[ -w "$path" ]]; then
    echo max > "$path"
    return 0
  fi

  if command -v sudo >/dev/null 2>&1; then
    echo max | sudo tee "$path" >/dev/null
    return 0
  fi

  echo "No hay permisos para escribir en $path y sudo no esta disponible." >&2
  return 1
}

read_value() {
  local path="$1"
  if [[ -r "$path" ]]; then
    cat "$path"
    return 0
  fi

  if command -v sudo >/dev/null 2>&1; then
    sudo cat "$path"
    return 0
  fi

  return 1
}

remove_via_cgroup() {
  local pid
  local cgroup_rel
  local cgroup_path
  local cid_full
  local memory_max_file
  local memory_swap_max_file
  local effective_memory_max
  local effective_swap_max

  pid="$(docker inspect "$CONTAINER_NAME" --format '{{.State.Pid}}')"
  if [[ -z "$pid" || "$pid" == "0" ]]; then
    echo "No se pudo obtener PID del contenedor para ajuste por cgroup." >&2
    return 1
  fi

  if [[ ! -r "/proc/$pid/cgroup" ]]; then
    echo "No se puede leer /proc/$pid/cgroup desde este entorno." >&2
    cid_full="$(docker inspect "$CONTAINER_NAME" --format '{{.Id}}')"
    echo "Ejecuta esto en el host (fuera del devcontainer):" >&2
    echo "  PID=\$(docker inspect $CONTAINER_NAME --format '{{.State.Pid}}')" >&2
    echo "  CG=\$(awk -F: '\$1==\"0\"{print \$3}' /proc/\$PID/cgroup)" >&2
    echo "  echo max | sudo tee /sys/fs/cgroup\$CG/memory.max" >&2
    echo "  echo max | sudo tee /sys/fs/cgroup\$CG/memory.swap.max" >&2
    echo "  docker inspect $CONTAINER_NAME --format 'Memory={{.HostConfig.Memory}} MemorySwap={{.HostConfig.MemorySwap}}'" >&2
    echo "Container ID: $cid_full" >&2
    return 1
  fi

  # cgroup v2: linea 0::/...
  cgroup_rel="$(awk -F: '$1=="0" {print $3}' "/proc/$pid/cgroup" | head -n1)"
  if [[ -z "$cgroup_rel" ]]; then
    cgroup_rel="$(awk -F: 'NR==1{print $3}' "/proc/$pid/cgroup")"
  fi

  cgroup_path="/sys/fs/cgroup${cgroup_rel}"
  memory_max_file="$cgroup_path/memory.max"
  memory_swap_max_file="$cgroup_path/memory.swap.max"

  if [[ ! -e "$memory_max_file" ]]; then
    echo "No existe $memory_max_file." >&2
    return 1
  fi

  echo "Eliminando limite de memoria via cgroup en $cgroup_path..."
  write_max_value "$memory_max_file"

  if [[ -e "$memory_swap_max_file" ]]; then
    write_max_value "$memory_swap_max_file"
  fi

  effective_memory_max="$(read_value "$memory_max_file" || echo "<sin_acceso>")"
  effective_swap_max="$(read_value "$memory_swap_max_file" || echo "<sin_acceso>")"

  echo "Limite efectivo cgroup -> memory.max=$effective_memory_max memory.swap.max=$effective_swap_max"

  return 0
}

echo "Revirtiendo limites de memoria para: $CONTAINER_NAME"
show_limits "Antes"

if ! remove_via_cgroup; then
  echo "Error: no se pudo eliminar el limite via cgroup." >&2
  exit 2
fi

show_limits "Despues"
