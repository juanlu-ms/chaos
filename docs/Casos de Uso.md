# Casos de Uso y Aplicación

Este documento detalla escenarios prácticos donde la validación de resiliencia mediante `CHAOS` aporta valor significativo al ciclo de desarrollo. Se analizan situaciones críticas donde los fallos de software pueden tener consecuencias operativas graves.

## Caso 1: Monitorización en Telemedicina (Sistemas Críticos)

**Contexto:** Un sistema de monitorización clínica despliega contenedores en dispositivos edge (ej. Raspberry Pi) en una UCI. El componente centraliza alertas de sensores vitales y las reporta a un servidor central.

**El Problema:** Un fallo en la red puede causar que el hilo principal del proceso se bloquee en una operación de E/S (`socket write`), dejando de procesar nuevas lecturas de sensores.

**Validación con CHAOS:**
1.  **Escenario:** Simulación de una red inestable con latencia variable y cortes abruptos.
2.  **Resultado esperado:** El sistema debe detectar la falta de conectividad y reiniciar la conexión o alertar localmente, en lugar de quedar en estado de *deadlock*.
3.  **Diagnóstico:** Si el agente `CHAOS_wrapper` detecta inactividad de CPU prolongada mientras el contenedor sigue "Running", se identifica un bloqueo silencioso, validando la necesidad de un *Watchdog* interno.

## Caso 2: Gestión de Memoria en Dispositivos Limitados

**Contexto:** Una aplicación de procesamiento de datos se despliega en hardware con recursos limitados.

**El Problema:** Fugas de memoria menores que pasan desapercibidas en entornos de desarrollo con amplia RAM (32GB+), pero causan un *crash* (OOM Kill) tras horas de funcionamiento en el dispositivo final (512MB).

**Validación con CHAOS:**
1.  **Escenario:** Restricción estricta de memoria mediante Cgroups (ej. 200MB).
2.  **Resultado:** El fallo se reproduce en minutos en lugar de horas.
3.  **Diagnóstico:** El agente captura la señal `SIGKILL` e identifica si fue provocada por el OOM Killer del kernel, permitiendo al desarrollador optimizar el uso de buffers antes del despliegue.

## Caso 3: Integridad Transaccional en FinTech

**Contexto:** Un bot de trading o pasarela de pagos que requiere datos de mercado en tiempo real.

**El Problema:** La latencia de red invisible ("staleness") puede llevar a tomar decisiones basadas en precios obsoletos.

**Validación con CHAOS:**
1.  **Escenario:** Inyección de *jitter* (latencia aleatoria) en la interfaz de red.
2.  **Resultado:** Verificación de mecanismos de tiempo de espera (*timeouts*).
3.  **Validación:** Se comprueba si la aplicación descarta datos viejos o implementa un *Kill Switch* para detener operaciones cuando la latencia supera un umbral seguro, evitando pérdidas financieras.

## Caso 4: Resiliencia de Microservicios (E-commerce)

**Contexto:** Comunicación entre una API de pedidos y una pasarela de pagos externa.

**El Problema:** La caída de un servicio externo puede agotar los hilos del servicio llamante si no se gestionan los tiempos de espera.

**Validación con CHAOS:**
1.  **Escenario:** Pérdida de paquetes del 30% y latencia elevada.
2.  **Validación de Patrones:**
    *   **Timeouts:** ¿El servicio corta la conexión tras 2 segundos o se queda esperando indefinidamente?
    *   **Circuit Breakers:** ¿El sistema deja de intentar conectar tras múltiples fallos consecutivos para protegerse?

## Caso 5: Procesamiento de Video/Imagen (Big Data / ETL)

**Contexto:** Un proceso en Python o C++ que procesa archivos grandes (video, imagen médica, lotes de logs) dentro de un contenedor.

**El Problema:** En un entorno con memoria limitada, cargar todo el fichero en RAM puede provocar un cierre abrupto por OOM (*Out Of Memory*), especialmente cuando el tamaño real del archivo supera lo probado en desarrollo.

**Validación con CHAOS:**
1.  **Escenario:** Restricción estricta de memoria con Cgroups (ej. 512MB).
2.  **Resultado esperado:** El proceso debe usar streaming o procesamiento por bloques en lugar de intentar cargar el archivo completo.
3.  **Diagnóstico:** Si el proceso muere por `SIGKILL`, se confirma que el diseño depende de RAM abundante y no escala en producción.

## Caso 6: Servidores de Videojuegos en Tiempo Real

**Contexto:** Un servidor multijugador (UDP) que mantiene un bucle de actualización constante (ticks), sensible al rendimiento de CPU.

**El Problema:** Cuando la CPU se estrangula, los ticks se retrasan, aparecen saltos en el estado del juego y los clientes pueden desconectarse por timeout.

**Validación con CHAOS:**
1.  **Escenario:** Limitación de CPU al 30% en el contenedor objetivo.
2.  **Resultado esperado:** El servidor debe degradar de forma controlada o reducir carga antes de provocar desconexiones masivas.
3.  **Diagnóstico:** Correlación entre la bajada de CPU y mensajes de warning del servidor (ej. "server overloaded").

## Caso 7: Pools de Conexiones a Base de Datos

**Contexto:** Servicios en Node.js/Java con conexiones persistentes a una base de datos (PostgreSQL/MySQL).

**El Problema:** Un corte de red breve puede invalidar el pool. Al volver la conexión, muchas apps reintentan en masa y saturan el servidor de base de datos.

**Validación con CHAOS:**
1.  **Escenario:** Corte total de red durante 10 segundos, seguido de restauración inmediata.
2.  **Resultado esperado:** Reconexión progresiva con backoff, limpieza de conexiones rotas y recuperación estable.
3.  **Diagnóstico:** Se detecta si hay tormenta de reintentos o conexiones zombie que nunca se reciclan.
