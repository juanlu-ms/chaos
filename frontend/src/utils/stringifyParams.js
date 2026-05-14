export function stringifyParams(p) {
  const out = {};
  for (const [k, v] of Object.entries(p || {})) {
    if (v === '' || v == null) continue;
    out[k] = String(v);
  }
  return out;
}
