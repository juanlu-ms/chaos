// Thin wrappers around the CHAOS HTTP API.

async function jsonOrThrow(res) {
  if (!res.ok) throw new Error(`${res.status} ${res.statusText}`);
  const ct = res.headers.get('content-type') || '';
  return ct.includes('application/json') ? res.json() : res.text();
}

export const getTargets = () => fetch('/api/targets').then(jsonOrThrow);
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
  fetch(`/containers/${id}/stop`, { method: 'POST' }).then(jsonOrThrow);

export const killContainer = (id) =>
  fetch(`/containers/${id}/kill`, { method: 'POST' }).then(jsonOrThrow);

export const getContainerLogs = (id) =>
  fetch(`/containers/${id}/logs`).then(jsonOrThrow);

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
  const es = new EventSource('/events');

  es.addEventListener('state', (e) => {
    try {
      onState && onState(JSON.parse(e.data));
    } catch (err) {
      console.error('bad state event', err);
    }
  });

  es.addEventListener('complete', (e) => {
    try {
      onComplete && onComplete(JSON.parse(e.data));
    } catch (err) {
      console.error('bad complete event', err);
    }
  });

  es.addEventListener('error', (e) => {
    // Named server-sent error event has data; transport errors don't.
    if (e && e.data) {
      try {
        const payload = JSON.parse(e.data);
        onServerError && onServerError(payload);
      } catch {
        onServerError && onServerError({ error: String(e.data) });
      }
    } else {
      onConnectionError && onConnectionError();
    }
  });

  es.addEventListener('open', () => {
    console.log('[eventsource] connected to /events');
    onConnectionRestored && onConnectionRestored();
  });

  return () => es.close();
}
