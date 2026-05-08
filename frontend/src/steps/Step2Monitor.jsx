import { useEffect, useState } from 'react';
import LiveChart, { formatBytesPerSec } from '../components/LiveChart.jsx';
import LogViewer from '../components/LogViewer.jsx';
import ContainerInfo from '../components/ContainerInfo.jsx';
import ConnectionBanner from '../components/ConnectionBanner.jsx';

export default function Step2Monitor({
  manifest,
  testStartTime,
  theme,
  data,
  logs,
  lastState,
  conn,
  isRunning,
  onAbort,
  onBack,
}) {
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

  // When viewing a finished run, freeze elapsed at the last data point.
  useEffect(() => {
    if (!isRunning && data.length) {
      setElapsed(data[data.length - 1].t);
    }
  }, [isRunning, data]);

  const target = { id: lastState?.container_id, name: manifest?.target?.id };

  return (
    <>
      <ConnectionBanner kind={conn} onBack={onBack} />
      <div className="monitor-header">
        <div className="info">
          <span className="test-name">{manifest?.test_name}</span>
          <span className="elapsed">{elapsed.toFixed(1)}s elapsed</span>
          <span
            className="tag"
            style={{ background: 'var(--accent-muted)', color: 'var(--accent)' }}
          >
            {lastState?.phase || (isRunning ? 'pending' : 'finished')}
          </span>
        </div>
        {isRunning && (
          <button className="danger" onClick={onAbort}>
            Abort
          </button>
        )}
      </div>
      <div className="split">
        <div className="col">
          <LiveChart title="CPU Usage" data={data} dataKey="cpu" unit="%" themeKey={theme} />
          <LiveChart title="Memory" data={data} dataKey="mem" unit="MB" themeKey={theme} />
          <LiveChart title="Network I/O" data={data} dataKey="net" themeKey={theme} formatter={formatBytesPerSec} />
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
