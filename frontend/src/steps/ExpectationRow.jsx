import {
  EXPECTATION_TYPES,
  EXPECTATION_PARAMS,
  defaultExpectationParams,
} from '../constants/expectations.js';

export default function ExpectationRow({ value, onChange, onRemove }) {
  const params = EXPECTATION_PARAMS[value.type] || [];

  const setType = (type) => {
    onChange({ type, parameters: defaultExpectationParams(type) });
  };
  const setParam = (k, v) => {
    onChange({ ...value, parameters: { ...value.parameters, [k]: v } });
  };

  return (
    <div className="row">
      <div>
        <label>Type</label>
        <select value={value.type} onChange={(e) => setType(e.target.value)}>
          {EXPECTATION_TYPES.map((t) => (
            <option key={t.type} value={t.type}>
              {t.label}
            </option>
          ))}
        </select>
      </div>
      <div className="params">
        {params.length === 0 ? (
          <div className="empty-hint">No parameters.</div>
        ) : (
          params.map((p) => (
            <div key={p.key}>
              <label>
                {p.label}
                {!p.required && <span className="optional">(optional)</span>}
              </label>
              <input
                value={value.parameters?.[p.key] ?? ''}
                placeholder={p.placeholder}
                onChange={(e) => setParam(p.key, e.target.value)}
              />
            </div>
          ))
        )}
      </div>
      <button className="remove" onClick={onRemove} title="Remove">
        ✕
      </button>
    </div>
  );
}
