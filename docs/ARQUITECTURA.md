# Diseño Arquitectónico

La arquitectura de `CHAOS` sigue el patrón de **Arquitectura Hexagonal**, favoreciendo el desacoplamiento entre las reglas de negocio y los detalles de implementación tecnológica. El sistema se desarrolla en C++23.

La solución se divide en dos componentes autónomos que colaboran entre sí: el **Orquestador** (Host) y el **Agente de Instrumentación** (Guest).

---

## 1. Componentes del Sistema

### 1.1. Orquestador (`CHAOS`)

Es el componente que reside en el host del desarrollador y coordina la ejecución de las pruebas.

*   **Responsabilidades:**
    *   Gestión del ciclo de vida de los escenarios de caos.
    *   Interacción con el Docker Engine API para la provisión de infraestructura.
    *   Control de recursos del host (Cgroups, Network Namespaces) para la inyección de fallos.
    *   Recolección y procesamiento de telemetría.
*   **Implementación:** Encapsula la lógica de dominio (Entidades `Scenario`, `Experiment`) y utiliza adaptadores para interactuar con entradas (CLI, Web) y salidas (Docker, Sistema de Ficheros).

### 1.2. Agente de Instrumentación (`CHAOS_wrapper`)

Es un componente ligero diseñado para ejecutarse dentro del contenedor objetivo.

*   **Responsabilidades:**
    *   Supervisión directa del proceso de usuario (`PID 1`).
    *   Intercepción de señales del sistema operativo (`SIGTERM`, `SIGSEGV`).
    *   Recolección de métricas de proceso (uso de memoria, tiempos de CPU).
    *   Detección de bloqueos o *deadlocks*.
    *   Transmisión de datos al orquestador vía UDP.
*   **Implementación:** Binario sin dependencias dinámicas (linked static) para asegurar compatibilidad con cualquier imagen base Linux (Alpine, Debian, Distroless).

### 1.3. Diagrama de Componentes

El siguiente diagrama ilustra la relación entre los componentes:

```ascii
+---------------------------------+
|         Máquina Anfitriona      |
|                                 |
|  +---------------------------+  |
|  |   Orquestador (`CHAOS`)    |  |
|  |---------------------------|  |
|  | 🔹 Lógica de Dominio      |  |
|  | 🔹 Adaptador API Docker   |------> Docker Engine
|  | 🔹 Adaptador Servidor Web |<-----> (Usuario)
|  | 🔹 Adaptador Telemetría   |<--+
|  +---------------------------+  |   |
|                                 |   |
+---------------------------------+   | (UDP)
                                      |
+---------------------------------+   |
|      Contenededor Docker          |   |
|                                 |   |
|  +---------------------------+  |   |
|  | Agente Parásito (`wrapper`)|--+
|  |---------------------------|  |
|  | 🔹 Monitoriza App         |  |
|  | 🔹 Envía Telemetría       |  |
|  |                           |  |
|  |   +-------------------+   |  |
|  |   | Aplicación Usuario|   |  |
|  |   +-------------------+   |  |
|  +---------------------------+  |
+---------------------------------+
```

---

## 2. Organización del Código

El repositorio sigue una estructura de monorepo alineada con los principios de la arquitectura definida:

```plaintext
CHAOS/
├── src/
│   ├── domain/       # Núcleo del negocio: Entidades y Puertos (Interfaces).
│   │                 # Sin dependencias externas.
│   ├── application/  # Servicios de aplicación y casos de uso.
│   │
│   ├── adapters/     # Implementaciones de infraestructura.
│   │   ├── infra/    # Cliente Docker, Controladores de Cgroups.
│   │   └── ui/       # Controladores HTTP, Entrada CLI.
│   │
│   ├── wrapper/      # Código fuente del agente interno (independiente).
│   │
│   └── main.cpp      # Punto de entrada y composición de dependencias.
│
├── tests/            # Suite de pruebas automatizadas (GTest).
└── CMakeLists.txt    # Configuración de construcción (CMake).
```
