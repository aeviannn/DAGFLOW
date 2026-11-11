import express from 'express';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { EngineError, runEngine } from './engine.js';

const backendDir = path.dirname(fileURLToPath(import.meta.url));
const projectRoot = path.resolve(backendDir, '..');
const defaultEngine = path.join(projectRoot, 'build', 'dagflow.exe');
const policies = new Set(['fcfs', 'sjf', 'cpf']);
const bodyLimit = '16kb';

export function validateSimulationBody(body) {
  if (!isPlainObject(body) || !hasExactKeys(body, ['workflow', 'workers', 'policy'])) {
    throw new EngineError('Expected workflow, workers, and policy fields.', 'INVALID_REQUEST', 400);
  }
  validateWorkflow(body.workflow);
  validateWorkers(body.workers);
  if (typeof body.policy !== 'string' || !policies.has(body.policy)) {
    throw new EngineError('Policy must be fcfs, sjf, or cpf.', 'INVALID_POLICY', 400);
  }
}

export function validateComparisonBody(body) {
  if (!isPlainObject(body) || !hasExactKeys(body, ['workflow', 'workers'])) {
    throw new EngineError('Expected workflow and workers fields.', 'INVALID_REQUEST', 400);
  }
  validateWorkflow(body.workflow);
  validateWorkers(body.workers);
}

function validateWorkflow(workflow) {
  if (typeof workflow !== 'string' || !/^[a-z0-9-]{1,64}$/.test(workflow)) {
    throw new EngineError('Workflow must be a valid workflow ID.', 'INVALID_WORKFLOW', 400);
  }
}

function validateWorkers(workers) {
  if (!Number.isSafeInteger(workers) || workers < 1 || workers > 256) {
    throw new EngineError('Worker count must be an integer from 1 to 256.', 'INVALID_WORKERS', 400);
  }
}

function isPlainObject(value) {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}

function hasExactKeys(value, keys) {
  const actual = Object.keys(value).sort();
  return actual.length === keys.length && actual.every((key, index) => key === [...keys].sort()[index]);
}

export function createApp({ enginePath = process.env.DAGFLOW_ENGINE_PATH || defaultEngine,
                            timeoutMs = 15000, serveFrontend = true } = {}) {
  const app = express();
  let catalogPromise;
  const engine = (args) => runEngine(enginePath, args, { timeoutMs, cwd: projectRoot });
  const catalog = () => {
    catalogPromise ??= engine(['--list-workflows', '--json']).catch((error) => {
      catalogPromise = null;
      throw error;
    });
    return catalogPromise;
  };
  const ensureWorkflow = async (id) => {
    const { workflows } = await catalog();
    if (!workflows.some((workflow) => workflow.id === id)) {
      throw new EngineError(`Unknown workflow: ${id}`, 'UNKNOWN_WORKFLOW', 400);
    }
  };

  app.disable('x-powered-by');
  app.use(express.json({ limit: bodyLimit, strict: true }));
  app.get('/api/health', asyncRoute(async (_request, response) => {
    await catalog();
    response.json({ status: 'ok', engine: 'available' });
  }));
  app.get('/api/workflows', asyncRoute(async (_request, response) => {
    response.json(await catalog());
  }));
  app.post('/api/simulate', asyncRoute(async (request, response) => {
    validateSimulationBody(request.body);
    await ensureWorkflow(request.body.workflow);
    const result = await engine([
      '--simulate', '--workflow', request.body.workflow,
      '--workers', String(request.body.workers), '--policy', request.body.policy, '--json',
    ]);
    response.json(result);
  }));
  app.post('/api/compare', asyncRoute(async (request, response) => {
    validateComparisonBody(request.body);
    await ensureWorkflow(request.body.workflow);
    const result = await engine([
      '--compare', '--workflow', request.body.workflow,
      '--workers', String(request.body.workers), '--json',
    ]);
    response.json(result);
  }));
  app.use('/api', (_request, response) => {
    response.status(404).json({ error: { code: 'NOT_FOUND', message: 'API route not found.' } });
  });

  if (serveFrontend) {
    const frontendDist = path.join(projectRoot, 'frontend', 'dist');
    app.use(express.static(frontendDist, { fallthrough: true, index: 'index.html' }));
    app.get('*path', (_request, response) => response.sendFile(path.join(frontendDist, 'index.html')));
  }

  app.use((error, _request, response, _next) => {
    const status = error.status ?? (error.type === 'entity.too.large' ? 413 : 400);
    const code = error.code ?? (error.type === 'entity.too.large' ? 'BODY_TOO_LARGE' : 'INVALID_JSON');
    response.status(status).json({ error: { code, message: error.message || 'Request failed.' } });
  });
  return app;
}

function asyncRoute(handler) {
  return (request, response, next) => Promise.resolve(handler(request, response)).catch(next);
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const app = createApp();
  app.listen(4174, '127.0.0.1', () => {
    console.log(`DAGFlow API listening at http://127.0.0.1:4174 (engine: ${process.env.DAGFLOW_ENGINE_PATH || defaultEngine})`);
  });
}
