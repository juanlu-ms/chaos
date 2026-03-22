# CHAOS: Chaos Handling And Observability System

**CHAOS** es un framework de ingeniería del caos diseñado para la etapa de desarrollo de software (*Shift-Left Testing*). A diferencia de las herramientas que operan sobre clústeres en producción, CHAOS permite a los desarrolladores validar la resiliencia de sus contenedores Docker localmente, sometiéndolos a condiciones adversas como límites de CPU, inestabilidad de red o fugas de memoria durante la fase de programación.

La principal innovación del proyecto es su **arquitectura de "Caja Gris"**. Mediante un agente ligero (`CHAOS_wrapper`), el sistema no solo interrumpe contenedores desde el exterior, sino que se ejecuta dentro del entorno de la aplicación para interceptar señales y diagnósticos que el runtime de Docker generalmente no expone.

---

## Motivación

En sistemas críticos (como aquellos utilizados en telemedicina, finanzas o procesamiento en tiempo real), un fallo silencioso suele ser más peligroso que una parada total. Un contenedor puede aparecer como "Running" según el orquestador, mientras su proceso interno se encuentra en estado de *deadlock* o con la comunicación de red bloqueada. Las herramientas de monitorización convencionales a menudo tardan en detectar estas anomalías.

CHAOS busca identificar estos estados y verificar la capacidad de recuperación automática del software antes de que el código se despliegue.

---

## Arquitectura Técnica

El sistema está implementado en C++23 moderno siguiendo una arquitectura **híbrida por componentes**. Se compone de 3 elementos principales:

### 1. El Orquestador (`chaos`)

Ejecutado en el host del desarrollador, gestiona los escenarios de prueba definidos en JSON. Se comunica con la API de Docker para controlar el ciclo de vida de los contenedores y manipula directamente los subsistemas del Kernel de Linux (`cgroups` y `traffic control`) para inyectar fallos de recursos y red.

### 2. El Agente (`chaos-wrapper`)

Un binario estático que se inyecta en el contenedor y actúa como proceso padre (`PID 1`). Su función es ejecutar la aplicación del usuario como subproceso, interceptar señales de terminación (`SIGSEGV`, `SIGKILL`) y medir tiempos de ejecución precisos, reportando la telemetría al orquestador vía UDP.

### Stack tecnológico

- **Lenguaje:** C++23.
- **Build System:** CMake + vcpkg.
- **Librerías:** `spdlog`, `nlohmann_json`, `cpp-httplib`, `GTest`, `fmt`.
- **Infraestructura:** Docker Engine API y Linux cgroups/namespaces.

---

## Flujo de Trabajo

1. Definir escenario y condiciones de fallo.
2. Ejecutar objetivo bajo control del orchestrator.
3. Aplicar perturbaciones y recolectar evidencias.
4. Evaluar resultado y emitir veredicto técnico.

---

## Estructura del Proyecto

```plaintext
CHAOS/
├── orchestrator/
│   ├── include/
│   │   └── containers/
│   ├── src/
│   │   ├── interfaces/
│   │   ├── containers/
│   │   ├── manifests/
│   │   ├── perturbations/
│   │   ├── observability/
│   │   └── main.cpp
│   └── tests/
├── wrapper/
│   ├── src/
│   └── tests/
├── tests/
│   └── e2e/
└── CMakeLists.txt
```
