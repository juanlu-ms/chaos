# Casos de Estudio — Demo de Defensa del TFG

## Arquitectura de la Demo

La demo utiliza dos contenedores Docker comunicados via una red personalizada (`chaos-defense-net`):

| Contenedor | Rol | Puerto | Endpoints |
|-----------|-----|--------|-----------|
| `chaos-demo-api` | Servicio principal | 8000 | `/ping`, `/allocate`, `/cpu`, `/call-downstream` |
| `chaos-demo-downstream` | Servicio dependiente | 8001 | `/ping`, `/data` |

El servidor API expone `/call-downstream` que internamente realiza una peticion HTTP saliente hacia `chaos-demo-downstream:8001/data`. Ambos contenedores reciben `--cap-add=NET_ADMIN` para permitir la inyeccion de fallos de red.

---

## Caso 1: MediWatch — Fuga de Memoria

**Narrativa:** Una empresa de telemedicina despliega monitores de pacientes en dispositivos edge (Raspberry Pi) en una UCI. En desarrollo, las pruebas pasan con 32GB de RAM. En produccion, los dispositivos tienen 512MB y el sistema se reinicia aleatoriamente tras 8 horas de funcionamiento.

**Manifiesto:** `examples/defense-demo/02-mediwatch-memory-cap.json`

```json
{
  "test_name": "MediWatch — Memory Cap (Fuga de Memoria)",
  "target": { "id": "chaos-demo-api" },
  "duration_s": 12,
  "perturbations": [
    {
      "type": "memory_cap",
      "parameters": { "limit_bytes": "33554432" }
    }
  ],
  "expectations": [
    { "type": "container_running" },
    {
      "type": "http_status",
      "parameters": { "port": "8000", "path": "/ping", "expected_status": "200" }
    }
  ]
}
```

### Que hace CHAOS

1. El script `run-defense-demo.sh` pre-fuga 30MB en el API llamando 3 veces a `/allocate` (10MB cada llamada)
2. CHAOS aplica el limite de 32MB via Docker API — establece `memory.limit_in_bytes` en los cgroups del contenedor
3. El contenedor ya consume ~18MB base + 30MB fugados = 48MB > 32MB de limite
4. El OOM killer del kernel Linux termina el proceso inmediatamente al aplicar el limite
5. El contenedor se lanza sin politica de reinicio, asi que no vuelve: queda `exited` para el resto del run

### Resultados esperados

Las dos expectativas describen el servicio que MediWatch *cree* tener: un contenedor vivo que responde. El run demuestra que no lo es.

| Expectativa | Resultado | Razon |
|-------------|-----------|-------|
| `container_running` | FAIL | El OOM killer se lleva al proceso y el contenedor pasa a `exited` |
| `http_status 200` en `/ping` | FAIL | El contenedor esta muerto, no puede responder |

> **Nota sobre la inyeccion.** Al aplicar un limite por debajo del uso actual, la API de Docker puede
> devolver HTTP 500 aunque el limite si se haya aplicado: con cgroup v2 y el driver `systemd`, el OOM
> killer destruye el cgroup del contenedor antes de que runc termine de leerlo. CHAOS lo detecta
> —comprueba si el kernel mato al contenedor por memoria— y cuenta la perturbacion como aplicada, de
> modo que el veredicto lo deciden las expectativas y no un fallo de inyeccion. Es intermitente:
> depende de si systemd ha borrado ya el scope cuando runc lo abre.

### Concepto enseñado

**Cgroups de Linux y OOM killer.** Cuando un proceso excede `memory.limit_in_bytes`, el kernel invoca el OOM killer que envia `SIGKILL` al proceso. CHAOS comprime 8 horas de degradacion en produccion en 10 segundos de validacion local.

### Lo que el desarrollador aprende

Su aplicacion tiene una fuga de memoria invisible en entornos de desarrollo con RAM abundante. Debe cambiar su estrategia de asignacion de memoria — usar streaming o procesamiento por bloques en lugar de cargar datos completos en RAM.

---

## Caso 2: PayFlow — Fallo en Cascada

**Narrativa:** Una pasarela de pagos (PayFlow) llama a un servicio externo de deteccion de fraude via HTTP. En staging muestra 99.9% de disponibilidad. Durante el Black Friday las transacciones empiezan a caducar sin que ningun servicio aparezca como "caido". El contenedor esta `running` pero el sistema no funciona.

**Manifiesto:** `examples/defense-demo/03-payflow-cascade.json`

```json
{
  "test_name": "PayFlow — Latencia en Cascada (network_delay + cpu_cap)",
  "target": { "id": "chaos-demo-api" },
  "duration_s": 15,
  "perturbations": [
    {
      "type": "network_delay",
      "parameters": { "delay_ms": "1200" }
    },
    {
      "type": "cpu_cap",
      "parameters": { "cpu_cores": "0.5" }
    }
  ],
  "expectations": [
    { "type": "container_running" },
    {
      "type": "http_latency",
      "parameters": { "port": "8000", "path": "/call-downstream", "max_latency_ms": "2400" }
    }
  ]
}
```

### Que hace CHAOS

1. **`network_delay`**: ejecuta `nsenter -t <pid> -n tc qdisc replace dev eth0 root netem delay 1200ms` dentro del network namespace del contenedor API. Esto anade 1200ms de latencia a cada paquete TCP que sale por `eth0`.

2. **`cpu_cap`**: establece `CpuQuota=50000` / `CpuPeriod=100000` via Docker API, limitando el contenedor al 50% de un nucleo de CPU.

3. **`http_latency` con validacion continua**: esta expectativa se evalua automaticamente cada 500ms durante la fase de caos (marcada como `continuous: true` por defecto). El `ObservationLoop` realiza peticiones HTTP reales al contenedor y mide la latencia.

### Por que falla la latencia

La ruta de una peticion `GET /call-downstream` durante el caos:

```
CHAOS ──[ingress, 0ms]──> API ──[egress, +1200ms]──> Downstream ──[+50ms /data]──> API ──[egress, +1200ms]──> CHAOS
                                                                           (ingress, 0ms)
```

- **Ingress** (entrada al contenedor): sin delay — `tc netem` en el root qdisc solo afecta trafico de salida
- **Egress** (salida del contenedor): +1200ms cada vez
- La llamada al downstream (`/data`) tarda ~50ms
- Latencia total: 0 + 1200 + 50 + 1200 ≈ **2450ms**
- Con CPU al 50%, el overhead de procesamiento anade mas latencia
- `max_latency_ms: 2400` → 2450ms > 2400ms → **FAIL**

**De donde sale el umbral de 2400ms.** No es un numero arbitrario: es el *suelo* que impone la propia
perturbacion, 1200ms de egress en la llamada al downstream mas otros 1200ms de egress en la respuesta
a CHAOS. Ninguna peticion puede bajar de ahi mientras el fallo este inyectado, y todo lo demas
—el procesamiento del downstream, el `cpu_cap`— solo puede sumar. El umbral se fija por tanto en el
**mejor caso teorico alcanzable bajo el fallo**, y el sistema tampoco lo cumple: el FAIL no depende
de la carga de la maquina, esta garantizado por construccion.

### Mecanismo de validacion continua

1. `ObservationLoop::metricsThreadFn()` llama a `validation::validate()` cada 500ms durante la fase `chaos`
2. El `TargetState` incluye `container_ip` (172.18.0.3) — el cliente HTTP de `httplib` conecta directamente al contenedor
3. El fallo se registra via `SharedState::addContinuousFailure("http_latency")`
4. En `ChaosRunner::finalize()`, aunque la peticion final pase (porque el delay ya se revirtio en la fase de recuperacion), el resultado se sobreescribe: *"Passed final validation but failed mid-run continuous check"*

### Resultados esperados

| Expectativa | Resultado | Razon |
|-------------|-----------|-------|
| `container_running` | PASS | El contenedor sobrevive — el servicio no se cae, solo se degrada |
| `http_latency < 500ms` | FAIL | Latencia real ~2450ms, muy por encima del maximo de 500ms |

### Concepto enseñado

**Network namespaces de Linux** (`nsenter` + `tc netem`), **cgroups de CPU**, y **fallos en cascada entre microservicios**. Un servicio no necesita caerse para estar roto — basta con que sea lento y provoque timeouts en sus dependientes.

### Lo que el desarrollador aprende

Su API no tiene timeout configurado en la llamada HTTP saliente al servicio de fraude. Sin timeout, el hilo principal se bloquea esperando una respuesta que nunca llega, acumulando peticiones y agotando el pool de threads. Debe implementar timeouts, circuit breakers, y backpressure.

---

## Caso 3: GameGrid — Ataque DDoS

**Narrativa:** Una empresa de videojuegos (GameGrid) opera servidores multijugador que mantienen un bucle de actualizacion constante (ticks). Cada fin de semana de lanzamiento, los servidores colapsan bajo la carga de jugadores. Pero sus pruebas de carga muestran que pueden manejar 10k conexiones simultaneas. ¿Que cambia en produccion?

**Manifiesto:** `examples/defense-demo/01-gamegrid-flood.json`

```json
{
  "test_name": "GameGrid — Ataque DDoS (packet_flood + traffic_corruption)",
  "target": { "id": "chaos-demo-api" },
  "duration_s": 12,
  "perturbations": [
    {
      "type": "packet_flood",
      "parameters": { "rate": "5000", "packet_size": "512" }
    },
    {
      "type": "traffic_corruption",
      "parameters": { "corrupt_pct": "10%", "loss_pct": "10%", "duplicate_pct": "5%" }
    }
  ],
  "expectations": [
    { "type": "container_running" },
    {
      "type": "http_status",
      "parameters": { "port": "8000", "path": "/ping", "expected_status": "200" }
    },
    {
      "type": "http_latency",
      "parameters": { "port": "8000", "path": "/ping", "max_latency_ms": "100" }
    }
  ]
}
```

### Que hace CHAOS

1. **`packet_flood`**: abre un raw socket (`AF_PACKET`) dentro del network namespace del contenedor via `setns()`. Construye tramas Ethernet+IP+TCP validas dirigidas a la IP del propio contenedor con:
   - Direcciones MAC origen aleatorias
   - IPs origen suplantadas (spoofed)
   - Puertos TCP origen aleatorios
   - Puerto destino 8000
   - Payload de basura aleatoria
   
   Envia 5000 paquetes por segundo con tamano de trama de 512 bytes.

2. **`traffic_corruption`**: aplica `tc qdisc add dev eth0 root netem corrupt 10% loss 10% duplicate 5%` dentro del network namespace, corrompiendo/dropeando/duplicando paquetes legitimos.

### Efecto combinado

- La inundacion de 5000 pps con tramas de 512 bytes (~20 Mbps) satura la cola SYN del kernel y consume CPU por interrupciones
- La CPU se dispara por el procesamiento de interrupciones de red y la construccion de respuestas TCP
- El 10% de perdida + 10% de corrupcion hacen que las conexiones TCP legitimas fallen o requieran retransmisiones
- El 5% de duplicacion genera trafico adicional y confunde a la capa de aplicacion
- `http_status` y `http_latency` se validan de forma continua (cada 500ms) durante la fase de caos, por lo que un solo fallo de conexion durante el ataque marca la expectativa como fallida de forma definitiva
- El contenedor sigue `running` pero el servicio HTTP se vuelve **inalcanzable de forma intermitente y con latencia disparada** durante el ataque

### Resultados esperados

| Expectativa | Resultado | Razon |
|-------------|-----------|-------|
| `container_running` | PASS | El contenedor sobrevive al ataque volumetrico |
| `http_status 200` en `/ping` | FAIL | La saturacion de la cola SYN hace que las conexiones nuevas fallen de forma intermitente durante el ataque |
| `http_latency < 100ms` en `/ping` | FAIL | La latencia real esta muy por encima de 100ms bajo el ataque |

### Fase de recuperacion

Tras los 12 segundos de caos:
1. El hilo de flood se detiene (`stop_token` solicitado)
2. El raw socket se cierra
3. Las reglas `tc netem` se eliminan con `tc qdisc del dev eth0 root netem`
4. El `PerturbationEngine` garantiza la reversion via RAII (`waitForTeardown()`)
5. El contenedor vuelve a responder normalmente

En la Web UI, los graficos de linea temporal muestran tres zonas coloreadas:
- **Verde** (Normal): trafico y latencia normales
- **Rojo** (Caos): pico de trafico de red, CPU al 100%, latencia disparada
- **Azul** (Recuperacion): metricas volviendo a la normalidad

### Concepto enseñado

**Raw sockets** (`AF_PACKET` + `setns()`), **suplantacion de direcciones** (spoofing), y **congestion de red a nivel de kernel**. La validacion continua de `http_latency` (cada 500ms durante el caos) demuestra que un servicio puede estar *running* pero *degradado* — CHAOS expone esa diferencia midiendo la latencia real bajo ataque.

### Lo que el desarrollador aprende

Su servidor de juego no tiene proteccion contra trafico malicioso a nivel de red. Debe implementar rate limiting, SYN cookies, y posiblemente un proxy inverso con capacidades de filtrado. La recuperacion automatica funciona — el servidor vuelve a la normalidad tras el ataque — pero los jugadores experimentaron lag, desconexiones, y rubber-banding durante el incidente.

---

## Resumen

| Caso | Perturbaciones | Expectativas | Concepto Linux |
|------|---------------|-------------|----------------|
| MediWatch | `memory_cap` (32MB) | `container_running` FAIL, `http_status` FAIL | Cgroups v1/v2, OOM killer |
| PayFlow | `network_delay` (1200ms) + `cpu_cap` (0.5 cores) | `container_running` PASS, `http_latency` FAIL | Network namespaces, tc netem, cgroups CPU |
| GameGrid | `packet_flood` (5000 pps, 512B) + `traffic_corruption` (10% / 10% / 5%) | `container_running` PASS, `http_status` FAIL, `http_latency` FAIL | Raw sockets (AF_PACKET), setns, tc netem |

**Principio unificador:** La resiliencia no es una funcionalidad — es una propiedad que se valida. CHAOS permite validarla en tiempo de desarrollo, antes de que los fallos lleguen a produccion.
