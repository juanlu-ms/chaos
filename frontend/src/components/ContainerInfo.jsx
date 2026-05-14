import { shortId } from '../utils/shortId.js';

export default function ContainerInfo({ state, target }) {
  const id = state?.container_id || target?.id || '';
  return (
    <div className="panel">
      <h3>Container</h3>
      <dl className="kv">
        <dt>Name</dt>
        <dd>{target?.name || '—'}</dd>
        <dt>ID</dt>
        <dd title={id || '—'}>{shortId(id)}</dd>
        <dt>Status</dt>
        <dd>{state?.status || '—'}</dd>
        <dt>Phase</dt>
        <dd>{state?.phase || '—'}</dd>
        <dt>CPU</dt>
        <dd>
          {state?.cpu_usage_percent != null
            ? `${state.cpu_usage_percent.toFixed(1)} %`
            : '—'}
        </dd>
        <dt>Memory</dt>
        <dd>
          {state?.memory_usage_mb != null
            ? `${state.memory_usage_mb.toFixed(1)} MB`
            : '—'}
        </dd>
      </dl>
    </div>
  );
}
