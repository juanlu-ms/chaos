import {
  AreaChart,
  Area,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  ReferenceArea,
  ReferenceLine,
  ResponsiveContainer,
} from 'recharts';
import { readVar } from '../theme.js';

export default function ResultsChart({
  data,
  dataKey,
  title,
  zones,
  themeKey,
  formatter,
  unit,
}) {
  const accent = readVar('--accent', '#e6a817');
  const grid = readVar('--border', '#3d3228');
  const text = readVar('--text-dim', '#9e8f7e');
  const surface = readVar('--bg-elevated', '#2d251e');
  const success = readVar('--success', '#4caf50');
  const error = readVar('--error', '#ef5350');

  const id = `grad-r-${dataKey}-${themeKey}`;
  const yFormat = formatter || ((v) => `${v}${unit ? ' ' + unit : ''}`);

  const tEnd = data.length ? data[data.length - 1].t : 0;
  const normalEnd = zones?.normalEnd ?? 0;
  const chaosEnd = zones?.chaosEnd || tEnd;
  const hasPhases = normalEnd > 0;

  return (
    <div className="chart-card">
      <div className="chart-card-head">
        <h4>{title}</h4>
        {hasPhases && (
          <div className="chart-legend">
            <span className="dot baseline" /> Baseline
            <span className="dot chaos" /> Chaos
            <span className="dot recovery" /> Recovery
          </div>
        )}
      </div>
      <ResponsiveContainer width="100%" height={180}>
        <AreaChart data={data} margin={{ top: 4, right: 8, left: 0, bottom: 0 }}>
          <defs>
            <linearGradient id={id} x1="0" y1="0" x2="0" y2="1">
              <stop offset="0%" stopColor={accent} stopOpacity={0.55} />
              <stop offset="100%" stopColor={accent} stopOpacity={0.02} />
            </linearGradient>
          </defs>
          <CartesianGrid stroke={grid} strokeDasharray="3 3" />
          <XAxis
            dataKey="t"
            type="number"
            domain={['dataMin', 'dataMax']}
            allowDataOverflow={false}
            stroke={text}
            tick={{ fill: text, fontSize: 10 }}
            tickFormatter={(v) => `${Number(v).toFixed(0)}s`}
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

          {hasPhases && (
            <>
              <ReferenceArea
                x1={0}
                x2={normalEnd}
                fill={success}
                fillOpacity={0.1}
                stroke="none"
                ifOverflow="extendDomain"
              />
              <ReferenceArea
                x1={normalEnd}
                x2={chaosEnd}
                fill={error}
                fillOpacity={0.28}
                stroke="none"
                ifOverflow="extendDomain"
              />
              <ReferenceArea
                x1={chaosEnd}
                x2={tEnd}
                fill={success}
                fillOpacity={0.18}
                stroke="none"
                ifOverflow="extendDomain"
              />
              <ReferenceLine
                x={normalEnd}
                stroke={error}
                strokeWidth={1.5}
                strokeOpacity={0.7}
                strokeDasharray="4 3"
              />
              <ReferenceLine
                x={chaosEnd}
                stroke={success}
                strokeWidth={1.5}
                strokeOpacity={0.7}
                strokeDasharray="4 3"
              />
            </>
          )}

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
