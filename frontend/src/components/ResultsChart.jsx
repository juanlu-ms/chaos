import { ReferenceArea, ReferenceLine } from 'recharts';
import ChartBase, { useChartTheme } from './ChartBase.jsx';

export default function ResultsChart({ data, dataKey, title, zones, themeKey, formatter, unit }) {
  const { success, error } = useChartTheme();

  const tEnd = data.length ? data[data.length - 1].t : 0;
  const normalEnd = zones?.normalEnd ?? 0;
  const chaosEnd = zones?.chaosEnd || tEnd;
  const hasPhases = normalEnd > 0;

  const chartTitle = hasPhases ? (
    <div className="chart-card-head">
      <h4 style={{ margin: 0 }}>{title}</h4>
      <div className="chart-legend">
        <span className="dot baseline" /> Baseline
        <span className="dot chaos" /> Chaos
        <span className="dot recovery" /> Recovery
      </div>
    </div>
  ) : title;

  return (
    <ChartBase
      data={data}
      dataKey={dataKey}
      unit={unit}
      title={chartTitle}
      formatter={formatter}
      themeKey={themeKey}
      height={180}
      domain={['dataMin', 'dataMax']}
    >
      {hasPhases && (
        <>
          <ReferenceArea x1={0} x2={normalEnd} fill={success} fillOpacity={0.1} stroke="none" ifOverflow="extendDomain" />
          <ReferenceArea x1={normalEnd} x2={chaosEnd} fill={error} fillOpacity={0.28} stroke="none" ifOverflow="extendDomain" />
          <ReferenceArea x1={chaosEnd} x2={tEnd} fill={success} fillOpacity={0.18} stroke="none" ifOverflow="extendDomain" />
          <ReferenceLine x={normalEnd} stroke={error} strokeWidth={1.5} strokeOpacity={0.7} strokeDasharray="4 3" />
          <ReferenceLine x={chaosEnd} stroke={success} strokeWidth={1.5} strokeOpacity={0.7} strokeDasharray="4 3" />
        </>
      )}
    </ChartBase>
  );
}
