import { useMemo } from 'react';
import { ArrowRight, Check, Command, Info, Sparkles } from 'lucide-react';
import PageHeader from '../components/PageHeader.jsx';
import SimulationControls from '../components/SimulationControls.jsx';
import DependencyGraph from '../components/DependencyGraph.jsx';
import GanttTimeline from '../components/GanttTimeline.jsx';
import ExecutionInsights from '../components/ExecutionInsights.jsx';
import { MetricCard } from '../components/MetricCard.jsx';

const policyNames = { fcfs: 'FCFS', sjf: 'SJF', cpf: 'Critical Path First' };

export default function CommandCenter({ workflows, workflow, setWorkflow, workers, setWorkers,
                                       policy, setPolicy, simulation, comparison, busy, error, onRun, onClearError }) {
  const selectedWorkflow = workflows.find((item) => item.id === workflow);
  const current = simulation?.workflow?.id === workflow &&
    simulation?.worker_count === workers && simulation?.policy === policy ? simulation : null;
  const resultLabel = useMemo(() => current && `${policyNames[current.policy]} · ${current.worker_count} workers`, [current]);

  return (
    <div className="page-stack">
      <PageHeader eyebrow="Workspace" title="Command Center"
        description="Plan, simulate, and understand how your workflow runs across workers."
        action={<span className="local-badge"><span className="status-dot" />LOCAL ENGINE</span>} />
      <SimulationControls workflows={workflows} workflow={workflow} onWorkflow={setWorkflow}
        workers={workers} onWorkers={setWorkers} policy={policy} onPolicy={setPolicy}
        onRun={onRun} busy={busy} />
      {error && <div className="alert alert-error" role="alert"><Info size={17} /><span>{error}</span><button onClick={onClearError}>Dismiss</button></div>}
      {current ? <>
        <div className="result-strip"><div className="result-ready"><span className="check-icon"><Check size={13} /></span>
          <span>Simulation complete</span><i />{resultLabel}</div><span className="result-task-count">{current.completed_task_count} / {current.workflow.task_count} tasks completed</span></div>
        <div className="metrics-grid">
          <MetricCard metric="makespan" value={current.metrics.makespan} />
          <MetricCard metric="total_busy_time" value={current.metrics.total_busy_time} />
          <MetricCard metric="worker_utilization" value={current.metrics.worker_utilization} />
          <MetricCard metric="throughput" value={current.metrics.throughput} />
        </div>
        <DependencyGraph analysis={current.analysis} selectedWorkflow={selectedWorkflow} />
        <GanttTimeline simulation={current} />
        <ExecutionInsights simulation={current} comparison={comparison} />
      </> : <>
        <div className="welcome-panel surface">
          <div className="welcome-art"><div className="art-node art-node-a">01</div><div className="art-node art-node-b">02</div><div className="art-node art-node-c">03</div><div className="art-link art-link-a" /><div className="art-link art-link-b" /><div className="art-spark"><Sparkles size={18} /></div></div>
          <div className="welcome-copy"><span className="eyebrow-chip"><Command size={12} /> SIMULATION WORKSPACE</span>
            <h2>Your workflow, in motion.</h2><p>Choose a workflow and scheduling policy to see how DAGFlow assigns dependency-ready tasks across your workers.</p>
            <button className="text-action" onClick={onRun} disabled={busy || workflows.length === 0}>{busy ? 'Running engine…' : 'Run your first simulation'}<ArrowRight size={15} /></button>
          </div>
        </div>
        <div className="starter-grid">
          <div className="starter-note"><span className="starter-index">01</span><div><strong>Explore the task graph</strong><p>Dependencies, bottom levels, and critical path are calculated by the C++ engine.</p></div></div>
          <div className="starter-note"><span className="starter-index">02</span><div><strong>Compare execution policies</strong><p>Replay the same workflow with FCFS, SJF, and Critical Path First.</p></div></div>
        </div>
      </>}
      {busy && <div className="loading-note" role="status"><span className="loading-pulse" />The C++ simulator is calculating a deterministic schedule. This is a completed-run view, not live task progress.</div>}
    </div>
  );
}
