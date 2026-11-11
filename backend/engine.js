import { spawn } from 'node:child_process';

const MAX_OUTPUT_BYTES = 1024 * 1024;

export class EngineError extends Error {
  constructor(message, code = 'ENGINE_ERROR', status = 502) {
    super(message);
    this.name = 'EngineError';
    this.code = code;
    this.status = status;
  }
}

export function runEngine(executable, args, { timeoutMs = 15000, cwd = process.cwd() } = {}) {
  return new Promise((resolve, reject) => {
    const child = spawn(executable, args, {
      cwd,
      shell: false,
      windowsHide: true,
      stdio: ['ignore', 'pipe', 'pipe'],
    });
    let stdout = '';
    let stderr = '';
    let outputBytes = 0;
    let settled = false;
    const finish = (callback, value) => {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      callback(value);
    };
    const timer = setTimeout(() => {
      child.kill();
      finish(reject, new EngineError('The scheduling engine timed out.', 'ENGINE_TIMEOUT', 504));
    }, timeoutMs);

    child.stdout.on('data', (chunk) => {
      outputBytes += chunk.length;
      if (outputBytes > MAX_OUTPUT_BYTES) {
        child.kill();
        finish(reject, new EngineError('The engine response exceeded the output limit.', 'ENGINE_OUTPUT_LIMIT'));
        return;
      }
      stdout += chunk.toString('utf8');
    });
    child.stderr.on('data', (chunk) => {
      stderr += chunk.toString('utf8').slice(0, 8192 - stderr.length);
    });
    child.on('error', (error) => {
      finish(reject, new EngineError(`Could not start the scheduling engine: ${error.message}`, 'ENGINE_UNAVAILABLE', 503));
    });
    child.on('close', (code) => {
      if (settled) return;
      if (code !== 0) {
        finish(reject, new EngineError(stderr.trim() || `Engine exited with code ${code}.`));
        return;
      }
      try {
        finish(resolve, JSON.parse(stdout));
      } catch {
        finish(reject, new EngineError('The scheduling engine returned invalid JSON.'));
      }
    });
  });
}
