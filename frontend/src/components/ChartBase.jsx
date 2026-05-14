import { useId, useMemo } from 'react';
import { AreaChart, Area, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer } from 'recharts';
import { readVar } from '../theme.js';

export function useChartTheme(themeKey) {
  return useMemo(() => ({
    accent: readVar('--accent', '#e6a817'),
    grid: readVar('--border', '#3d3228'),
    text: readVar('--text-dim', '#9e8f7e'),
    surface: readVar('--bg-elevated', '#2d251e'),
    success: readVar('--success', '#4caf50'),
    error: readVar('--error', '#ef5350'),
  }), [themeKey]);
}

export default function ChartBase({ data, dataKey, unit, title, formatter, themeKey, height = 160, domain = [0, 'auto'], children }) {
  const theme = useChartTheme(themeKey);
  const { accent, grid, text, surface } = theme;
  const reactId = useId();
  const id = `grad-${reactId.replace(/:/g, '')}`;
  const yFormat = formatter || ((v) => `${v}${unit ? ' ' + unit : ''}`);

  const tooltipStyle = useMemo(
    () => ({ background: surface, border: `1px solid ${grid}`, borderRadius: 6, fontSize: 11 }),
    [surface, grid]
  );

  return (
    <div className="chart-card">
      {typeof title === 'string' ? <h4>{title}</h4> : title}
      <ResponsiveContainer width="100%" height={height}>
        <AreaChart data={data} margin={{ top: 4, right: 8, left: 0, bottom: 0 }}>
          <defs>
            <linearGradient id={id} x1="0" y1="0" x2="0" y2="1">
              <stop offset="0%" stopColor={accent} stopOpacity={0.5} />
              <stop offset="100%" stopColor={accent} stopOpacity={0.02} />
            </linearGradient>
          </defs>
          <CartesianGrid stroke={grid} strokeDasharray="3 3" />
          <XAxis dataKey="t" type="number" domain={domain} stroke={text} tick={{ fill: text, fontSize: 10 }} tickFormatter={(v) => `${Number(v).toFixed(0)}s`} />
          <YAxis stroke={text} tick={{ fill: text, fontSize: 10 }} tickFormatter={yFormat} width={60} />
          <Tooltip contentStyle={tooltipStyle} labelFormatter={(v) => `t = ${Number(v).toFixed(1)}s`} formatter={(v) => yFormat(v)} />
          {children}
          <Area type="monotone" dataKey={dataKey} stroke={accent} strokeWidth={2} fill={`url(#${id})`} isAnimationActive={false} />
        </AreaChart>
      </ResponsiveContainer>
    </div>
  );
}
