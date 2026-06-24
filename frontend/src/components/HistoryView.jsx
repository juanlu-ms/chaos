import { useEffect, useState } from 'react';
import { getHistory, getHistoryRun, deleteHistoryRun, clearHistory } from '../api.js';
import { historyRecordToResult } from '../utils/historyTransform.js';
import { shortId } from '../utils/shortId.js';

export default function HistoryView({ onOpenRun, onBack }) {
  const [runs, setRuns] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);

  const fetchList = () => {
    setLoading(true);
    setError(null);
    getHistory()
      .then((data) => {
        const sorted = [...(data || [])].sort((a, b) => (b.started_at_unix || 0) - (a.started_at_unix || 0));
        setRuns(sorted);
        setLoading(false);
      })
      .catch((err) => {
        setError(err.message);
        setLoading(false);
      });
  };

  useEffect(() => { fetchList(); }, []);

  const handleRowClick = async (id) => {
    try {
      const record = await getHistoryRun(id);
      const { manifest, result } = historyRecordToResult(record);
      onOpenRun(manifest, result);
    } catch (err) {
      setError(err.message);
    }
  };

  const handleDelete = async (e, id) => {
    e.stopPropagation();
    try {
      await deleteHistoryRun(id);
    } catch (err) {
      setError(err.message);
    } finally {
      fetchList();
    }
  };

  const handleClear = async () => {
    if (!window.confirm('Delete all run history?')) return;
    try {
      await clearHistory();
      fetchList();
    } catch (err) {
      setError(err.message);
    }
  };

  const formatRunId = (id) => {
    if (!id) return '\u2014';
    return id.startsWith('run-') ? id.slice(4) : id;
  };

  const formatDate = (unixMs) => {
    if (!unixMs) return '\u2014';
    const d = new Date(unixMs);
    return d.toLocaleString();
  };

  return (
    <div>
      <div className="history-header">
        <h2>Run History</h2>
        <div className="history-actions">
          <button onClick={onBack}>Back</button>
          {runs.length > 0 && (
            <button className="danger" onClick={handleClear}>Clear All</button>
          )}
        </div>
      </div>

      {error && <div className="banner error" role="alert">{error}</div>}

      {loading && <div className="empty-hint">Loading history...</div>}

      {!loading && !error && runs.length === 0 && (
        <div className="empty-hint">No runs yet.</div>
      )}

      {runs.length > 0 && (
        <table className="history-table">
          <thead>
            <tr>
              <th>Date/Time</th>
              <th>Test</th>
              <th>Run ID</th>
              <th>Target</th>
              <th>Duration</th>
              <th>Status</th>
              <th>Result</th>
              <th></th>
            </tr>
          </thead>
          <tbody>
            {runs.map((r) => (
              <tr key={r.id} className="history-row" onClick={() => handleRowClick(r.id)}>
                <td>{formatDate(r.started_at_unix)}</td>
                <td>{r.manifest_name || '\u2014'}</td>
                <td title={r.id}>{formatRunId(r.id)}</td>
                <td title={r.target_id}>{shortId(r.target_id || '')}</td>
                <td>{(r.duration_s || 0).toFixed(1)}s</td>
                <td>{r.status}</td>
                <td>
                  <span className={`tag ${r.passed ? 'pass' : 'fail'}`}>
                    {r.passed ? 'Pass' : 'Fail'}
                  </span>
                </td>
                <td>
                  <button className="danger" onClick={(e) => handleDelete(e, r.id)} aria-label="Delete run">&times;</button>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      )}
    </div>
  );
}
