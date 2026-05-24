// Thin wrappers around the CHAOS HTTP API.

export class ApiError extends Error {
  constructor(message, { status, statusText, body } = {}) {
    super(message);
    this.name = 'ApiError';
    this.status = status;
    this.statusText = statusText;
    this.body = body;
  }
}

async function jsonOrThrow(res) {
  const ct = res.headers.get('content-type') || '';
  const isJson = ct.includes('application/json');
  // Read body once, regardless of ok-ness, so error responses surface.
  const body = isJson
    ? await res.json().catch(() => null)
    : await res.text().catch(() => '');

  if (!res.ok) {
    const detail =
      (body && typeof body === 'object' && (body.error || body.message)) ||
      (typeof body === 'string' && body) ||
      res.statusText ||
      'Request failed';
    throw new ApiError(`${res.status} ${detail}`, {
      status: res.status,
      statusText: res.statusText,
      body,
    });
  }
  return body;
}

export const getTargets = () => fetch('/api/containers').then(jsonOrThrow);
export const getLimits = () => fetch('/api/limits').then(jsonOrThrow);

export const runManifest = (manifest) =>
  fetch('/api/run', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(manifest),
  }).then(jsonOrThrow);

export const abortRun = () =>
  fetch('/api/run/abort', { method: 'POST' }).then(jsonOrThrow);

export const stopContainer = (id) =>
  fetch(`/api/containers/${id}/stop`, { method: 'POST' }).then(jsonOrThrow);

export const killContainer = (id) =>
  fetch(`/api/containers/${id}/kill`, { method: 'POST' }).then(jsonOrThrow);

export const getContainerLogs = (id) =>
  fetch(`/api/containers/${id}/logs`).then(jsonOrThrow);

/**
 * Open the SSE event stream and dispatch typed callbacks.
 * Distinguishes server-sent `event: error` (named) from transport
 * EventSource.onerror failures.
 */
export function openEventStream({
  onState,
  onComplete,
  onServerError,
  onConnectionError,
  onConnectionRestored,
}) {
  const es = new EventSource('/api/events');

  es.addEventListener('state', (e) => {
    try {
      onState?.(JSON.parse(e.data));
    } catch (err) {
      console.error('bad state event', err);
    }
  });

  es.addEventListener('complete', (e) => {
    try {
      onComplete?.(JSON.parse(e.data));
    } catch (err) {
      console.error('bad complete event', err);
    }
  });

  es.addEventListener('error', (e) => {
    if (e?.data) {
      try {
        const payload = JSON.parse(e.data);
        onServerError?.(payload);
      } catch {
        onServerError?.({ error: String(e.data) });
      }
    } else {
      onConnectionError?.();
    }
  });

  es.addEventListener('open', () => {
    onConnectionRestored?.();
  });

  return () => es.close();
}
