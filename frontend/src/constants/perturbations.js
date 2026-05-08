export const PERTURBATION_TYPES = [
  { type: 'kill', label: 'Kill', params: [] },
  {
    type: 'memory_cap',
    label: 'Memory Cap',
    params: [
      {
        key: 'limit_bytes',
        label: 'Limit (bytes)',
        placeholder: '268435456',
        required: true,
        default: '268435456',
      },
    ],
  },
  {
    type: 'cpu_cap',
    label: 'CPU Cap',
    params: [
      {
        key: 'cpu_cores',
        label: 'CPU Cores',
        placeholder: '1',
        required: true,
        default: '1',
      },
    ],
  },
  {
    type: 'network_delay',
    label: 'Network Delay',
    params: [
      {
        key: 'delay_ms',
        label: 'Delay (ms)',
        placeholder: '1000',
        required: true,
        default: '1000',
      },
    ],
  },
  {
    type: 'network_cutoff',
    label: 'Network Cutoff',
    params: [
      { key: 'dst_ip', label: 'Dest IP', placeholder: 'target IP' },
      { key: 'dst_port', label: 'Dest Port', placeholder: 'target port' },
      { key: 'src_port', label: 'Source Port', placeholder: 'source port' },
    ],
  },
  {
    type: 'garbage_packet',
    label: 'Garbage Packet',
    params: [
      { key: 'corrupt_pct', label: 'Corrupt %', placeholder: 'e.g. 25' },
      { key: 'loss_pct', label: 'Loss %', placeholder: 'e.g. 10' },
      { key: 'duplicate_pct', label: 'Duplicate %', placeholder: 'e.g. 5' },
      { key: 'iface', label: 'Interface', placeholder: 'eth0', default: 'eth0' },
    ],
  },
];

export const PERTURBATION_BY_TYPE = Object.fromEntries(
  PERTURBATION_TYPES.map((p) => [p.type, p])
);

export function defaultParamsFor(type) {
  const def = PERTURBATION_BY_TYPE[type];
  if (!def) return {};
  const out = {};
  for (const p of def.params) out[p.key] = p.default ?? '';
  return out;
}
