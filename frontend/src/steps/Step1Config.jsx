import { useEffect, useState } from 'react';
import { getTargets, getLimits, runManifest } from '../api.js';
import PerturbationRow from './PerturbationRow.jsx';
import ExpectationRow from './ExpectationRow.jsx';
import {
  PERTURBATION_TYPES,
  defaultParamsFor,
} from '../constants/perturbations.js';
import {
  EXPECTATION_TYPES,
  defaultExpectationParams,
} from '../constants/expectations.js';

export default function Step1Config({ initial, onRun, setOnline }) {
  const [testName, setTestName] = useState(initial?.test_name || 'my-test');
  const [targetId, setTargetId] = useState(initial?.target?.id || '');
  const [duration, setDuration] = useState(initial?.duration_s || 15);
  const [perturbations, setPerturbations] = useState(
    initial?.perturbations?.length
      ? initial.perturbations
      : [{ type: 'cpu_cap', parameters: defaultParamsFor('cpu_cap') }]
  );
  const [expectations, setExpectations] = useState(
    initial?.expectations?.length
      ? initial.expectations
      : [
          {
            type: 'container_running',
            parameters: defaultExpectationParams('container_running'),
          },
        ]
  );
  const [targets, setTargets] = useState([]);
  const [limits, setLimits] = useState(null);
  const [error, setError] = useState(null);
  const [submitting, setSubmitting] = useState(false);

  useEffect(() => {
    let cancelled = false;
    Promise.all([getTargets(), getLimits()])
      .then(([t, l]) => {
        if (cancelled) return;
        setTargets(t);
        setLimits(l);
        setOnline(true);
        if (!targetId && t.length) setTargetId(t[0].id);
      })
      .catch(() => setOnline(false));
    return () => {
      cancelled = true;
    };
    // eslint-disable-next-line
  }, []);

  const validate = () => {
    if (!testName.trim()) return 'Test name is required.';
    if (!targetId) return 'Select a target container.';
    if (!duration || duration <= 0) return 'Duration must be positive.';
    if (!perturbations.length) return 'Add at least one perturbation.';
    if (!expectations.length) return 'Add at least one expectation.';
    for (const p of perturbations) {
      const def = PERTURBATION_TYPES.find((t) => t.type === p.type);
      for (const param of def?.params || []) {
        if (param.required && !p.parameters?.[param.key]) {
          return `Perturbation "${p.type}" missing "${param.label}".`;
        }
      }
    }
    for (const e of expectations) {
      const def = EXPECTATION_TYPES.find((t) => t.type === e.type);
      for (const param of def?.params || []) {
        if (param.required && !e.parameters?.[param.key]) {
          return `Expectation "${e.type}" missing "${param.label}".`;
        }
      }
    }
    return null;
  };

  const submit = async () => {
    const err = validate();
    if (err) {
      setError(err);
      return;
    }
    setError(null);
    setSubmitting(true);
    const manifest = {
      test_name: testName.trim(),
      target: { id: targetId },
      duration_s: Number(duration),
      perturbations: perturbations.map((p) => ({
        type: p.type,
        parameters: stringifyParams(p.parameters),
      })),
      expectations: expectations.map((e) => ({
        type: e.type,
        parameters: stringifyParams(e.parameters),
      })),
    };
    try {
      await runManifest(manifest);
      onRun(manifest);
    } catch (e) {
      setError(`Failed to start: ${e.message}`);
    } finally {
      setSubmitting(false);
    }
  };

  return (
    <>
      {error && (
        <div className="banner error">
          <span>{error}</span>
        </div>
      )}
      <div className="form-section">
        <h2>Test Setup</h2>
        <div className="form-grid">
          <div>
            <label>Test name</label>
            <input
              value={testName}
              onChange={(e) => setTestName(e.target.value)}
            />
          </div>
          <div>
            <label>Target container</label>
            <select
              value={targetId}
              onChange={(e) => setTargetId(e.target.value)}
            >
              <option value="">— select —</option>
              {targets.map((t) => (
                <option key={t.id} value={t.id}>
                  {t.name} ({t.state})
                </option>
              ))}
            </select>
          </div>
          <div>
            <label>Duration (s)</label>
            <input
              type="number"
              min="1"
              value={duration}
              onChange={(e) => setDuration(e.target.value)}
            />
          </div>
        </div>
        {limits && (
          <div style={{ marginTop: 10, fontSize: 11, color: 'var(--text-dim)' }}>
            System: {limits.cpu_cores} cores · {limits.memory_total_mb} MB total
          </div>
        )}
      </div>

      <div className="form-section">
        <h2>
          Perturbations
          <button
            onClick={() =>
              setPerturbations([
                ...perturbations,
                { type: 'cpu_cap', parameters: defaultParamsFor('cpu_cap') },
              ])
            }
          >
            + Add Perturbation
          </button>
        </h2>
        {perturbations.map((p, i) => (
          <PerturbationRow
            key={i}
            value={p}
            onChange={(np) => {
              const next = [...perturbations];
              next[i] = np;
              setPerturbations(next);
            }}
            onRemove={() =>
              setPerturbations(perturbations.filter((_, j) => j !== i))
            }
          />
        ))}
      </div>

      <div className="form-section">
        <h2>
          Expectations
          <button
            onClick={() =>
              setExpectations([
                ...expectations,
                {
                  type: 'container_running',
                  parameters: defaultExpectationParams('container_running'),
                },
              ])
            }
          >
            + Add Expectation
          </button>
        </h2>
        {expectations.map((e, i) => (
          <ExpectationRow
            key={i}
            value={e}
            onChange={(ne) => {
              const next = [...expectations];
              next[i] = ne;
              setExpectations(next);
            }}
            onRemove={() =>
              setExpectations(expectations.filter((_, j) => j !== i))
            }
          />
        ))}
      </div>

      <button className="primary" disabled={submitting} onClick={submit}>
        {submitting ? 'Starting…' : 'Run Chaos Test'}
      </button>
    </>
  );
}

function stringifyParams(p) {
  const out = {};
  for (const [k, v] of Object.entries(p || {})) {
    if (v === '' || v == null) continue;
    out[k] = String(v);
  }
  return out;
}
