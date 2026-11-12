import { Activity, GitBranch, Lightbulb, Target } from 'lucide-react';
import { getExecutionInsights } from '../services/executionInsights.js';

const timeValue = (value) => Number.isFinite(value) ? `${value} time units` : '—';
const policyNames = { fcfs: 'FCFS', sjf: 'SJF', cpf: 'Critical Path First' };

function dependencyLabel(dependencies) {
  if (!Array.isArray(dependencies) || dependencies.length === 0) return 'None (source task)';
  return dependencies.join(', ');
}

export default function ExecutionInsights({ simulation, comparison }) {
  const insights = getExecutionInsights(simulation, comparison);
  if (!insights) return null;

  const sourceDetails = (insights.analysisTasks ?? []).filter((task) => insights.initialReadyTaskIds.includes(task.id));
  const sourceSummary = sourceDetails.map((task) => `${task.id} (${task.duration} units)`).join(', ');
  const policyName = policyNames[insights.policy.id] ?? insights.policy.name;

  return (
    <section className="surface execution-insights" aria-labelledby="execution-insights-title">
      <div className="panel-heading execution-insights-heading">
        <div className="panel-title-wrap"><span className="panel-icon violet-icon"><Lightbulb size={16} /></span>
          <div><h2 id="execution-insights-title">Execution Insights</h2>
            <p>What the C++ analysis and returned schedule tell us about this run.</p></div></div>
        <span className="insight-evidence-tag">ENGINE DATA</span>
      </div>

      <div className="execution-insights-grid">
        <section className="execution-insight-block critical-path-insight" aria-labelledby="critical-path-title">
          <div className="execution-insight-title"><Target size={15} /><h3 id="critical-path-title">Critical path</h3></div>
          {insights.taskCount === 0 ? <p className="execution-insight-copy">This empty workflow has no critical path or task dependencies.</p> : <>
            <div className="critical-path-value">{timeValue(insights.criticalPathLength)}</div>
            <div className="critical-task-ids"><span>Tasks on a global critical path</span>
              <div>{insights.criticalPathTaskIds.length > 0
                ? insights.criticalPathTaskIds.map((taskId) => <span className="critical-task-chip" key={taskId}>{taskId}</span>)
                : <span className="insight-muted">No task IDs were returned.</span>}</div>
            </div>
            <p className="execution-insight-copy">The C++ graph analysis identifies these tasks as belonging to at least one longest dependency path. Their durations and precedence constraints set the minimum time along that path; extra workers cannot skip those dependencies or shorten task durations.</p>
            {insights.criticalTasks.length > 0 && <div className="critical-task-table-wrap">
              <table className="critical-task-table"><thead><tr><th>Task</th><th>Duration</th><th>Dependencies</th><th>Bottom level</th></tr></thead>
                <tbody>{insights.criticalTasks.map((task) => <tr key={task.id}>
                  <td>{task.id}</td><td>{timeValue(task.duration)}</td><td>{dependencyLabel(task.dependencies)}</td><td>{timeValue(task.bottom_level)}</td>
                </tr>)}</tbody></table>
            </div>}
          </>}
        </section>

        <div className="execution-insight-side">
          <section className="execution-insight-block" aria-labelledby="parallelism-title">
            <div className="execution-insight-title"><Activity size={15} /><h3 id="parallelism-title">Parallelism</h3></div>
            <dl className="execution-facts">
              <div><dt>Configured workers</dt><dd>{insights.configuredWorkers || '—'}</dd></div>
              <div><dt>Workers used</dt><dd>{insights.workersUsed}{insights.configuredWorkers ? ` / ${insights.configuredWorkers}` : ''}</dd></div>
              <div><dt>Peak concurrent tasks</dt><dd>{insights.peakConcurrency}</dd></div>
              <div><dt>Total busy time</dt><dd>{timeValue(insights.totalBusyTime)}</dd></div>
              <div><dt>Makespan</dt><dd>{timeValue(insights.makespan)}</dd></div>
              <div><dt>Worker utilization</dt><dd>{insights.workerUtilization === null ? '—' : `${(insights.workerUtilization * 100).toFixed(1)}%`}</dd></div>
            </dl>
          </section>

          <section className="execution-insight-block policy-insight" aria-labelledby="policy-explanation-title">
            <div className="execution-insight-title"><GitBranch size={15} /><h3 id="policy-explanation-title">{policyName} policy</h3></div>
            <p className="execution-insight-copy">{insights.policy.principle}</p>
            {insights.firstStart ? <p className="dispatch-observation">
              {insights.initialReadyTaskIds.length > 0 && <>At time zero, source tasks {sourceSummary || insights.initialReadyTaskIds.join(', ')} were ready. </>}
              The returned records show Task {insights.firstStart.taskId} on Worker {insights.firstStart.workerId} starting at {insights.firstStart.startTime}{insights.firstStart.startTime === 0 ? ' (first among simultaneous starts by worker ID, then task ID)' : ''}, for {insights.firstStart.duration ?? 'an unreported'} time units. This is an observed execution record, separate from the policy rule.
            </p> : <p className="dispatch-observation">No task execution was recorded, so there is no dispatch example for this run.</p>}
          </section>
        </div>
      </div>

      <div className="execution-observations" aria-label="Performance observations">
        <div className="execution-observations-heading"><Lightbulb size={14} /><span>Measured observations</span></div>
        <ul>{insights.observations.map((observation, index) => <li key={`${index}-${observation}`}>{observation}</li>)}</ul>
      </div>
    </section>
  );
}
