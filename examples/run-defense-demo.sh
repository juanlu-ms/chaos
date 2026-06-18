#!/bin/bash
set -e

cd "$(dirname "$0")"

CHAOS_BIN="${CHAOS_BIN:-../build/dev-linux-clang/orchestrator/chaos}"
NETWORK="chaos-defense-net"

echo "=================================================="
echo " CHAOS - Demo de Defensa del TFG"
echo "=================================================="

echo ""
echo "[1/5] Construyendo imagen de los contenedores demo..."
docker build -t chaos-defense-target ./demo-target

echo ""
echo "[2/5] Creando red Docker personalizada..."
docker network rm -f "$NETWORK" 2>/dev/null || true
docker network create "$NETWORK"

echo ""
echo "[3/5] Lanzando contenedor downstream (chaos-demo-downstream)..."
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
echo "[3/5] Lanzando contenedor API (chaos-demo-api)..."
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
echo "[4/5] Verificando conectividad..."
echo -n "    API /ping: "
curl -s http://127.0.0.1:8000/ping | python3 -c "import sys,json; print(json.load(sys.stdin)['status'])"
echo -n "    Downstream /ping: "
curl -s http://127.0.0.1:8001/ping | python3 -c "import sys,json; print(json.load(sys.stdin)['status'])"
echo -n "    API /call-downstream: "
curl -s http://127.0.0.1:8000/call-downstream | python3 -c "import sys,json; print(json.load(sys.stdin)['status'])"

echo ""
echo "[5/5] Pre-fugando memoria en el API (50MB)..."
for i in $(seq 1 5); do
  curl -s http://127.0.0.1:8000/allocate > /dev/null
  echo "    +10MB (total: ${i}0MB)"
done

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
echo ""
echo " Para limpiar:"
echo "   docker rm -f chaos-demo-api chaos-demo-downstream"
echo "   docker network rm $NETWORK"
echo "=================================================="
