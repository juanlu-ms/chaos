# Definición de Requisitos

## Requisitos Funcionales

La implementación del sistema se divide en cuatro fases incrementales, priorizando la arquitectura base y la conectividad antes de avanzar hacia funcionalidades complejas de instrumentación.

### Fase 1: Infraestructura Base y Conectividad

El objetivo inicial es establecer la comunicación con el Docker Engine y validar la arquitectura hibrida por componentes.

| ID | Requisito | Prioridad | Descripción |
| :--- | :--- | :--- | :--- |
| **RF-1.1** | Conexión Docker | **MUST** | El sistema debe conectarse al socket de Docker (`/var/run/docker.sock`) y listar contenedores activos. |
| **RF-1.2** | Gestión de Ciclo de Vida | **MUST** | Capacidad de detener (`stop`) y forzar la terminación (`kill`) de un contenedor específico por su ID. |
| **RF-1.3** | Arquitectura Hibrida por Componentes | **MUST** | El sistema debe separar claramente `orchestrator` y `wrapper`, con ownership explícito de código y pruebas por módulo. |
| **RF-1.4** | CLI de Control | **SHOULD** | Interfaz de línea de comandos para facilitar la ejecución en scripts (ej: `CHAOS attack --target <id>`). |
| **RF-1.5** | Panel Web Básico | **SHOULD** | Interfaz visual ligera para seleccionar contenedores y visualizar métricas básicas en tiempo real. |

### Fase 2: Observabilidad y Reportes

En esta fase se implementan los mecanismos para detectar fallos y evaluar el resultado de las pruebas.

| ID | Requisito | Prioridad | Descripción |
| :--- | :--- | :--- | :--- |
| **RF-2.1** | Monitorización de Estado | **MUST** | Detectar terminaciones inesperadas de contenedores (Exit Code != 0). |
| **RF-2.2** | Análisis de Logs | **SHOULD** | Capacidad de analizar los logs (`stdout`/`stderr`) buscando patrones de error definidos mediante expresiones regulares. |
| **RF-2.3** | Motor de Escenarios | **MUST** | Carga y ejecución de ficheros de configuración (manifest) que definen la víctima, el ataque y el resultado esperado. |
| **RF-2.4** | Generación de Informes | **MUST** | Emisión de un veredicto (`PASS`/`FAIL`) en formato JSON estructurado al finalizar la prueba. |

### Fase 3: Instrumentación (Caja Gris)

Desarrollo del agente interno para obtener métricas desde el interior del contenedor.

| ID | Requisito | Prioridad | Descripción |
| :--- | :--- | :--- | :--- |
| **RF-3.1** | Inyección de Agente | **COULD** | Modificación dinámica del `EntryPoint` del contenedor para inyectar el binario `CHAOS_wrapper` en tiempo de ejecución. |
| **RF-3.2** | Captura de Señales | **SHOULD** | El agente debe interceptar señales del kernel (`SIGTERM`, `SIGSEGV`) antes de que finalicen el proceso hijo. |
| **RF-3.3** | Telemetría Interna | **COULD** | Medición precisa del tiempo de vida del proceso hijo y transmisión de datos al orquestador. |

### Fase 4: Simulación de Fallos Avanzada

Mecanismos para degradar el entorno de ejecución.

| ID | Requisito | Prioridad | Descripción |
| :--- | :--- | :--- | :--- |
| **RF-4.1** | Restricción de Memoria | **SHOULD** | Uso de Cgroups para limitar la RAM disponible y provocar condiciones de *Out Of Memory* (OOM). |
| **RF-4.2** | Latencia de Red | **COULD** | Inyección de retardo y pérdida de paquetes en la interfaz de red virtual usando `tc` (Traffic Control). |
| **RF-4.3** | Restricción de CPU | **SHOULD** | Uso de Cgroups para limitar la cuota de CPU (ej. `cpu.max`) y simular entornos con carga elevada. |

## Requisitos No Funcionales

*   **RNF-1 Rendimiento:** El agente `CHAOS_wrapper` debe introducir una latencia de arranque inferior a **10ms**.
*   **RNF-2 Seguridad:** La operación estándar no debe requerir privilegios de `root` en el host, asumiendo que el usuario pertenece al grupo `docker`.
*   **RNF-3 Portabilidad:** El código fuente debe ser compatible con cualquier distribución Linux moderna que soporte C++23.
*   **RNF-4 Calidad de Código:** Adherencia a principios SOLID y mantenimiento de una cobertura de pruebas razonable para el núcleo del dominio.
*   **RNF-5 Gestión de Dependencias:** Uso estricto de `vcpkg` para garantizar la reproducibilidad de la compilación.
