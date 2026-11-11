import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { after, before, test } from 'node:test';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { createApp, validateComparisonBody, validateSimulationBody } from './server.js';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const server = createServer(createApp({ enginePath: path.join(root, 'build', 'dagflow.exe'), serveFrontend: false }));
let baseUrl;

before(async () => {
  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
  baseUrl = `http://127.0.0.1:${server.address().port}`;
});
after(async () => new Promise((resolve) => server.close(resolve)));

test('request validators accept exact schemas and reject invalid fields', () => {
  assert.doesNotThrow(() => validateSimulationBody({ workflow: 'chain', workers: 3, policy: 'cpf' }));
  assert.doesNotThrow(() => validateComparisonBody({ workflow: 'empty', workers: 1 }));
  assert.throws(() => validateSimulationBody({ workflow: 'chain', workers: 0, policy: 'fcfs' }));
  assert.throws(() => validateSimulationBody({ workflow: 'chain', workers: 1, policy: 'random' }));
  assert.throws(() => validateComparisonBody({ workflow: 'chain', workers: 1, extra: true }));
});

test('health and workflow routes use the C++ engine', async () => {
  const health = await fetch(`${baseUrl}/api/health`).then((response) => response.json());
  const catalog = await fetch(`${baseUrl}/api/workflows`).then((response) => response.json());
  assert.equal(health.engine, 'available');
  assert.equal(catalog.workflows.length, 5);
  assert.equal(catalog.workflows.find((workflow) => workflow.id === 'chain').task_count, 3);
});

test('simulation and comparison routes forward actual engine results', async () => {
  const simulationResponse = await fetch(`${baseUrl}/api/simulate`, {
    method: 'POST', headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ workflow: 'fork-join', workers: 3, policy: 'cpf' }),
  });
  const simulation = await simulationResponse.json();
  assert.equal(simulation.metrics.makespan, 7);
  assert.deepEqual(simulation.analysis.critical_path_task_ids, [2, 3]);

  const comparisonResponse = await fetch(`${baseUrl}/api/compare`, {
    method: 'POST', headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ workflow: 'chain', workers: 2 }),
  });
  const comparison = await comparisonResponse.json();
  assert.deepEqual(comparison.results.map((result) => result.policy), ['fcfs', 'sjf', 'cpf']);
  assert.ok(comparison.results.every((result) => result.metrics.makespan === 6));
});

test('all workflow presets and scheduling policies return valid simulation results', async () => {
  const { workflows } = await fetch(`${baseUrl}/api/workflows`).then((response) => response.json());
  for (const workflow of workflows) {
    for (const policy of ['fcfs', 'sjf', 'cpf']) {
      const response = await fetch(`${baseUrl}/api/simulate`, {
        method: 'POST', headers: { 'content-type': 'application/json' },
        body: JSON.stringify({ workflow: workflow.id, workers: 2, policy }),
      });
      const result = await response.json();
      assert.equal(response.status, 200, `${workflow.id}/${policy} should succeed`);
      assert.equal(result.policy, policy);
      assert.equal(result.completed_task_count, workflow.task_count);
      assert.equal(result.executions.length, workflow.task_count);
      assert.ok(Number.isFinite(result.metrics.makespan));
    }
  }
  const emptyComparison = await fetch(`${baseUrl}/api/compare`, {
    method: 'POST', headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ workflow: 'empty', workers: 4 }),
  }).then((response) => response.json());
  assert.ok(emptyComparison.results.every((result) => result.metrics.speedup === null));
});

test('invalid workflow, policy, and worker count return structured client errors', async () => {
  const response = await fetch(`${baseUrl}/api/simulate`, {
    method: 'POST', headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ workflow: 'no-such-workflow', workers: 2, policy: 'sjf' }),
  });
  assert.equal(response.status, 400);
  assert.equal((await response.json()).error.code, 'UNKNOWN_WORKFLOW');

  for (const body of [
    { workflow: 'chain', workers: 2, policy: 'other' },
    { workflow: 'chain', workers: 0, policy: 'fcfs' },
    { workflow: 'chain', workers: 2, policy: 'fcfs', injected: 'ignored' },
  ]) {
    const invalid = await fetch(`${baseUrl}/api/simulate`, {
      method: 'POST', headers: { 'content-type': 'application/json' }, body: JSON.stringify(body),
    });
    assert.equal(invalid.status, 400);
    assert.ok((await invalid.json()).error.code);
  }
});
