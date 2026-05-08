import {
  AreaChart,
  Area,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  ResponsiveContainer,
} from 'recharts';
import { readVar } from '../theme.js';

export function formatBytesPerSec(v) {
  if (v == null) return '0';
  if (v > 1048576) return `${(v / 1048576).toFixed(1)} MB/s`;
  if (v > 1024) return `${(v / 1024).toFixed(1)} KB/s`;
  return `${v.toFixed(0)} B/s`;
}

export default function LiveChart({
  data,
  dataKey,
  unit,
  title,
  windowSize = 60,
  themeKey,
  formatter,
}) {
  const accent = readVar('--accent', '#e6a817');
  const grid = readVar('--border', '#3d3228');
  const text = readVar('--text-dim', '#9e8f7e');
  const surface = readVar('--bg-elevated', '#2d251e');

  const slice = data.length > windowSize ? data.slice(-windowSize) : data;
  const id = `grad-${dataKey}-${themeKey}`;

  const yFormat = formatter || ((v) => `${v}${unit ? ' ' + unit : ''}`);

  return (
    <div className="chart-card">
      <h4>{title}</h4>
      <ResponsiveContainer width="100%" height={160}>
        <AreaChart data={slice} margin={{ top: 4, right: 8, left: 0, bottom: 0 }}>
          <defs>
            <linearGradient id={id} x1="0" y1="0" x2="0" y2="1">
              <stop offset="0%" stopColor={accent} stopOpacity={0.5} />
              <stop offset="100%" stopColor={accent} stopOpacity={0.02} />
            </linearGradient>
          </defs>
          <CartesianGrid stroke={grid} strokeDasharray="3 3" />
          <XAxis
            dataKey="t"
            stroke={text}
            tick={{ fill: text, fontSize: 10 }}
            tickFormatter={(v) => `${v.toFixed(0)}s`}
          />
          <YAxis
            stroke={text}
            tick={{ fill: text, fontSize: 10 }}
            tickFormatter={yFormat}
            width={60}
          />
          <Tooltip
            contentStyle={{
              background: surface,
              border: `1px solid ${grid}`,
              borderRadius: 6,
              fontSize: 11,
            }}
            labelFormatter={(v) => `t = ${Number(v).toFixed(1)}s`}
            formatter={(v) => yFormat(v)}
          />
          <Area
            type="monotone"
            dataKey={dataKey}
            stroke={accent}
            strokeWidth={2}
            fill={`url(#${id})`}
            isAnimationActive={false}
          />
        </AreaChart>
      </ResponsiveContainer>
    </div>
  );
}
