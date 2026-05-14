import { useId } from 'react';

export default function ConfigRow({ types, getParamsForType, getDefaultParams, value, onChange, onRemove }) {
  const currentParams = getParamsForType(value.type) || [];
  const baseId = useId();

  const setType = (type) => {
    onChange({ type, parameters: getDefaultParams(type) });
  };
  const setParam = (k, v) => {
    onChange({ ...value, parameters: { ...value.parameters, [k]: v } });
  };

  const typeId = `${baseId}-type`;

  return (
    <div className="row">
      <div>
        <label htmlFor={typeId}>Type</label>
        <select id={typeId} value={value.type} onChange={(e) => setType(e.target.value)}>
          {types.map((t) => (
            <option key={t.type} value={t.type}>{t.label}</option>
          ))}
        </select>
      </div>
      <div className="params">
        {currentParams.length === 0 ? (
          <div className="empty-hint">No parameters.</div>
        ) : (
          currentParams.map((p) => {
            const pid = `${baseId}-${p.key}`;
            return (
              <div key={p.key}>
                <label htmlFor={pid}>
                  {p.label}
                  {!p.required && <span className="optional">(optional)</span>}
                </label>
                <input
                  id={pid}
                  value={value.parameters?.[p.key] ?? ''}
                  placeholder={p.placeholder}
                  onChange={(e) => setParam(p.key, e.target.value)}
                />
              </div>
            );
          })
        )}
      </div>
      <button className="remove" onClick={onRemove} title="Remove" aria-label="Remove row">✕</button>
    </div>
  );
}
