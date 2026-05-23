import LiveChart, { formatBytesPerSec, formatLatency } from '../components/LiveChart.jsx';
import LogViewer from '../components/LogViewer.jsx';
import ContainerInfo from '../components/ContainerInfo.jsx';
import ConnectionBanner from '../components/ConnectionBanner.jsx';
import { useElapsedTimer } from '../hooks/useElapsedTimer.js';

export default function Step2Monitor({
  manifest,
  testStartTime,
  theme,
  data,
  logs,
  lastState,
  conn,
  continuousFailures,
  isRunning,
  onAbort,
  onBack,
}) {
  const elapsed = useElapsedTimer(isRunning, testStartTime, data.length ? data[data.length - 1].t : null);

  const target = {
    id: lastState?.container_id,
    name: manifest?.target?.name || manifest?.target?.id,
  };
  const phase = lastState?.phase || (isRunning ? 'pending' : 'finished');

  return (
    <>
      <ConnectionBanner kind={conn} onBack={onBack} />
      {continuousFailures.length > 0 && (
        <div className="banner error" role="alert">
          <strong>Continuous expectation{continuousFailures.length > 1 ? 's' : ''} failed:</strong>{' '}
          {continuousFailures.join(', ')}
        </div>
      )}
      <div className="monitor-header">
        <div className="info">
          <div className="info-row">
            <span className="test-name">{manifest?.test_name}</span>
            <span className="tag tag-phase">{phase}</span>
            <span className="elapsed">{elapsed.toFixed(1)}s elapsed</span>
          </div>
          <div className="target-name">
            container: <strong>{target.name || '—'}</strong>
          </div>
        </div>
        {isRunning ? (
          <button className="danger" onClick={onAbort}>
            Abort
          </button>
        ) : (
          <span className="tag tag-completed">completed</span>
        )}
      </div>
      <div className="split">
        <div className="col">
          {data.length === 0 && isRunning && (
            <div className="panel" role="status" style={{ fontSize: 12, color: 'var(--text-dim)' }}>
              Waiting for first sample…
            </div>
          )}
          <LiveChart title="CPU Usage" data={data} dataKey="cpu" unit="%" themeKey={theme} />
          <LiveChart title="Memory" data={data} dataKey="mem" unit="MB" themeKey={theme} />
          <LiveChart title="Network I/O" data={data} dataKey="net" themeKey={theme} formatter={formatBytesPerSec} />
          <LiveChart title="Network Latency" data={data} dataKey="lat" themeKey={theme} formatter={formatLatency} />
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
