import { ChevronDown, Minus, Play, Plus, RotateCw } from 'lucide-react';

const policies = [{ id: 'fcfs', label: 'FCFS' }, { id: 'sjf', label: 'SJF' }, { id: 'cpf', label: 'Critical Path First' }];

export default function SimulationControls({ workflows, workflow, onWorkflow, workers, onWorkers,
                                            policy, onPolicy, onRun, busy }) {
  return <section className="controls-panel surface">
    <div className="control-field workflow-field"><label htmlFor="workflow-select">Workflow</label>
      <div className="select-wrap"><select id="workflow-select" value={workflow} onChange={(event) => onWorkflow(event.target.value)}>
        {workflows.map((item) => <option key={item.id} value={item.id}>{item.name} · {item.task_count} tasks</option>)}
      </select><ChevronDown size={15} /></div></div>
    <div className="control-field policy-field"><label htmlFor="policy-select">Scheduling policy</label>
      <div className="select-wrap"><select id="policy-select" value={policy} onChange={(event) => onPolicy(event.target.value)}>
        {policies.map((item) => <option key={item.id} value={item.id}>{item.label}</option>)}
      </select><ChevronDown size={15} /></div></div>
    <div className="control-field workers-field"><label>Workers</label>
      <div className="stepper" aria-label="Worker count"><button aria-label="Remove worker" disabled={workers <= 1} onClick={() => onWorkers(Math.max(1, workers - 1))}><Minus size={14} /></button>
        <span aria-live="polite">{workers}</span><button aria-label="Add worker" disabled={workers >= 32} onClick={() => onWorkers(Math.min(32, workers + 1))}><Plus size={14} /></button></div></div>
    <button className="run-button" onClick={onRun} disabled={busy || workflows.length === 0}>
      {busy ? <RotateCw className="spin" size={16} /> : <Play size={15} fill="currentColor" />}{busy ? 'Running simulation' : 'Run simulation'}
    </button>
  </section>;
}
