import { useEffect, useState } from 'react';

export function useElapsedTimer(isRunning, testStartTime, lastDataPoint) {
  const [elapsed, setElapsed] = useState(
    testStartTime ? (Date.now() - testStartTime) / 1000 : 0
  );

  useEffect(() => {
    if (!isRunning) return;
    let id = null;
    const tick = () => setElapsed((Date.now() - testStartTime) / 1000);
    const start = () => {
      if (id != null) return;
      id = setInterval(tick, 250);
    };
    const stop = () => {
      if (id != null) { clearInterval(id); id = null; }
    };
    const onVisibility = () => {
      if (document.visibilityState === 'visible') { tick(); start(); }
      else stop();
    };
    if (document.visibilityState === 'visible') start();
    document.addEventListener('visibilitychange', onVisibility);
    return () => {
      stop();
      document.removeEventListener('visibilitychange', onVisibility);
    };
  }, [isRunning, testStartTime]);

  useEffect(() => {
    if (!isRunning && lastDataPoint != null) {
      setElapsed(lastDataPoint);
    }
  }, [isRunning, lastDataPoint]);

  return elapsed;
}
