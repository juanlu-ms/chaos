export const EXPECTATION_PARAMS = {
  container_running: [],
  container_not_running: [],
  log_contains: [
    {
      key: 'substring',
      label: 'Substring',
      placeholder: 'text to find',
      required: true,
    },
  ],
  log_not_contains: [
    {
      key: 'substring',
      label: 'Substring',
      placeholder: 'must not appear',
      required: true,
    },
  ],
  http_status: [
    { key: 'port', label: 'Port', placeholder: '8000', required: true },
    { key: 'path', label: 'Path', placeholder: '/ping', required: true },
    {
      key: 'expected_status',
      label: 'Expected Status',
      placeholder: '200',
      required: true,
    },
  ],
  http_latency: [
    { key: 'port', label: 'Port', placeholder: '8000', required: true },
    { key: 'path', label: 'Path', placeholder: '/ping', required: true },
    {
      key: 'max_latency_ms',
      label: 'Max Latency',
      placeholder: '1000',
      required: true,
    },
  ],
};

export const EXPECTATION_TYPES = Object.keys(EXPECTATION_PARAMS).map((t) => ({
  type: t,
  label: t
    .split('_')
    .map((w) => w[0].toUpperCase() + w.slice(1))
    .join(' '),
}));

export function defaultExpectationParams(type) {
  const params = EXPECTATION_PARAMS[type] || [];
  const out = {};
  for (const p of params) out[p.key] = p.default ?? '';
  return out;
}
