import ResultsChart from '../components/ResultsChart.jsx';
import { formatBytesPerSec } from '../components/LiveChart.jsx';
import LogViewer from '../components/LogViewer.jsx';
import { shortId } from '../utils/shortId.js';

export default function Step3Results({
  manifest,
  result,
  theme,
  onRunAgain,
  onModify,
}) {
  const data = result?.data || [];
  const logs = result?.logs || [];
  const zones = result?.zones || { normalEnd: 0, chaosEnd: 0 };
  const passed = result?.passed;
  const expectations = result?.results || [];
  const duration = data.length ? data[data.length - 1].t : 0;
  const targetId = manifest?.target?.id || '';
  const pertTypes = (manifest?.perturbations || []).map((p) => p.type);

  return (
    <>
      {result?.error && (
        <div className="banner error" role="alert">
          <span>{result.error}</span>
        </div>
      )}
      <div className="split">
        <div className="col">
          <ResultsChart title="CPU Timeline" data={data} dataKey="cpu" zones={zones} themeKey={theme} unit="%" />
          <ResultsChart title="Memory Timeline" data={data} dataKey="mem" zones={zones} themeKey={theme} unit="MB" />
          <ResultsChart title="Network Timeline" data={data} dataKey="net" zones={zones} themeKey={theme} formatter={formatBytesPerSec} />
        </div>
        <div className="col">
          <div className="panel">
            <h3>Summary</h3>
            <dl className="kv">
              <dt>Test</dt>
              <dd>{manifest?.test_name}</dd>
              <dt>Duration</dt>
              <dd>{duration.toFixed(1)}s</dd>
              <dt>Target</dt>
              <dd title={targetId}>{manifest?.target?.name || shortId(targetId)}</dd>
              <dt>Perturbations</dt>
              <dd>{pertTypes.length} ({pertTypes.join(', ')})</dd>
              <dt>Result</dt>
              <dd>
                <span className={`tag ${passed ? 'pass' : 'fail'}`}>
                  {passed ? 'Passed' : 'Failed'}
                </span>
              </dd>
            </dl>
          </div>

          <div className="panel">
            <h3>Expectations</h3>
            <div className="exp-list">
              {expectations.length === 0 && (
                <div className="empty-hint">No results.</div>
              )}
              {expectations.map((r, i) => (
                <div
                  key={`${r.type}-${i}`}
                  className={`exp-item ${r.passed ? 'passed' : 'failed'}`}
                  title={r.message}
                >
                  <span>{r.type}</span>
                  <span className={`tag ${r.passed ? 'pass' : 'fail'}`}>
                    {r.passed ? 'Pass' : 'Fail'}
                  </span>
                </div>
              ))}
            </div>
          </div>

          <div className="panel">
            <h3>Logs</h3>
            <LogViewer logs={logs} autoScroll={false} />
          </div>

          <div className="actions">
            <button className="primary" onClick={onRunAgain}>Run Again</button>
            <button onClick={onModify}>Modify Manifest</button>
          </div>
        </div>
      </div>
    </>
  );
}
