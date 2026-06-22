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
      fetchList();
    } catch (err) {
      setError(err.message);
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
                <td>{r.run_result?.manifest_name || r.runResult?.manifest_name || '\u2014'}</td>
                <td title={r.run_result?.target_id || r.runResult?.target_id}>{shortId(r.run_result?.target_id || r.runResult?.target_id || '')}</td>
                <td>{(r.run_result?.duration_s || r.runResult?.duration_s || 0).toFixed(1)}s</td>
                <td>{r.status}</td>
                <td>
                  <span className={`tag ${(r.run_result?.passed || r.runResult?.passed) ? 'pass' : 'fail'}`}>
                    {(r.run_result?.passed || r.runResult?.passed) ? 'Pass' : 'Fail'}
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
