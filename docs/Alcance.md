# Alcance del Proyecto

El desarrollo se centra en la validación de resilencia para **contenedores Docker individuales** en entornos **Linux (x86_64)**.

## Funcionalidad Principal (In-Scope)

El sistema operará bajo dos modalidades: monitorización externa (**Caja Negra**) utilizando la API de Docker, e instrumentación interna (**Caja Gris**) mediante la inyección de un binario *wrapper* para interceptar señales del proceso.

La herramienta implementará tres categorías de inyección de fallos:
*   **Ciclo de vida:** Detención, reinicio y terminación forzada de contenedores.
*   **Recursos:** Restricción dinámica de CPU y memoria utilizando *Cgroups*.
*   **Red:** Simulación de latencia y pérdida de paquetes mediante `tc` (Traffic Control).

El control de la herramienta se realizará principalmente a través de una **Interfaz de Línea de Comandos (CLI)** apta para integración continua, complementada por una API REST para monitorización básica.

## Limitaciones (Out-of-Scope)

Quedan explícitamente excluidos del alcance actual:
*   **Orquestadores:** No se dará soporte nativo a Kubernetes o Docker Swarm, centrándose exclusivamente en el motor Docker.
*   **Sistemas Operativos no-Linux:** Dado que el motor de caos depende de primitivas específicas del kernel Linux (Namespaces, Cgroups), no se soportará la ejecución nativa en Windows o macOS (salvo en entornos virtualizados o WSL2).
*   **Ataques Distribuidos:** No se realizarán simulaciones de ataques DDoS reales; las pruebas de red se limitan a la interfaz local del contenedor.
*   **Interfaz Gráfica Avanzada:** La interfaz visual se limitará a la representacion funcional de estado, priorizando la usabilidad técnica sobre el diseño de experiencia de usuario avanzado.
