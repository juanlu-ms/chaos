import { useState, useRef, useCallback } from 'react';

export function useStreamData() {
  const [data, setData] = useState([]);
  const [logs, setLogs] = useState([]);
  const [zones, setZones] = useState({ normalEnd: 0, chaosEnd: 0 });
  const [lastState, setLastState] = useState(null);
  const [conn, setConn] = useState(null);
  const phaseRef = useRef('normal');

  const resetData = useCallback(() => {
    setData([]);
    setLogs([]);
    setZones({ normalEnd: 0, chaosEnd: 0 });
    setLastState(null);
    setConn(null);
    phaseRef.current = 'normal';
  }, []);

  const handleState = useCallback((s, start) => {
    setConn((c) => (c ? null : c));
    const t = (Date.now() - start) / 1000;
    const net = (s.network_rx_bps || 0) + (s.network_tx_bps || 0);
    setData((prev) => [...prev, { t, cpu: s.cpu_usage_percent ?? 0, mem: s.memory_usage_mb ?? 0, net, phase: s.phase || 'normal' }]);
    setLastState(s);
    if (s.recent_logs && s.recent_logs.length) {
      setLogs((prev) => {
        const seen = new Set(prev);
        const merged = [...prev];
        for (const line of s.recent_logs) {
          if (line && !seen.has(line)) { merged.push(line); seen.add(line); }
        }
        return merged;
      });
    }
    const prev = phaseRef.current;
    const next = s.phase || 'normal';
    if (prev !== next) {
      if (prev === 'normal' && next === 'chaos') setZones((z) => ({ ...z, normalEnd: t }));
      else if (prev === 'chaos' && next === 'recovery') setZones((z) => ({ ...z, chaosEnd: t }));
      phaseRef.current = next;
    }
  }, []);

  const handleConnectionError = useCallback((kind) => {
    if (kind === 'dead') setConn('dead');
    else setConn('lost');
  }, []);

  return { data, logs, zones, lastState, conn, resetData, handleState, handleConnectionError, setConn };
}
