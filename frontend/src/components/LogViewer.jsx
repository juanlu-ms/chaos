import { useEffect, useRef } from 'react';

export default function LogViewer({ logs, autoScroll = true }) {
  const ref = useRef(null);
  useEffect(() => {
    if (autoScroll && ref.current) {
      ref.current.scrollTop = ref.current.scrollHeight;
    }
  }, [logs, autoScroll]);

  return (
    <div
      className="log-viewer"
      ref={ref}
      role="log"
      aria-live="polite"
      aria-relevant="additions"
    >
      {logs.length === 0 ? (
        <div className="line">No logs yet.</div>
      ) : (
        logs.map((l, i) => (
          <div className="line" key={i}>
            {l}
          </div>
        ))
      )}
    </div>
  );
}
