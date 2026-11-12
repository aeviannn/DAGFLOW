const POLICY_GUIDANCE = {
  fcfs: {
    name: 'FCFS',
    principle: 'Selects the ready task that entered the ready queue earliest. Simultaneously released tasks enter in deterministic task-ID order.',
  },
  sjf: {
    name: 'SJF',
    principle: 'Selects the ready task with the shortest duration, using ascending task ID to break ties.',
  },
  cpf: {
    name: 'Critical Path First',
    principle: 'Selects the ready task with the highest bottom level from C++ graph analysis, using ascending task ID to break ties.',
  },
};

const finiteNumber = (value) => Number.isFinite(value) ? value : null;

export function getPeakConcurrency(executions = []) {
  const events = [];
  for (const execution of executions) {
    const start = finiteNumber(execution?.start_time);
    const completion = finiteNumber(execution?.completion_time);
    if (start === null || completion === null || completion <= start) continue;
    events.push({ time: start, change: 1 }, { time: completion, change: -1 });
  }

  // Process finishes before starts at the same timestamp so back-to-back tasks
  // are not counted as concurrent. Zero-duration records occupy no interval.
  events.sort((left, right) => left.time - right.time || left.change - right.change);
  let running = 0;
  let peak = 0;
  for (const event of events) {
    running += event.change;
    peak = Math.max(peak, running);
  }
  return peak;
}

function matchingComparison(simulation, comparison) {
  if (!comparison || typeof comparison !== 'object' ||
      comparison?.workflow?.id !== simulation?.workflow?.id ||
      comparison?.worker_count !== simulation?.worker_count ||
      !Array.isArray(comparison.results)) return null;

  const policies = ['fcfs', 'sjf', 'cpf'];
  const results = policies.map((policy) => comparison.results.find((result) => result?.policy === policy));
  if (results.some((result) => !result?.metrics)) return null;

  const makespans = results.map((result) => finiteNumber(result.metrics.makespan));
  const utilizations = results.map((result) => finiteNumber(result.metrics.worker_utilization));
  const throughputs = results.map((result) => finiteNumber(result.metrics.throughput));
  const bestMakespan = makespans.every((value) => value !== null) ? Math.min(...makespans) : null;
  const highestUtilization = utilizations.every((value) => value !== null) ? Math.max(...utilizations) : null;
  const lowestUtilization = utilizations.every((value) => value !== null) ? Math.min(...utilizations) : null;
  const highestThroughput = throughputs.every((value) => value !== null) ? Math.max(...throughputs) : null;
  const byPolicy = Object.fromEntries(results.map((result) => [result.policy, result]));
  const comparedMetrics = ['makespan', 'total_busy_time', 'worker_utilization', 'throughput'];
  const hasCompleteMetrics = comparedMetrics.every((metric) =>
    results.every((result) => finiteNumber(result.metrics[metric]) !== null));
  const identicalMetrics = hasCompleteMetrics && comparedMetrics
    .every((metric) => results.every((result) => result.metrics[metric] === results[0].metrics[metric]));

  return {
    results,
    byPolicy,
    bestMakespan,
    bestMakespanPolicies: results.filter((result) => result.metrics.makespan === bestMakespan).map((result) => result.policy),
    allPoliciesTieOnMakespan: makespans.every((value) => value !== null) && new Set(makespans).size === 1,
    identicalMetrics,
    highestUtilization,
    lowestUtilization,
    highestUtilizationPolicies: results.filter((result) => result.metrics.worker_utilization === highestUtilization).map((result) => result.policy),
    highestThroughput,
    highestThroughputPolicies: results.filter((result) => result.metrics.throughput === highestThroughput).map((result) => result.policy),
  };
}

export function getExecutionInsights(simulation, comparison = null) {
  if (!simulation || typeof simulation !== 'object') return null;

  const analysis = simulation.analysis ?? {};
  const tasks = Array.isArray(analysis.tasks) ? analysis.tasks : [];
  const executions = Array.isArray(simulation.executions) ? simulation.executions : [];
  const metrics = simulation.metrics ?? {};
  const configuredWorkers = Number.isInteger(simulation.worker_count) && simulation.worker_count > 0
    ? simulation.worker_count : 0;
  const workerIds = [...new Set(executions
    .map((execution) => execution?.worker_id)
    .filter((workerId) => Number.isInteger(workerId) && workerId >= 0))].sort((a, b) => a - b);
  const criticalPathTaskIds = Array.isArray(analysis.critical_path_task_ids)
    ? [...new Set(analysis.critical_path_task_ids)] : [];
  const taskById = new Map(tasks.filter((task) => task && typeof task === 'object').map((task) => [task.id, task]));
  const bottomLevelById = new Map((Array.isArray(analysis.bottom_levels) ? analysis.bottom_levels : [])
    .map((entry) => [entry.task_id, entry.value]));
  const criticalTasks = criticalPathTaskIds
    .map((taskId) => taskById.get(taskId))
    .filter(Boolean)
    .map((task) => ({ ...task, bottom_level: finiteNumber(bottomLevelById.get(task.id)) }));
  const initialReadyTaskIds = tasks
    .filter((task) => task && Array.isArray(task.dependencies) && task.dependencies.length === 0)
    .map((task) => task.id)
    .sort((a, b) => a - b);
  const firstStart = executions
    .filter((execution) => execution?.start_time === 0)
    .slice()
    .sort((left, right) => left.worker_id - right.worker_id || left.task_id - right.task_id)[0] ?? null;
  const criticalPathLength = finiteNumber(analysis.critical_path_length);
  const makespan = finiteNumber(metrics.makespan);
  const comparisonSummary = matchingComparison(simulation, comparison);
  const policy = POLICY_GUIDANCE[simulation.policy] ?? {
    name: String(simulation.policy ?? 'Unknown policy'),
    principle: 'The scheduling policy was not recognized by this explanation view.',
  };

  const observations = [];
  if (tasks.length === 0) {
    observations.push('This workflow has no tasks, so it has no execution span or observed parallelism.');
  } else if (criticalPathLength !== null && makespan !== null) {
    if (makespan === criticalPathLength) {
      observations.push(`The ${makespan}-unit makespan equals the C++ global critical-path length. Under this scheduling model, that path is the lower bound; more workers cannot shorten its task durations or dependencies.`);
    } else if (makespan > criticalPathLength) {
      observations.push(`The observed makespan is ${makespan - criticalPathLength} time units above the C++ critical-path lower bound. The returned schedule has additional span beyond the longest dependency path.`);
    } else {
      observations.push('The returned makespan is below the reported critical-path length; inspect the engine response because these values are inconsistent.');
    }
  }

  const peakConcurrency = getPeakConcurrency(executions);
  if (tasks.length > 0 && configuredWorkers > 0 && peakConcurrency < configuredWorkers) {
    observations.push(`The schedule peaked at ${peakConcurrency} concurrent task${peakConcurrency === 1 ? '' : 's'} with ${configuredWorkers} workers configured; this run never occupied the full worker pool at once.`);
  }
  if (tasks.length > 0 && workerIds.length < configuredWorkers) {
    observations.push(`${workerIds.length} of ${configuredWorkers} configured workers received tasks; ${configuredWorkers - workerIds.length} worker${configuredWorkers - workerIds.length === 1 ? '' : 's'} stayed unused in this run.`);
  }

  if (comparisonSummary) {
    if (comparisonSummary.allPoliciesTieOnMakespan) {
      observations.push(`FCFS, SJF, and Critical Path First tie at ${comparisonSummary.bestMakespan} time units for this workflow and worker count${comparisonSummary.identicalMetrics ? ', with identical reported metrics' : ''}.`);
    } else if (comparisonSummary.bestMakespan !== null) {
      observations.push(`${comparisonSummary.bestMakespanPolicies.map((id) => POLICY_GUIDANCE[id].name).join(' and ')} produced the shortest makespan (${comparisonSummary.bestMakespan} time units) in the matching policy comparison.`);
    }
    const selectedResult = comparisonSummary.byPolicy[simulation.policy];
    const speedup = finiteNumber(selectedResult?.metrics?.speedup);
    if (configuredWorkers > 1 && speedup !== null) {
      if (speedup > 1) {
        observations.push(`${policy.name} improved makespan against its single-worker baseline at ${configuredWorkers} workers (measured speedup ${speedup.toFixed(2)}×).`);
      } else if (speedup === 1) {
        observations.push(`${policy.name} showed no makespan reduction against its single-worker baseline at ${configuredWorkers} workers.`);
      } else {
        observations.push(`${policy.name} was slower than its single-worker baseline at ${configuredWorkers} workers (measured speedup ${speedup.toFixed(2)}×).`);
      }
    }
    if (comparisonSummary.highestUtilization !== null) {
      observations.push(`At this worker count, ${comparisonSummary.highestUtilizationPolicies.map((id) => POLICY_GUIDANCE[id].name).join(' and ')} reported the highest utilization (${(comparisonSummary.highestUtilization * 100).toFixed(1)}%).`);
      if (comparisonSummary.lowestUtilization !== comparisonSummary.highestUtilization) {
        observations.push(`Across these policies at the same worker count, reported utilization ranged from ${(comparisonSummary.lowestUtilization * 100).toFixed(1)}% to ${(comparisonSummary.highestUtilization * 100).toFixed(1)}%.`);
      }
    }
    if (comparisonSummary.highestThroughput !== null) {
      observations.push(`${comparisonSummary.highestThroughputPolicies.map((id) => POLICY_GUIDANCE[id].name).join(' and ')} reported the highest throughput (${comparisonSummary.highestThroughput.toFixed(2)} tasks per time unit) in this comparison.`);
    }
  } else {
    observations.push('No matching policy comparison is attached, so cross-policy ties and single-worker speedup are not inferred for this run.');
  }

  return {
    workflowId: simulation.workflow?.id ?? null,
    taskCount: tasks.length,
    analysisTasks: tasks,
    criticalPathLength,
    criticalPathTaskIds,
    criticalTasks,
    criticalPathSlack: makespan !== null && criticalPathLength !== null ? makespan - criticalPathLength : null,
    configuredWorkers,
    workerIds,
    workersUsed: workerIds.length,
    peakConcurrency,
    makespan,
    totalBusyTime: finiteNumber(metrics.total_busy_time),
    workerUtilization: finiteNumber(metrics.worker_utilization),
    policy: { id: simulation.policy ?? null, ...policy },
    initialReadyTaskIds,
    firstStart: firstStart ? {
      taskId: firstStart.task_id,
      workerId: firstStart.worker_id,
      startTime: firstStart.start_time,
      completionTime: firstStart.completion_time,
      duration: finiteNumber(firstStart.duration),
    } : null,
    comparison: comparisonSummary,
    observations,
  };
}
