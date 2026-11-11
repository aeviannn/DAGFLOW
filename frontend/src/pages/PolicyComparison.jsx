import { ArrowDownRight, ArrowRight, Award, BarChart3, Gauge, Zap } from 'lucide-react';
import PageHeader from '../components/PageHeader.jsx';
import { MetricCard } from '../components/MetricCard.jsx';

const policyInfo = {
  fcfs: { name: 'FCFS', full: 'First Come, First Served', color: '#8d77de', initials: 'F' },
  sjf: { name: 'SJF', full: 'Shortest Job First', color: '#43a99a', initials: 'S' },
  cpf: { name: 'CPF', full: 'Critical Path First', color: '#d39b52', initials: 'C' },
};

function ComparisonBars({ results }) {
  const max = Math.max(1, ...results.map((result) => result.metrics.makespan));
  return <div className="comparison-bars" role="img" aria-label="Makespan comparison by scheduling policy">
    {results.map((result) => {
      const info = policyInfo[result.policy];
      const width = result.metrics.makespan === 0 ? 2 : Math.max(3, result.metrics.makespan / max * 100);
      return <div className="bar-row" key={result.policy}><span className="bar-name">{info.name}</span>
        <div className="bar-track"><div className="bar-fill" style={{ width: `${width}%`, background: info.color }} /></div>
        <strong className="bar-value">{result.metrics.makespan}<small> units</small></strong></div>;
    })}
    <div className="bar-axis"><span>0</span><span>{max} time units</span></div>
  </div>;
}

export default function PolicyComparison({ workflows, workflow, setWorkflow, workers, setWorkers,
                                          comparison, busy, error, onCompare, onClearError }) {
  const selected = workflows.find((item) => item.id === workflow);
  const current = comparison?.workflow?.id === workflow && comparison?.worker_count === workers ? comparison : null;
  const results = current?.results ?? [];
  const best = results.length ? Math.min(...results.map((result) => result.metrics.makespan)) : null;
  const winners = results.filter((result) => result.metrics.makespan === best);
  const winnerLabel = winners.map((result) => policyInfo[result.policy].name).join(' & ');

  return <div className="page-stack">
    <PageHeader eyebrow="Analysis" title="Policy comparison"
      description="Run every scheduling strategy against the same graph and worker pool."
      action={<span className="comparison-tag"><BarChart3 size={14} /> SIDE-BY-SIDE ANALYSIS</span>} />
    <section className="compare-controls surface">
      <div className="compare-context"><span className="compare-context-icon"><BarChart3 size={17} /></span><div><span className="field-caption">WORKFLOW CONTEXT</span>
        <select aria-label="Comparison workflow" value={workflow} onChange={(event) => setWorkflow(event.target.value)}>
          {workflows.map((item) => <option key={item.id} value={item.id}>{item.name}</option>)}
        </select><span className="context-detail">{selected?.task_count ?? 0} tasks · {workers} workers</span></div></div>
      <div className="compare-worker"><span className="field-caption">WORKER POOL</span><div className="worker-pills">
        {[1, 2, 4, 8].map((count) => <button key={count} className={workers === count ? 'selected' : ''} onClick={() => setWorkers(count)}>{count}</button>)}
        <input aria-label="Custom worker count" type="number" min="1" max="32" value={workers} onChange={(event) => setWorkers(Math.max(1, Math.min(32, Number(event.target.value) || 1)))} />
      </div></div>
      <button className="run-button compare-run" onClick={onCompare} disabled={busy || workflows.length === 0}>
        {busy ? <span className="button-spinner" /> : <BarChart3 size={16} />}{busy ? 'Comparing…' : 'Compare policies'}
      </button>
    </section>
    {error && <div className="alert alert-error" role="alert"><span>{error}</span><button onClick={onClearError}>Dismiss</button></div>}
    {!current ? <div className="compare-empty surface"><div className="compare-empty-icon"><BarChart3 size={24} /></div><h2>See how each policy performs.</h2>
      <p>All three strategies will run on <strong>{selected?.name ?? 'your workflow'}</strong> with the same {workers}-worker pool. Metrics and speedup come directly from DAGFlow.</p>
      <button className="text-action" onClick={onCompare} disabled={busy || workflows.length === 0}>{busy ? 'Running comparisons…' : 'Run policy comparison'}<ArrowRight size={15} /></button></div> : <>
      <div className="policy-cards">{results.map((result) => { const info = policyInfo[result.policy]; const isBest = result.metrics.makespan === best;
        return <article className={`policy-card surface policy-${result.policy}`} key={result.policy}>
          <div className="policy-card-head"><div className="policy-mark" style={{ color: info.color, background: `${info.color}15` }}>{info.initials}</div>
            <div><h3>{info.name}</h3><span>{info.full}</span></div>{isBest && <span className="best-badge"><Award size={13} /> FASTEST</span>}</div>
          <div className="policy-makespan"><strong>{result.metrics.makespan}</strong><span>time units</span></div>
          <div className="policy-secondary"><span><Gauge size={14} />{(result.metrics.worker_utilization * 100).toFixed(1)}% utilized</span>
            <span><Zap size={14} />{result.metrics.throughput.toFixed(2)} tasks/time</span></div>
          <div className="policy-speedup"><span>Speedup vs. one worker</span><strong>{result.metrics.speedup == null ? '—' : `${result.metrics.speedup.toFixed(2)}×`}</strong></div>
        </article>;
      })}</div>
      <section className="surface chart-panel"><div className="panel-heading"><div className="panel-title-wrap"><span className="panel-icon violet-icon"><BarChart3 size={16} /></span>
        <div><h2>Makespan by policy</h2><p>Lower values indicate a shorter overall schedule.</p></div></div><span className="chart-context">{current.workflow.name} · {current.worker_count} workers</span></div>
        <ComparisonBars results={results} /></section>
      <div className="insight-panel"><div className="insight-icon"><Award size={17} /></div><div><span className="insight-kicker">SCHEDULE INSIGHT</span>
        <p>{winners.length === results.length ? 'All three policies produce the same makespan for this workflow.' : <>{winnerLabel} {winners.length === 1 ? 'achieves' : 'share'} the shortest makespan at <strong>{best} time units</strong>.</>}</p>
        <span className="insight-caption">Based on the completed schedules returned by the C++ engine.</span></div>
        <div className="insight-arrow"><ArrowDownRight size={17} /></div></div>
      <section className="surface table-panel"><div className="panel-heading"><div className="panel-title-wrap"><div><h2>Detailed comparison</h2><p>Measured on the same workflow and worker count.</p></div></div><span className="table-count">{results.length} policies</span></div>
        <div className="table-scroll"><table><thead><tr><th>POLICY</th><th>MAKESPAN</th><th>BUSY TIME</th><th>UTILIZATION</th><th>THROUGHPUT</th><th>SPEEDUP</th></tr></thead>
          <tbody>{results.map((result) => <tr key={result.policy}><td><span className={`policy-dot dot-${result.policy}`} />{policyInfo[result.policy].full}</td>
            <td className="numeric-cell">{result.metrics.makespan}</td><td className="numeric-cell">{result.metrics.total_busy_time}</td>
            <td className="numeric-cell">{(result.metrics.worker_utilization * 100).toFixed(1)}%</td>
            <td className="numeric-cell">{result.metrics.throughput.toFixed(2)}</td>
            <td className="numeric-cell speedup-cell">{result.metrics.speedup == null ? '—' : `${result.metrics.speedup.toFixed(2)}×`}</td></tr>)}</tbody></table></div>
      </section>
      {workers === 1 && <p className="speedup-note"><ArrowDownRight size={14} /> Speedup is shown as unavailable because the selected run is already the one-worker baseline.</p>}
    </>}
  </div>;
}
