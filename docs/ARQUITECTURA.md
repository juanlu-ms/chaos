# Diseño Arquitectonico

La arquitectura de `CHAOS` es **hibrida por componentes**.

El primer nivel del repositorio se organiza por unidades de despliegue y ejecucion:

- `orchestrator/`: proceso host principal.
- `wrapper/`: agente dentro del contenedor objetivo.
- `ebpf_src/`: programas eBPF de bajo nivel para kernel.

Dentro de `orchestrator/`, el codigo se agrupa por ownership y responsabilidad (interfaces, contenedores, perturbaciones, observabilidad, escenarios), evitando acoplamientos innecesarios con carpetas globales de arquitectura legacy.

---

## 1. Componentes del Sistema

### 1.1. Orchestrator (`chaos`)

Componente host que coordina escenarios y expone control por CLI y Web.

**Responsabilidades**

- Gestion del ciclo de vida de escenarios.
- Interaccion con Docker Engine API para operaciones sobre contenedores.
- Aplicacion de perturbaciones en host (CPU, memoria, red, eBPF en fases posteriores).
- Recoleccion de evidencias y emision de estado/veredicto.

### 1.2. Wrapper (`chaos_wrapper`)

Agente ejecutado dentro del contenedor objetivo.

**Responsabilidades**

- Actuar como proceso de supervision de la app target.
- Capturar eventos/senales de ejecucion.
- Reportar telemetria al orchestrator.

### 1.3. eBPF Programs (`ebpf_src`)

Codigo fuente de programas del kernel para instrumentacion avanzada.

**Responsabilidades**

- Definir probes/filtros de red y observabilidad de bajo nivel.
- Mantener separacion explicita entre codigo kernel-space y user-space.

---

## 2. Organizacion del Codigo

```plaintext
CHAOS/
├── orchestrator/
│   ├── src/
│   │   ├── main.cpp
│   │   ├── interfaces/
│   │   │   ├── cli/
│   │   │   └── web/
│   │   ├── containers/
│   │   │   └── internal/
│   │   ├── scenarios/
│   │   ├── perturbations/
│   │   │   └── internal/
│   │   ├── observability/
│   │   │   └── internal/
│   │   └── shared/
│   └── tests/
│       ├── containers/
│       │   └── internal/
│       └── smoke/
├── wrapper/
│   ├── src/
│   └── tests/
├── ebpf_src/
│   └── src/
├── tests/
│   └── e2e/
└── CMakeLists.txt
```

---

## 3. Estrategia de Pruebas

- `orchestrator/tests/containers/internal`: unit tests de modulo propietario.
- `orchestrator/tests/smoke`: pruebas rapidas del binario host y conectividad minima.
- `tests/e2e`: pruebas cross-componente.

Regla de ownership: una suite de tests no debe depender de internals de otro modulo salvo que sea una prueba de integracion/e2e declarada como tal.

---

## 4. Build y Composicion

El build se orquesta desde `CMakeLists.txt` raiz con subproyectos:

- `add_subdirectory(orchestrator)`
- `add_subdirectory(wrapper)`
- `add_subdirectory(ebpf_src)`

Esto permite evolucionar cada componente de forma independiente sin arrastrar compatibilidad legacy de estructura previa.
