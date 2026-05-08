export default function ConnectionBanner({ kind, onBack }) {
  if (!kind) return null;
  if (kind === 'lost') {
    return (
      <div className="banner warn">
        <span>Connection lost — attempting to reconnect…</span>
      </div>
    );
  }
  return (
    <div className="banner error">
      <span>Connection lost permanently</span>
      <button onClick={onBack}>Back to Step 1</button>
    </div>
  );
}
