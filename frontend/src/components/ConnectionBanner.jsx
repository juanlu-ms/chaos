export default function ConnectionBanner({ kind, onBack }) {
  if (!kind) return null;
  if (kind === 'lost') {
    return (
      <div className="banner warn" role="status" aria-live="polite">
        <span>Connection lost — attempting to reconnect…</span>
      </div>
    );
  }
  return (
    <div className="banner error" role="alert">
      <span>Connection lost permanently</span>
      <button onClick={onBack}>Back to Step 1</button>
    </div>
  );
}
