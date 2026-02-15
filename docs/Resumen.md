# CHAOS: Plataforma de Validación de Resiliencia para Contenedores (TFG 2026)

**CHAOS** es un framework de ingeniería del caos diseñado para la etapa de desarrollo de software (*Shift-Left Testing*). A diferencia de las herramientas que operan sobre clústeres en producción, CHAOS permite a los desarrolladores validar la resiliencia de sus contenedores Docker localmente, sometiéndolos a condiciones adversas como límites de CPU, inestabilidad de red o fugas de memoria durante la fase de programación.

La principal innovación del proyecto es su **arquitectura de "Caja Gris"**. Mediante un agente ligero (`CHAOS_wrapper`), el sistema no solo interrumpe contenedores desde el exterior, sino que se ejecuta dentro del entorno de la aplicación para interceptar señales y diagnósticos que el runtime de Docker generalmente no expone.

---

## Motivación

En sistemas críticos (como aquellos utilizados en telemedicina, finanzas o procesamiento en tiempo real), un fallo silencioso suele ser más peligroso que una parada total. Un contenedor puede aparecer como "Running" según el orquestador, mientras su proceso interno se encuentra en estado de *deadlock* o con la comunicación de red bloqueada. Las herramientas de monitorización convencionales a menudo tardan en detectar estas anomalías.

CHAOS busca identificar estos estados y verificar la capacidad de recuperación automática del software antes de que el código se despliegue.

---

## Arquitectura Técnica

El sistema está implementado en C++23 moderno siguiendo una **Arquitectura Hexagonal Estricta**. Se compone de dos elementos principales:

### 1. El Orquestador (`CHAOS`)
Ejecutado en el host del desarrollador, gestiona los escenarios de prueba definidos en JSON. Se comunica con la API de Docker para controlar el ciclo de vida de los contenedores y manipula directamente los subsistemas del Kernel de Linux (`cgroups` y `traffic control`) para inyectar fallos de recursos y red.

### 2. El Agente (`CHAOS_wrapper`)
Un binario estático que se inyecta en el contenedor y actúa como proceso padre (`PID 1`). Su función es ejecutar la aplicación del usuario como subproceso, interceptar señales de terminación (`SIGSEGV`, `SIGKILL`) y medir tiempos de ejecución precisos, reportando la telemetría al orquestador vía UDP.

### Stack Tecnológico
*   **Lenguaje:** C++23.
*   **Build System:** CMake + vcpkg.
*   **Librerías:** `spdlog`, `nlohmann_json`, `cpp-httplib`, `GTest`.
*   **Infraestructura:** Docker Engine API, Linux Namespaces, Cgroups v2.

---

## Flujo de Trabajo

Una prueba típica en CHAOS sigue estos pasos:

1.  **Definición:** Se describe el escenario (ej. limitar la memoria a 200MB o la CPU al 30%) y el resultado esperado.
2.  **Instrumentación:** El sistema arranca el contenedor inyectando el agente y reescribiendo el punto de entrada.
3.  **Ejecución:** Durante el ciclo de vida de la aplicación, el orquestador aplica las perturbaciones definidas.
4.  **Veredicto:** El agente reporta la causa exacta de la terminación (por ejemplo, una señal `SIGKILL` provocada por el OOM Killer), permitiendo diferenciar un error de programación de una limitación de infraestructura.

---

## Casos de Uso

CHAOS valida patrones de resiliencia ante fallos comunes:

| Riesgo | Ataque Simulado | Validación |
| :--- | :--- | :--- |
| **Deadlocks de Red** | Latencia variable y cortes | Uso correcto de *Watchdogs* internos. |
| **Datos Obsoletos** | Inyección de *Jitter* | Mecanismos de *Kill Switch* ante lentitud. |
| **Fugas de Memoria** | Límite estricto de RAM | Gestión eficiente de recursos y buffers. |
| **Degradación por CPU** | Limitación de cuota | Degradación controlada y planificación de cargas. |
| **Timeouts** | Latencia HTTP artificial | Configuración de *Circuit Breakers*. |

---

## Estructura del Proyecto

El código sigue una estructura de monorepo organizada según la arquitectura hexagonal:

```plaintext
CHAOS/
├── src/
│   ├── domain/       # Lógica pura y reglas de negocio. Independiente de frameworks.
│   ├── application/  # Casos de uso que orquestan el dominio.
│   ├── adapters/     # Implementaciones (Cliente Docker, Servidor Web, CLI).
│   ├── main.cpp      # Punto de entrada único.
│   └── wrapper/      # Código fuente aislado del Agente.
├── tests/            # Tests unitarios y de integración (GTest).
└── CMakeLists.txt    # Configuración de compilación.
```
