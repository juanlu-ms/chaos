export function shortId(id, len = 12) {
  if (!id) return '—';
  return id.length > len ? id.slice(0, len) : id;
}
