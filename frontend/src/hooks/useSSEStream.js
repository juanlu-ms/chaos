import { useRef, useCallback } from 'react';
import { openEventStream } from '../api.js';

export function useSSEStream() {
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

  const startStream = useCallback((start, { onState, onComplete, onServerError, onConnectionError }) => {
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
        onConnectionError?.();
        if (lostTimer.current) clearTimeout(lostTimer.current);
        lostTimer.current = setTimeout(() => {
          if (Date.now() - lastEventAt.current >= 9500) onConnectionError?.('dead');
        }, 10000);
      },
      onConnectionRestored: () => {},
    });
  }, [closeStream]);

  return { startStream, closeStream };
}
