export default function ConfigRow({ types, getParamsForType, getDefaultParams, value, onChange, onRemove }) {
  const currentParams = getParamsForType(value.type) || [];

  const setType = (type) => {
    onChange({ type, parameters: getDefaultParams(type) });
  };
  const setParam = (k, v) => {
    onChange({ ...value, parameters: { ...value.parameters, [k]: v } });
  };

  return (
    <div className="row">
      <div>
        <label>Type</label>
        <select value={value.type} onChange={(e) => setType(e.target.value)}>
          {types.map((t) => (
            <option key={t.type} value={t.type}>{t.label}</option>
          ))}
        </select>
      </div>
      <div className="params">
        {currentParams.length === 0 ? (
          <div className="empty-hint">No parameters.</div>
        ) : (
          currentParams.map((p) => (
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
      <button className="remove" onClick={onRemove} title="Remove">✕</button>
    </div>
  );
}
