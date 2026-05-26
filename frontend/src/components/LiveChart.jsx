import ChartBase from './ChartBase.jsx';

export function formatLatency(ms) {
  if (ms == null) return '\u2014';
  if (ms < 1) return `${Number(ms).toFixed(3)} ms`;
  if (ms < 100) return `${Number(ms).toFixed(1)} ms`;
  return `${Math.round(ms)} ms`;
}

export function formatBytesPerSec(v) {
  if (v == null) return '0';
  if (v > 1048576) return `${(v / 1048576).toFixed(1)} MB/s`;
  if (v > 1024) return `${(v / 1024).toFixed(1)} KB/s`;
  return `${v.toFixed(0)} B/s`;
}

export default function LiveChart({ data, dataKey, unit, title, themeKey, formatter }) {
  return (
    <ChartBase
      data={data}
      dataKey={dataKey}
      unit={unit}
      title={title}
      formatter={formatter}
      themeKey={themeKey}
    />
  );
}
