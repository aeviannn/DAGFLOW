import { Clock3, Gauge, Layers3, Zap } from 'lucide-react';

const metrics = {
  makespan: { label: 'Makespan', icon: Clock3, unit: 'time units', color: 'violet' },
  total_busy_time: { label: 'Total busy time', icon: Layers3, unit: 'time units', color: 'teal' },
  worker_utilization: { label: 'Worker utilization', icon: Gauge, unit: 'of total capacity', color: 'blue' },
  throughput: { label: 'Throughput', icon: Zap, unit: 'tasks per time unit', color: 'amber' },
};

export function MetricCard({ metric, value }) {
  const config = metrics[metric];
  if (!config) return null;
  const Icon = config.icon;
  const display = metric === 'worker_utilization' ? `${(value * 100).toFixed(1)}%`
    : metric === 'throughput' ? Number(value).toFixed(2) : value;
  return <article className={`metric-card metric-${config.color}`}>
    <div className="metric-topline"><span className="metric-icon"><Icon size={16} /></span></div>
    <div className="metric-value">{display}<span className="metric-unit">{config.unit}</span></div>
    <div className="metric-label">{config.label}</div>
  </article>;
}
