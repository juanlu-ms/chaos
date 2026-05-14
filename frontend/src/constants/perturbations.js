export const PERTURBATION_PARAMS = {
  kill: [],
  memory_cap: [
    {
      key: 'limit_bytes',
      label: 'Limit (bytes)',
      placeholder: '268435456',
      required: true,
      default: '268435456',
    },
  ],
  cpu_cap: [
    {
      key: 'cpu_cores',
      label: 'CPU Cores',
      placeholder: '1',
      required: true,
      default: '1',
    },
  ],
  network_delay: [
    {
      key: 'delay_ms',
      label: 'Delay (ms)',
      placeholder: '1000',
      required: true,
      default: '1000',
    },
  ],
  network_cutoff: [
    { key: 'dst_ip', label: 'Dest IP', placeholder: 'target IP' },
    { key: 'dst_port', label: 'Dest Port', placeholder: 'target port' },
    { key: 'src_port', label: 'Source Port', placeholder: 'source port' },
  ],
  garbage_packet: [
    { key: 'corrupt_pct', label: 'Corrupt %', placeholder: 'e.g. 25' },
    { key: 'loss_pct', label: 'Loss %', placeholder: 'e.g. 10' },
    { key: 'duplicate_pct', label: 'Duplicate %', placeholder: 'e.g. 5' },
    { key: 'iface', label: 'Interface', placeholder: 'eth0', default: 'eth0' },
  ],
};

export const PERTURBATION_TYPES = Object.keys(PERTURBATION_PARAMS).map((t) => ({
  type: t,
  label: t
    .split('_')
    .map((w) => w[0].toUpperCase() + w.slice(1))
    .join(' '),
}));

export const PERTURBATION_BY_TYPE = PERTURBATION_PARAMS;

export function defaultParamsFor(type) {
  const params = PERTURBATION_PARAMS[type] || [];
  const out = {};
  for (const p of params) out[p.key] = p.default ?? '';
  return out;
}
