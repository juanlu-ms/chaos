# Placeholder target for eBPF sources. Real compilation pipeline (clang -> BPF bytecode)
# will be added when libbpf/skeleton integration is introduced.
add_custom_target(ebpf_programs
    SOURCES
        src/network_filter.bpf.c
        src/resource_monitor.bpf.c
)
