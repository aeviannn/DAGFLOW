import assert from 'node:assert/strict';
import test from 'node:test';
import { getExecutionInsights, getPeakConcurrency } from './executionInsights.js';

function simulation(overrides = {}) {
  return {
    workflow: { id: 'example' },
    worker_count: 2,
    policy: 'sjf',
    analysis: {
      critical_path_length: 5,
      critical_path_task_ids: [1, 2],
      bottom_levels: [{ task_id: 1, value: 5 }, { task_id: 2, value: 3 }, { task_id: 3, value: 2 }],
      tasks: [
        { id: 1, duration: 2, dependencies: [] },
        { id: 2, duration: 3, dependencies: [1] },
        { id: 3, duration: 2, dependencies: [1] },
      ],
    },
    executions: [
      { task_id: 1, worker_id: 0, start_time: 0, completion_time: 2, duration: 2 },
      { task_id: 2, worker_id: 1, start_time: 2, completion_time: 5, duration: 3 },
      { task_id: 3, worker_id: 0, start_time: 2, completion_time: 4, duration: 2 },
    ],
    metrics: { makespan: 5, total_busy_time: 7, worker_utilization: 0.7, throughput: 0.6 },
    ...overrides,
  };
}

function comparison(overrides = {}) {
  const policyResults = ['fcfs', 'sjf', 'cpf'].map((policy) => ({
    policy,
    metrics: {
      makespan: 5,
      total_busy_time: 7,
      worker_utilization: 0.7,
      throughput: 0.6,
      speedup: policy === 'fcfs' ? 1.1 : 1.2,
    },
  }));
  return { workflow: { id: 'example' }, worker_count: 2, results: policyResults, ...overrides };
}

test('insights use returned critical-path, task, execution, and metrics data', () => {
  const result = getExecutionInsights(simulation());
  assert.equal(result.criticalPathLength, 5);
  assert.deepEqual(result.criticalPathTaskIds, [1, 2]);
  assert.deepEqual(result.criticalTasks.map(({ id, duration, dependencies, bottom_level }) => [id, duration, dependencies, bottom_level]), [
    [1, 2, [], 5], [2, 3, [1], 3],
  ]);
  assert.deepEqual(result.initialReadyTaskIds, [1]);
  assert.deepEqual(result.firstStart, { taskId: 1, workerId: 0, startTime: 0, completionTime: 2, duration: 2 });
  assert.deepEqual(result.workerIds, [0, 1]);
  assert.equal(result.workersUsed, 2);
  assert.equal(result.peakConcurrency, 2);
  assert.equal(result.makespan, 5);
  assert.equal(result.totalBusyTime, 7);
  assert.equal(result.workerUtilization, 0.7);
  assert.ok(result.observations.some((line) => line.includes('equals the C++ global critical-path length')));
});

test('matching policy comparison adds only measured ties and single-worker speedup claims', () => {
  const result = getExecutionInsights(simulation(), comparison());
  assert.equal(result.comparison.allPoliciesTieOnMakespan, true);
  assert.equal(result.comparison.identicalMetrics, true);
  assert.equal(result.comparison.byPolicy.sjf.metrics.speedup, 1.2);
  assert.ok(result.observations.some((line) => line.includes('tie at 5 time units')));
  assert.ok(result.observations.some((line) => line.includes('measured speedup 1.20×')));
});

test('mismatched or absent comparisons do not create cross-policy claims', () => {
  const result = getExecutionInsights(simulation(), comparison({ worker_count: 4 }));
  assert.equal(result.comparison, null);
  assert.ok(result.observations.some((line) => line.includes('No matching policy comparison is attached')));
  assert.equal(getExecutionInsights(simulation()).comparison, null);
});

test('incomplete comparison metrics do not produce an identical-results claim', () => {
  const partial = comparison();
  delete partial.results[1].metrics.throughput;
  const result = getExecutionInsights(simulation(), partial);
  assert.equal(result.comparison.identicalMetrics, false);
  assert.equal(result.comparison.allPoliciesTieOnMakespan, true);
});

test('empty workflow and absent optional values are safe', () => {
  const empty = getExecutionInsights({
    workflow: { id: 'empty' }, worker_count: 4, policy: 'fcfs',
    analysis: { critical_path_length: 0, critical_path_task_ids: [], tasks: [] },
    executions: [], metrics: { makespan: 0, total_busy_time: 0, worker_utilization: 0 },
  });
  assert.equal(empty.taskCount, 0);
  assert.equal(empty.peakConcurrency, 0);
  assert.equal(empty.workersUsed, 0);
  assert.equal(empty.throughput, undefined);
  assert.ok(empty.observations[0].includes('no tasks'));

  const partial = getExecutionInsights({});
  assert.equal(partial.criticalPathLength, null);
  assert.equal(partial.makespan, null);
  assert.equal(partial.workerUtilization, null);
  assert.equal(partial.firstStart, null);
  assert.equal(getExecutionInsights(null), null);
});

test('zero-duration executions use no interval and do not inflate concurrency', () => {
  assert.equal(getPeakConcurrency([
    { start_time: 0, completion_time: 0 },
    { start_time: 1, completion_time: 1 },
  ]), 0);
  const result = getExecutionInsights(simulation({
    worker_count: 4,
    analysis: { critical_path_length: 0, critical_path_task_ids: [9], bottom_levels: [{ task_id: 9, value: 0 }],
      tasks: [{ id: 9, duration: 0, dependencies: [] }] },
    executions: [{ task_id: 9, worker_id: 3, start_time: 0, completion_time: 0, duration: 0 }],
    metrics: { makespan: 0, total_busy_time: 0, worker_utilization: 0, throughput: 0 },
  }));
  assert.equal(result.peakConcurrency, 0);
  assert.deepEqual(result.workerIds, [3]);
  assert.equal(result.workersUsed, 1);
  assert.ok(result.observations.some((line) => line.includes('3 workers stayed unused')));
});

test('completion and start at the same time are not double-counted as concurrent', () => {
  assert.equal(getPeakConcurrency([
    { start_time: 0, completion_time: 2 },
    { start_time: 2, completion_time: 4 },
    { start_time: 1, completion_time: 2 },
  ]), 2);
});
