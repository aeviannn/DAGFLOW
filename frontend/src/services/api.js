async function request(path, options) {
  const response = await fetch(path, {
    ...options,
    headers: { 'Content-Type': 'application/json', ...options?.headers },
  });
  const payload = await response.json().catch(() => null);
  if (!response.ok) {
    throw new Error(payload?.error?.message ?? `Request failed (${response.status})`);
  }
  return payload;
}

export const api = {
  health: () => request('/api/health'),
  workflows: () => request('/api/workflows'),
  simulate: (body) => request('/api/simulate', { method: 'POST', body: JSON.stringify(body) }),
  compare: (body) => request('/api/compare', { method: 'POST', body: JSON.stringify(body) }),
};
