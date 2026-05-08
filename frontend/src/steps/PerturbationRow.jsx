import {
  PERTURBATION_TYPES,
  PERTURBATION_BY_TYPE,
  defaultParamsFor,
} from '../constants/perturbations.js';

export default function PerturbationRow({ value, onChange, onRemove }) {
  const def = PERTURBATION_BY_TYPE[value.type] || PERTURBATION_TYPES[0];

  const setType = (type) => {
    onChange({ type, parameters: defaultParamsFor(type) });
  };
  const setParam = (k, v) => {
    onChange({ ...value, parameters: { ...value.parameters, [k]: v } });
  };

  return (
    <div className="row">
      <div>
        <label>Type</label>
        <select value={value.type} onChange={(e) => setType(e.target.value)}>
          {PERTURBATION_TYPES.map((t) => (
            <option key={t.type} value={t.type}>
              {t.label}
            </option>
          ))}
        </select>
      </div>
      <div className="params">
        {def.params.length === 0 ? (
          <div className="empty-hint">No parameters.</div>
        ) : (
          def.params.map((p) => (
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
