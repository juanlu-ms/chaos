import { useEffect, useId, useRef, useState } from 'react';
import { getTargets, getLimits, runManifest } from '../api.js';
import PerturbationRow from './PerturbationRow.jsx';
import ExpectationRow from './ExpectationRow.jsx';
import { defaultParamsFor } from '../constants/perturbations.js';
import { defaultExpectationParams } from '../constants/expectations.js';
import { validateManifestAll } from '../utils/validateManifest.js';
import { stringifyParams } from '../utils/stringifyParams.js';
import { uid } from '../utils/uid.js';

const withId = (item) => ({ _id: uid('row'), ...item });

export default function Step1Config({ initial, onRun }) {
  const [testName, setTestName] = useState(initial?.test_name || 'my-test');
  const [targetId, setTargetId] = useState(initial?.target?.id || '');
  const [duration, setDuration] = useState(initial?.duration_s || 15);
  const [perturbations, setPerturbations] = useState(() =>
    (initial?.perturbations?.length
      ? initial.perturbations
      : [{ type: 'cpu_cap', parameters: defaultParamsFor('cpu_cap') }]
    ).map(withId)
  );
  const [expectations, setExpectations] = useState(() =>
    (initial?.expectations?.length
      ? initial.expectations
      : [{ type: 'container_running', parameters: defaultExpectationParams('container_running') }]
    ).map(withId)
  );
  const [targets, setTargets] = useState([]);
  const [limits, setLimits] = useState(null);
  const [errors, setErrors] = useState([]);
  const [submitting, setSubmitting] = useState(false);
  const [loadingTargets, setLoadingTargets] = useState(true);

  const targetIdRef = useRef(targetId);
  useEffect(() => { targetIdRef.current = targetId; }, [targetId]);

  const nameId = useId();
  const targetSelectId = useId();
  const durationId = useId();

  useEffect(() => {
    let cancelled = false;
    setLoadingTargets(true);
    Promise.all([getTargets(), getLimits()])
      .then(([t, l]) => {
        if (cancelled) return;
        setTargets(t);
        setLimits(l);
        if (!targetIdRef.current && t.length) setTargetId(t[0].id);
      })
      .catch((e) => {
        if (cancelled) return;
        setErrors([`Failed to load targets: ${e.message}`]);
      })
      .finally(() => { if (!cancelled) setLoadingTargets(false); });
    return () => { cancelled = true; };
  }, []);

  const submit = async () => {
    const errs = validateManifestAll({ testName, targetId, duration, perturbations, expectations });
    if (errs.length) {
      setErrors(errs);
      return;
    }
    setErrors([]);
    setSubmitting(true);
    const selectedTarget = targets.find((t) => t.id === targetId);
    const manifest = {
      test_name: testName.trim(),
      target: { id: targetId, name: selectedTarget?.name },
      duration_s: Number(duration),
      perturbations: perturbations.map(({ _id, ...p }) => ({
        type: p.type,
        parameters: stringifyParams(p.parameters),
      })),
      expectations: expectations.map(({ _id, ...e }) => ({
        type: e.type,
        parameters: stringifyParams(e.parameters),
      })),
    };
    try {
      await runManifest(manifest);
      onRun(manifest);
    } catch (e) {
      setErrors([`Failed to start: ${e.message}`]);
    } finally {
      setSubmitting(false);
    }
  };

  return (
    <>
      {errors.length > 0 && (
        <div className="banner error" role="alert">
          {errors.length === 1 ? (
            <span>{errors[0]}</span>
          ) : (
            <ul style={{ paddingLeft: 18, margin: 0 }}>
              {errors.map((e, i) => <li key={i}>{e}</li>)}
            </ul>
          )}
        </div>
      )}
      <div className="form-section">
        <h2>Test Setup</h2>
        <div className="form-grid">
          <div>
            <label htmlFor={nameId}>Test name</label>
            <input id={nameId} value={testName} onChange={(e) => setTestName(e.target.value)} />
          </div>
          <div>
            <label htmlFor={targetSelectId}>Target container</label>
            <select
              id={targetSelectId}
              value={targetId}
              onChange={(e) => setTargetId(e.target.value)}
              disabled={loadingTargets}
            >
              <option value="">{loadingTargets ? '— loading… —' : '— select —'}</option>
              {targets.map((t) => (
                <option key={t.id} value={t.id}>
                  {t.name} ({t.state})
                </option>
              ))}
            </select>
          </div>
          <div>
            <label htmlFor={durationId}>Duration (s)</label>
            <input
              id={durationId}
              type="number"
              min="1"
              value={duration}
              onChange={(e) => setDuration(e.target.value)}
            />
          </div>
        </div>
        {limits && (
          <div className="form-hint">
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
                withId({ type: 'cpu_cap', parameters: defaultParamsFor('cpu_cap') }),
              ])
            }
          >
            + Add Perturbation
          </button>
        </h2>
        {perturbations.map((p, i) => (
          <PerturbationRow
            key={p._id}
            value={p}
            onChange={(np) => {
              const next = [...perturbations];
              next[i] = { ...np, _id: p._id };
              setPerturbations(next);
            }}
            onRemove={() => setPerturbations(perturbations.filter((_, j) => j !== i))}
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
                withId({
                  type: 'container_running',
                  parameters: defaultExpectationParams('container_running'),
                }),
              ])
            }
          >
            + Add Expectation
          </button>
        </h2>
        {expectations.map((e, i) => (
          <ExpectationRow
            key={e._id}
            value={e}
            onChange={(ne) => {
              const next = [...expectations];
              next[i] = { ...ne, _id: e._id };
              setExpectations(next);
            }}
            onRemove={() => setExpectations(expectations.filter((_, j) => j !== i))}
          />
        ))}
      </div>

      <button className="primary" disabled={submitting} onClick={submit}>
        {submitting ? 'Starting…' : 'Run Chaos Test'}
      </button>
    </>
  );
}
