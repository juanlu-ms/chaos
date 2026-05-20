import { useState, useRef, useCallback } from 'react';

export function useStreamData() {
  const [data, setData] = useState([]);
  const [logs, setLogs] = useState([]);
  const [zones, setZones] = useState({ normalEnd: 0, chaosEnd: 0 });
  const [lastState, setLastState] = useState(null);
  const [conn, setConn] = useState(null);
  const phaseRef = useRef('normal');

  // Snapshot refs so SSE callbacks can read latest values without
  // forcing the parent to mirror state into refs.
  const dataRef = useRef(data);
  const logsRef = useRef(logs);
  const zonesRef = useRef(zones);
  const lastStateRef = useRef(lastState);

  // Track highest log sequence seen, so we can de-dup without rebuilding
  // a Set on every tick. Falls back to a rolling tail set bounded by
  // recent_logs window length when the backend has no sequence id.
  const lastSeqRef = useRef(-1);
  const recentTailRef = useRef([]); // bounded, most-recent log lines

  const resetData = useCallback(() => {
    setData([]); dataRef.current = [];
    setLogs([]); logsRef.current = [];
    setZones({ normalEnd: 0, chaosEnd: 0 });
    zonesRef.current = { normalEnd: 0, chaosEnd: 0 };
    setLastState(null); lastStateRef.current = null;
    setConn(null);
    phaseRef.current = 'normal';
    lastSeqRef.current = -1;
    recentTailRef.current = [];
  }, []);

  const handleState = useCallback((s, start) => {
    setConn((c) => (c ? null : c));
    const t = (Date.now() - start) / 1000;
    const net = (s.network_rx_bps || 0) + (s.network_tx_bps || 0);

    setData((prev) => {
      const next = [...prev, { t, cpu: s.cpu_usage_percent ?? 0, mem: s.memory_usage_mb ?? 0, net, phase: s.phase || 'normal' }];
      dataRef.current = next;
      return next;
    });

    setLastState(s);
    lastStateRef.current = s;

    if (s.recent_logs?.length) {
      // Prefer a backend-provided sequence id when available.
      if (typeof s.log_seq === 'number') {
        if (s.log_seq > lastSeqRef.current) {
          const skip = lastSeqRef.current === -1
            ? 0
            : Math.max(0, s.recent_logs.length - (s.log_seq - lastSeqRef.current));
          const fresh = s.recent_logs.slice(skip);
          if (fresh.length) {
            setLogs((prev) => {
              const next = prev.concat(fresh);
              logsRef.current = next;
              return next;
            });
          }
          lastSeqRef.current = s.log_seq;
        }
      } else {
        // Fallback: dedup by trailing window only — bounded work.
        const tail = recentTailRef.current;
        const tailSet = new Set(tail);
        const fresh = [];
        for (const line of s.recent_logs) {
          if (line == null) continue;
          if (!tailSet.has(line)) {
            fresh.push(line);
            tailSet.add(line);
          }
        }
        if (fresh.length) {
          setLogs((prev) => {
            const next = prev.concat(fresh);
            logsRef.current = next;
            return next;
          });
          // Keep the tail window equal to the recent_logs window size.
          const merged = tail.concat(fresh);
          recentTailRef.current = merged.slice(-Math.max(s.recent_logs.length * 2, 50));
        }
      }
    }

    const prevPhase = phaseRef.current;
    const next = s.phase || 'normal';
    if (prevPhase !== next) {
      if (prevPhase === 'normal' && next === 'chaos') {
        setZones((z) => {
          const nz = { ...z, normalEnd: t };
          zonesRef.current = nz;
          return nz;
        });
      } else if (prevPhase === 'chaos' && next === 'recovery') {
        setZones((z) => {
          const nz = { ...z, chaosEnd: t };
          zonesRef.current = nz;
          return nz;
        });
      }
      phaseRef.current = next;
    }
  }, []);

  const handleConnectionError = useCallback((kind) => {
    if (kind === 'dead') setConn('dead');
    else setConn('lost');
  }, []);

  const handleConnectionRestored = useCallback(() => {
    setConn(null);
  }, []);

  const getSnapshot = useCallback(() => ({
    data: dataRef.current,
    logs: logsRef.current,
    zones: zonesRef.current,
    lastState: lastStateRef.current,
  }), []);

  return {
    data, logs, zones, lastState, conn,
    resetData, handleState, handleConnectionError, handleConnectionRestored,
    setConn, getSnapshot,
  };
}
