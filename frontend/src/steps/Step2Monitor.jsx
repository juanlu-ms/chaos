import { useEffect, useRef, useState } from 'react';
import { openEventStream, abortRun } from '../api.js';
import LiveChart, { formatBytesPerSec } from '../components/LiveChart.jsx';
import LogViewer from '../components/LogViewer.jsx';
import ContainerInfo from '../components/ContainerInfo.jsx';
import ConnectionBanner from '../components/ConnectionBanner.jsx';

export default function Step2Monitor({
  manifest,
  testStartTime,
  theme,
  onComplete,
  onBack,
}) {
  const [data, setData] = useState([]);
  const [logs, setLogs] = useState([]);
  const [lastState, setLastState] = useState(null);
  const [zones, setZones] = useState({ normalEnd: 0, chaosEnd: 0 });
  const [elapsed, setElapsed] = useState(0);
  const [conn, setConn] = useState(null); // null | 'lost' | 'dead'

  const phaseRef = useRef('normal');
  const lastEventAt = useRef(Date.now());
  const lostTimer = useRef(null);
  const closeRef = useRef(null);

  // Tick elapsed counter.
  useEffect(() => {
    const id = setInterval(() => {
      setElapsed((Date.now() - testStartTime) / 1000);
    }, 250);
    return () => clearInterval(id);
  }, [testStartTime]);

  // Open SSE.
  useEffect(() => {
    const close = openEventStream({
      onState: (s) => {
        lastEventAt.current = Date.now();
        if (conn) setConn(null);
        const t = (Date.now() - testStartTime) / 1000;
        const net =
          (s.network_rx_bps || 0) + (s.network_tx_bps || 0);
        setData((prev) => [
          ...prev,
          {
            t,
            cpu: s.cpu_usage_percent ?? 0,
            mem: s.memory_usage_mb ?? 0,
            net,
            phase: s.phase || 'normal',
          },
        ]);
        setLastState(s);
        if (s.recent_logs && s.recent_logs.length) {
          setLogs((prev) => {
            const seen = new Set(prev);
            const merged = [...prev];
            for (const line of s.recent_logs) {
              if (!seen.has(line)) {
                merged.push(line);
                seen.add(line);
              }
            }
            return merged;
          });
        }
        // Phase transitions for zones.
        const prev = phaseRef.current;
        const next = s.phase || 'normal';
        if (prev !== next) {
          if (prev === 'normal' && next === 'chaos') {
            setZones((z) => ({ ...z, normalEnd: t }));
          } else if (prev === 'chaos' && next === 'recovery') {
            setZones((z) => ({ ...z, chaosEnd: t }));
          }
          phaseRef.current = next;
        }
      },
      onComplete: (payload) => {
        finalize();
        onComplete({
          ...payload,
          data: dataRef.current,
          logs: logsRef.current,
          zones: zonesRef.current,
          lastState: lastStateRef.current,
        });
      },
      onServerError: (payload) => {
        finalize();
        onComplete({
          passed: false,
          error: payload.error,
          results: [],
          data: dataRef.current,
          logs: logsRef.current,
          zones: zonesRef.current,
          lastState: lastStateRef.current,
        });
      },
      onConnectionError: () => {
        setConn('lost');
        if (lostTimer.current) clearTimeout(lostTimer.current);
        lostTimer.current = setTimeout(() => {
          // 10s without recovery → permanent.
          if (Date.now() - lastEventAt.current >= 9500) {
            setConn('dead');
          }
        }, 10000);
      },
      onConnectionRestored: () => {
        // Don't immediately clear — wait until first state event arrives.
      },
    });
    closeRef.current = close;
    return () => {
      close();
      if (lostTimer.current) clearTimeout(lostTimer.current);
    };
    // eslint-disable-next-line
  }, []);

  // Refs that always see latest state (so async SSE callbacks read fresh).
  const dataRef = useRef(data);
  const logsRef = useRef(logs);
  const zonesRef = useRef(zones);
  const lastStateRef = useRef(lastState);
  useEffect(() => {
    dataRef.current = data;
  }, [data]);
  useEffect(() => {
    logsRef.current = logs;
  }, [logs]);
  useEffect(() => {
    zonesRef.current = zones;
  }, [zones]);
  useEffect(() => {
    lastStateRef.current = lastState;
  }, [lastState]);

  const finalize = () => {
    if (closeRef.current) closeRef.current();
    if (lostTimer.current) clearTimeout(lostTimer.current);
  };

  const handleAbort = async () => {
    try {
      await abortRun();
    } catch (e) {
      console.error(e);
    }
  };

  const target = { id: lastState?.container_id, name: manifest?.target?.id };

  return (
    <>
      <ConnectionBanner kind={conn} onBack={onBack} />
      <div className="monitor-header">
        <div className="info">
          <span className="test-name">{manifest?.test_name}</span>
          <span className="elapsed">{elapsed.toFixed(1)}s elapsed</span>
          <span className="tag" style={{ background: 'var(--accent-muted)', color: 'var(--accent)' }}>
            {lastState?.phase || 'pending'}
          </span>
        </div>
        <button className="danger" onClick={handleAbort}>
          Abort
        </button>
      </div>
      <div className="split">
        <div className="col">
          <LiveChart
            title="CPU Usage"
            data={data}
            dataKey="cpu"
            unit="%"
            themeKey={theme}
          />
          <LiveChart
            title="Memory"
            data={data}
            dataKey="mem"
            unit="MB"
            themeKey={theme}
          />
          <LiveChart
            title="Network I/O"
            data={data}
            dataKey="net"
            themeKey={theme}
            formatter={formatBytesPerSec}
          />
        </div>
        <div className="col">
          <ContainerInfo state={lastState} target={target} />
          <div className="panel">
            <h3>Logs</h3>
            <LogViewer logs={logs} />
          </div>
        </div>
      </div>
    </>
  );
}
