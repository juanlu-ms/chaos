#!/bin/bash
set -e

cd "$(dirname "$0")"

CHAOS_BIN="${CHAOS_BIN:-../build/dev-linux-clang/orchestrator/chaos}"
NETWORK="chaos-defense-net"

cleanup() {
  echo ""
  echo "Limpiando..."
  docker rm -f chaos-demo-api chaos-demo-downstream 2>/dev/null || true
  docker network rm "$NETWORK" 2>/dev/null || true
  echo "Hecho."
}
trap cleanup ERR

echo "=================================================="
echo " CHAOS - Demo de Defensa del TFG"
echo "=================================================="

echo ""
echo "[1/7] Construyendo imagen de los contenedores demo..."
docker build -t chaos-defense-target ./demo-target

echo ""
echo "[2/7] Creando red Docker personalizada..."
docker network rm -f "$NETWORK" 2>/dev/null || true
docker network create "$NETWORK"

echo ""
echo "[3/7] Lanzando contenedor downstream (chaos-demo-downstream)..."
docker rm -f chaos-demo-downstream 2>/dev/null || true
docker run -d \
  --name chaos-demo-downstream \
  --network "$NETWORK" \
  --cap-add=NET_ADMIN \
  -e SERVER_SCRIPT=server_downstream.py \
  -e DOWNSTREAM_PORT=8001 \
  -p 8001:8001 \
  chaos-defense-target

echo "    Esperando que downstream este listo..."
sleep 2

echo ""
echo "[4/7] Lanzando contenedor API (chaos-demo-api)..."
docker rm -f chaos-demo-api 2>/dev/null || true
docker run -d \
  --name chaos-demo-api \
  --network "$NETWORK" \
  --cap-add=NET_ADMIN \
  --sysctl net.ipv4.tcp_syncookies=0 \
  --sysctl net.ipv4.tcp_max_syn_backlog=16 \
  --sysctl net.core.somaxconn=16 \
  -e SERVER_SCRIPT=server.py \
  -e DOWNSTREAM_URL="http://chaos-demo-downstream:8001/data" \
  -e DOWNSTREAM_TIMEOUT="5.0" \
  -p 8000:8000 \
  chaos-defense-target

echo "    Esperando que API este listo..."
sleep 2

echo ""
echo "[5/7] Activando modo hairpin en el veth de la API..."
echo "    (necesario para que packet_flood, que se auto-inyecta via AF_PACKET,"
echo "     no sea descartado por el bridge como un bucle L2)"
command -v bridge >/dev/null 2>&1 || { echo "    ERROR: falta el comando 'bridge' (paquete iproute2)."; exit 1; }
API_IFLINK="$(docker exec chaos-demo-api cat /sys/class/net/eth0/iflink)"
API_VETH="$(ip -o link | awk -F': ' -v idx="$API_IFLINK" '$1==idx{split($2,a,"@"); print a[1]}')"
if [ -z "$API_VETH" ]; then
  echo "    ERROR: no se pudo localizar el veth del host para chaos-demo-api (iflink=$API_IFLINK)."
  exit 1
fi
sudo bridge link set dev "$API_VETH" hairpin on
echo "    Hairpin activado en $API_VETH"

echo ""
echo "[6/7] Verificando conectividad (via Docker exec)..."
echo -n "    API /ping: "
docker exec chaos-demo-api python3 -c "import urllib.request; print(urllib.request.urlopen('http://127.0.0.1:8000/ping',timeout=3).read().decode())" 2>/dev/null | python3 -c "import sys,json; print(json.load(sys.stdin)['status'])" 2>/dev/null || echo "(inaccesible via localhost, usando API interna)"
echo -n "    Downstream /ping: "
docker exec chaos-demo-downstream python3 -c "import urllib.request; print(urllib.request.urlopen('http://127.0.0.1:8001/ping',timeout=3).read().decode())" 2>/dev/null | python3 -c "import sys,json; print(json.load(sys.stdin)['status'])" 2>/dev/null || echo "(inaccesible via localhost, usando API interna)"
echo -n "    API /call-downstream: "
docker exec chaos-demo-api python3 -c "import urllib.request; print(urllib.request.urlopen('http://127.0.0.1:8000/call-downstream',timeout=5).read().decode())" 2>/dev/null | python3 -c "import sys,json; print(json.load(sys.stdin)['status'])" 2>/dev/null || echo "(inaccesible via localhost, usando API interna)"

echo ""
echo "[7/7] Pre-fugando memoria en el API (30MB)..."
docker exec chaos-demo-api python3 -c "
import urllib.request
for i in range(3):
    urllib.request.urlopen('http://127.0.0.1:8000/allocate', timeout=3)
    print(f'    +10MB (total: {(i+1)*10}MB)')
"

echo ""
echo "=================================================="
echo " Contenedores listos."
echo ""
echo " Ejecuta los manifiestos con:"
echo "   sudo $CHAOS_BIN run examples/defense-demo/01-gamegrid-flood.json"
echo "   sudo $CHAOS_BIN run examples/defense-demo/02-mediwatch-memory-cap.json"
echo "   sudo $CHAOS_BIN run examples/defense-demo/03-payflow-cascade.json"
echo ""
echo " IMPORTANTE: El Caso 2 (memory_cap) MATA el contenedor API (OOM)."
echo "   Tras ejecutar el Caso 2, recrea los contenedores con:"
echo "     $0"
echo ""
echo " O usa la Web UI:"
echo "   sudo $CHAOS_BIN serve --port 8080"
echo "   # Abre http://127.0.0.1:8080"
echo " NOTA: Los contenedores se limpian automaticamente si el script falla."
echo " Para limpiar manualmente tras la demo:"
echo "   docker rm -f chaos-demo-api chaos-demo-downstream"
echo "   docker network rm $NETWORK"
echo "=================================================="
