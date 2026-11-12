import { expect, test } from '@playwright/test';

async function setWorkerCount(page, target) {
  const count = page.locator('.stepper span[aria-live="polite"]');
  while (Number(await count.textContent()) < target) {
    await page.getByRole('button', { name: 'Add worker' }).click();
  }
  while (Number(await count.textContent()) > target) {
    await page.getByRole('button', { name: 'Remove worker' }).click();
  }
}

test.beforeEach(async ({ page }) => {
  await page.goto('/');
  await expect(page.getByRole('button', { name: 'Command Center' })).toBeVisible();
  await expect(page.getByLabel('Workflow', { exact: true })).toBeVisible();
});

test('Command Center runs every preset and only shows results for current settings', async ({ page }) => {
  const requests = [];
  page.on('request', (request) => {
    if (request.url().endsWith('/api/simulate') && request.method() === 'POST') {
      requests.push(request.postDataJSON());
    }
  });

  const runs = [
    { workflow: 'chain', workers: 1, policy: 'fcfs', makespan: '6' },
    { workflow: 'fork-join', workers: 2, policy: 'fcfs', makespan: '7' },
    { workflow: 'simultaneous', workers: 2, policy: 'sjf', makespan: '2' },
    { workflow: 'software-build-pipeline', workers: 2, policy: 'fcfs', makespan: '21' },
    { workflow: 'empty', workers: 4, policy: 'fcfs', makespan: '0' },
  ];

  for (const run of runs) {
    await page.getByLabel('Workflow', { exact: true }).selectOption(run.workflow);
    await page.getByLabel('Scheduling policy').selectOption(run.policy);
    await setWorkerCount(page, run.workers);
    await expect(page.getByText('Simulation complete')).toHaveCount(0);
    await page.getByRole('button', { name: 'Run simulation' }).click();
    await expect(page.getByText('Simulation complete')).toBeVisible();
    await expect(page.locator('.metric-card').filter({ hasText: 'Makespan' }).locator('.metric-value'))
      .toContainText(run.makespan);
    await expect(page.locator('.result-strip')).toContainText(`${run.workers} workers`);
    await expect(page.locator('.result-strip')).toContainText(run.workflow === 'empty' ? '0 / 0 tasks completed' : 'tasks completed');
  }

  expect(requests).toEqual(runs.map(({ workflow, workers, policy }) => ({ workflow, workers, policy })));

  await page.getByLabel('Scheduling policy').selectOption('sjf');
  await expect(page.getByText('Simulation complete')).toHaveCount(0);
  await expect(page.getByText('Your workflow, in motion.')).toBeVisible();
});

test('an older overlapping simulation cannot replace a newer result and loading is visible', async ({ page }) => {
  let firstRequestStarted;
  const started = new Promise((resolve) => { firstRequestStarted = resolve; });
  let releaseFirst;
  const release = new Promise((resolve) => { releaseFirst = resolve; });

  await page.route('**/api/simulate', async (route) => {
    const body = route.request().postDataJSON();
    if (body.workflow === 'chain') {
      const response = await route.fetch();
      firstRequestStarted();
      await release;
      await route.fulfill({ response });
      return;
    }
    await route.continue();
  });

  await page.getByLabel('Workflow', { exact: true }).selectOption('chain');
  await page.getByRole('button', { name: 'Run simulation' }).click();
  await started;
  await expect(page.getByRole('status')).toContainText('The C++ simulator is calculating');

  await page.getByLabel('Workflow', { exact: true }).selectOption('fork-join');
  await page.getByLabel('Scheduling policy').selectOption('cpf');
  await page.getByRole('button', { name: 'Run simulation' }).click();
  await expect(page.locator('.result-strip')).toContainText('Critical Path First · 4 workers');
  await expect(page.locator('.graph-footer')).toContainText('2 critical tasks');

  releaseFirst();
  await expect(page.locator('.graph-footer')).toContainText('2 critical tasks');
  await expect(page.locator('.result-strip')).toContainText('Critical Path First · 4 workers');
});

test('Policy Comparison shows three engine results and invalidates them when settings change', async ({ page }) => {
  await page.getByRole('button', { name: 'Policy Comparison' }).click();
  await page.getByLabel('Comparison workflow').selectOption('fork-join');
  await page.locator('.worker-pills').getByRole('button', { name: '2', exact: true }).click();
  await page.getByRole('button', { name: 'Compare policies' }).click();

  const cards = page.locator('.policy-card');
  await expect(cards).toHaveCount(3);
  await expect(cards.nth(0)).toContainText('First Come, First Served');
  await expect(cards.nth(1)).toContainText('Shortest Job First');
  await expect(cards.nth(2)).toContainText('Critical Path First');
  for (const card of await cards.all()) {
    await expect(card.locator('.policy-makespan strong')).toHaveText('7');
    await expect(card.locator('.policy-secondary')).toContainText('64.3% utilized');
    await expect(card.locator('.policy-secondary')).toContainText('0.43 tasks/time');
    await expect(card.locator('.policy-speedup')).toContainText('1.29×');
  }
  await expect(page.locator('.insight-panel')).toContainText('All three policies produce the same makespan');

  await page.getByLabel('Comparison workflow').selectOption('chain');
  await expect(cards).toHaveCount(0);
});

test('empty workflow and invalid worker input are handled safely', async ({ page }) => {
  await page.getByRole('button', { name: 'Policy Comparison' }).click();
  const workerInput = page.getByLabel('Custom worker count');
  await workerInput.fill('0');
  await expect(workerInput).toHaveValue('1');
  await page.getByLabel('Comparison workflow').selectOption('empty');
  await page.getByRole('button', { name: 'Compare policies' }).click();
  await expect(page.locator('.policy-card')).toHaveCount(3);
  await expect(page.locator('.policy-speedup').first()).toContainText('—');

  await page.getByRole('button', { name: 'Command Center' }).click();
  await page.getByLabel('Workflow', { exact: true }).selectOption('empty');
  await page.getByRole('button', { name: 'Run simulation' }).click();
  await expect(page.locator('.result-strip')).toContainText('0 / 0 tasks completed');
  await expect(page.locator('.timeline-panel')).toContainText('This workflow has no task executions.');
});

test('backend unavailable and engine errors render recoverable error states', async ({ page }) => {
  await page.route('**/api/health', (route) => route.abort());
  await page.route('**/api/workflows', (route) => route.abort());
  await page.goto('/');
  await expect(page.getByText('Can’t reach the DAGFlow engine')).toBeVisible();

  await page.unroute('**/api/health');
  await page.unroute('**/api/workflows');
  await page.reload();
  await expect(page.getByLabel('Workflow', { exact: true })).toBeVisible();
  await page.route('**/api/simulate', (route) => route.fulfill({
    status: 502,
    contentType: 'application/json',
    body: JSON.stringify({ error: { code: 'ENGINE_ERROR', message: 'Simulated engine failure.' } }),
  }));
  await page.getByRole('button', { name: 'Run simulation' }).click();
  await expect(page.getByRole('alert')).toContainText('Simulated engine failure.');
  await expect(page.getByText('Simulation complete')).toHaveCount(0);
});
