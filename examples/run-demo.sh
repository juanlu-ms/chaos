#!/bin/bash
set -e

# Change to the examples directory regardless of where script is called from
cd "$(dirname "$0")"

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo " CHAOS ► INTERACTIVE DEMO SETUP"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

echo "1. Building Demo Target Application..."
docker build -t chaos-demo-target ./demo-target > /dev/null

echo "2. Cleaning up old containers..."
docker rm -f chaos-demo-1 > /dev/null 2>&1 || true

echo "3. Starting target container (chaos-demo-1)..."
# NET_ADMIN is required for tc and iptables network perturbations
docker run -d --name chaos-demo-1 --cap-add=NET_ADMIN chaos-demo-target > /dev/null

echo ""
echo "✅ Target application is running in the background."
echo ""
echo "▶ NEXT STEPS"
echo ""
echo "1. Compile the orchestrator if you haven't already:"
echo "   bash scripts/cmake-local.sh all"
echo ""
echo "2. Start the CHAOS Web Dashboard (from the repository root):"
echo "   ./build/debug/orchestrator/chaos serve --port 8080"
echo ""
echo "3. Open your browser to http://127.0.0.1:8080"
echo "   - View live logs for 'chaos-demo-1' by clicking the Logs button."
echo "   - Copy a JSON manifest from examples/demo/"
echo "     (like examples/demo/01-resource-exhaustion.json)"
echo "   - Paste it into the 'Run Chaos Scenario' panel and execute!"
echo ""
echo "Watch the container logs and State Dashboard react in real-time."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
