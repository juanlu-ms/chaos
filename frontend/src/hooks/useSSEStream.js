import { useRef, useCallback, useEffect } from 'react';
import { openEventStream } from '../api.js';

const DEFAULT_DEAD_MS = 30000;

export function useSSEStream({ deadThresholdMs = DEFAULT_DEAD_MS } = {}) {
  const closeRef = useRef(null);
  const lostTimer = useRef(null);
  const lastEventAt = useRef(Date.now());
  const runEpoch = useRef(0);

  const closeStream = useCallback(() => {
    if (closeRef.current) {
      closeRef.current();
      closeRef.current = null;
    }
    if (lostTimer.current) {
      clearTimeout(lostTimer.current);
      lostTimer.current = null;
    }
  }, []);

  const startStream = useCallback((start, {
    onState, onComplete, onServerError, onConnectionError, onConnectionRestored,
  }) => {
    const epoch = ++runEpoch.current;
    closeStream();
    lastEventAt.current = Date.now();

    closeRef.current = openEventStream({
      onState: (s) => {
        if (runEpoch.current !== epoch) return;
        lastEventAt.current = Date.now();
        onState?.(s);
      },
      onComplete: (payload) => {
        if (runEpoch.current !== epoch) return;
        closeStream();
        onComplete?.(payload);
      },
      onServerError: (payload) => {
        if (runEpoch.current !== epoch) return;
        closeStream();
        onServerError?.(payload);
      },
      onConnectionError: () => {
        if (runEpoch.current !== epoch) return;
        onConnectionError?.();
        if (lostTimer.current) clearTimeout(lostTimer.current);
        lostTimer.current = setTimeout(() => {
          if (Date.now() - lastEventAt.current >= deadThresholdMs - 500) {
            onConnectionError?.('dead');
          }
        }, deadThresholdMs);
      },
      onConnectionRestored: () => {
        if (runEpoch.current !== epoch) return;
        onConnectionRestored?.();
      },
    });
  }, [closeStream, deadThresholdMs]);

  // Drop the connection on unmount so we don't leak EventSources.
  useEffect(() => () => closeStream(), [closeStream]);

  return { startStream, closeStream };
}
