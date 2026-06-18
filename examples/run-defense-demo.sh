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
trap cleanup EXIT

echo "=================================================="
echo " CHAOS - Demo de Defensa del TFG"
echo "=================================================="

echo ""
echo "[1/6] Construyendo imagen de los contenedores demo..."
docker build -t chaos-defense-target ./demo-target

echo ""
echo "[2/6] Creando red Docker personalizada..."
docker network rm -f "$NETWORK" 2>/dev/null || true
docker network create "$NETWORK"

echo ""
echo "[3/6] Lanzando contenedor downstream (chaos-demo-downstream)..."
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
echo "[4/6] Lanzando contenedor API (chaos-demo-api)..."
docker rm -f chaos-demo-api 2>/dev/null || true
docker run -d \
  --name chaos-demo-api \
  --network "$NETWORK" \
  --cap-add=NET_ADMIN \
  -e SERVER_SCRIPT=server.py \
  -e DOWNSTREAM_URL="http://chaos-demo-downstream:8001/data" \
  -e DOWNSTREAM_TIMEOUT="5.0" \
  -p 8000:8000 \
  chaos-defense-target

echo "    Esperando que API este listo..."
sleep 2

echo ""
echo "[5/6] Verificando conectividad (via Docker exec)..."
echo -n "    API /ping: "
docker exec chaos-demo-api python3 -c "import urllib.request; print(urllib.request.urlopen('http://127.0.0.1:8000/ping',timeout=3).read().decode())" 2>/dev/null | python3 -c "import sys,json; print(json.load(sys.stdin)['status'])" 2>/dev/null || echo "(inaccesible via localhost, usando API interna)"
echo -n "    Downstream /ping: "
docker exec chaos-demo-downstream python3 -c "import urllib.request; print(urllib.request.urlopen('http://127.0.0.1:8001/ping',timeout=3).read().decode())" 2>/dev/null | python3 -c "import sys,json; print(json.load(sys.stdin)['status'])" 2>/dev/null || echo "(inaccesible via localhost, usando API interna)"
echo -n "    API /call-downstream: "
docker exec chaos-demo-api python3 -c "import urllib.request; print(urllib.request.urlopen('http://127.0.0.1:8000/call-downstream',timeout=5).read().decode())" 2>/dev/null | python3 -c "import sys,json; print(json.load(sys.stdin)['status'])" 2>/dev/null || echo "(inaccesible via localhost, usando API interna)"

echo ""
echo "[6/6] Pre-fugando memoria en el API (30MB)..."
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
echo "   sudo $CHAOS_BIN run examples/defense-demo/01-mediwatch-memory-cap.json"
echo "   sudo $CHAOS_BIN run examples/defense-demo/02-payflow-cascade.json"
echo "   sudo $CHAOS_BIN run examples/defense-demo/03-gamegrid-flood.json"
echo ""
echo " O usa la Web UI:"
echo "   sudo $CHAOS_BIN serve --port 8080"
echo "   # Abre http://127.0.0.1:8080"
echo " Limpieza automatica al salir del script (trap EXIT)."
echo " Para limpiar manualmente si cancelaste con Ctrl+C:"
echo "   docker rm -f chaos-demo-api chaos-demo-downstream"
echo "   docker network rm $NETWORK"
echo "=================================================="
