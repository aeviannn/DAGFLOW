import { useMemo } from 'react';
import { Activity, Clock3 } from 'lucide-react';
import { getPeakConcurrency } from '../services/executionInsights.js';

const colors = ['#8d77de', '#46a99b', '#d59a52', '#5d91c9', '#c7789c', '#78a96d', '#a47bc4'];
const chartWidth = 900;
const left = 86;
const right = 18;
const plotWidth = chartWidth - left - right;

export default function GanttTimeline({ simulation }) {
  const executions = simulation?.executions ?? [];
  const workers = simulation?.worker_count ?? 0;
  const makespan = simulation?.metrics?.makespan ?? 0;
  const peakWorkers = getPeakConcurrency(executions);
  const scaleEnd = makespan > 0 ? makespan : 1;
  const chartHeight = 60 + workers * 50;
  const ticks = useMemo(() => Array.from({ length: 6 }, (_, index) => ({ value: makespan * index / 5, x: left + plotWidth * index / 5 })), [makespan]);

  return <section className="surface timeline-panel">
    <div className="panel-heading"><div className="panel-title-wrap"><span className="panel-icon teal-icon"><Activity size={16} /></span>
      <div><h2>Worker timeline</h2><p>{simulation ? `${executions.length} task executions across ${workers} worker${workers === 1 ? '' : 's'}` : 'Schedule visualization'}</p></div></div>
      {simulation && <div className="timeline-meta"><Clock3 size={14} /><span>Makespan <b>{makespan}</b></span><span className="timeline-meta-divider" />
        <span>Peak concurrency <b>{peakWorkers} {peakWorkers === 1 ? 'worker' : 'workers'}</b></span></div>}</div>
    {!simulation ? <div className="empty-timeline">Run the engine to see task allocation over simulated time.</div> : executions.length === 0 ?
      <div className="empty-timeline"><span className="empty-ring"><Activity size={18} /></span>This workflow has no task executions.</div> :
      <div className="timeline-scroll"><svg className="timeline-svg" viewBox={`0 0 ${chartWidth} ${chartHeight}`} role="img" aria-label="Worker execution Gantt chart">
        {ticks.map((tick, index) => <g key={index}><line x1={tick.x} y1="28" x2={tick.x} y2={chartHeight - 16} className="timeline-gridline" />
          <text x={tick.x} y="17" textAnchor={index === 0 ? 'start' : index === ticks.length - 1 ? 'end' : 'middle'} className="timeline-tick">{Number.isInteger(tick.value) ? tick.value : tick.value.toFixed(1)}</text></g>)}
        {Array.from({ length: workers }, (_, worker) => {
          const y = 36 + worker * 50;
          return <g key={worker}><text x="4" y={y + 17} className="worker-label">WORKER {worker}</text>
            <line x1={left} y1={y + 25} x2={chartWidth - right} y2={y + 25} className="worker-baseline" />
            {executions.filter((entry) => entry.worker_id === worker).map((entry) => {
              const x = left + entry.start_time / scaleEnd * plotWidth;
              const rawWidth = entry.duration / scaleEnd * plotWidth;
              const width = entry.duration === 0 ? 2 : Math.max(5, rawWidth);
              const fill = colors[(entry.task_id >>> 0) % colors.length];
              const tooltip = `Task ${entry.task_id} · Worker ${worker} · ${entry.start_time}–${entry.completion_time} · ${entry.duration} time units`;
              return <g key={`${entry.task_id}-${entry.start_time}`}><title>{tooltip}</title>
                <rect x={x} y={y} width={width} height="28" rx="7" fill={fill} className="gantt-block" />
                {width > 42 ? <text x={x + width / 2} y={y + 18} textAnchor="middle" className="gantt-label">T{entry.task_id}</text>
                  : <text x={Math.min(x + width + 5, chartWidth - right - 10)} y={y + 18} className="gantt-short-label">T{entry.task_id}</text>}
              </g>;
            })}</g>;
        })}
        <line x1={left} y1={chartHeight - 15} x2={chartWidth - right} y2={chartHeight - 15} className="timeline-axis" />
      </svg></div>}
    <div className="timeline-footer"><span>SIMULATED TIME</span><span><i className="timeline-color-key" /> Task duration</span><span className="footer-note">Bars are scaled to makespan · hover for execution details</span></div>
  </section>;
}
