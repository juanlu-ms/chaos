// Stable client-side id generator for list keys.
let counter = 0;
export function uid(prefix = 'id') {
  counter += 1;
  return `${prefix}-${counter}-${Math.random().toString(36).slice(2, 8)}`;
}
