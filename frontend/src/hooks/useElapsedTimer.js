import { useEffect, useState } from 'react';

export function useElapsedTimer(isRunning, testStartTime, lastDataPoint) {
  const [elapsed, setElapsed] = useState(
    testStartTime ? (Date.now() - testStartTime) / 1000 : 0
  );

  useEffect(() => {
    if (!isRunning) return;
    const id = setInterval(() => {
      setElapsed((Date.now() - testStartTime) / 1000);
    }, 250);
    return () => clearInterval(id);
  }, [isRunning, testStartTime]);

  useEffect(() => {
    if (!isRunning && lastDataPoint != null) {
      setElapsed(lastDataPoint);
    }
  }, [isRunning, lastDataPoint]);

  return elapsed;
}
