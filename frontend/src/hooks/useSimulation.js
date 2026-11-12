import { useCallback, useEffect, useRef, useState } from 'react';
import { api } from '../services/api.js';

export function useSimulation() {
  const [workflows, setWorkflows] = useState([]);
  const [status, setStatus] = useState('loading');
  const [error, setError] = useState('');
  const [simulation, setSimulation] = useState(null);
  const [comparison, setComparison] = useState(null);
  const [busy, setBusy] = useState(false);
  const requestSequence = useRef(0);

  useEffect(() => {
    let active = true;
    Promise.all([api.health(), api.workflows()])
      .then(([, payload]) => {
        if (!active) return;
        setWorkflows(payload.workflows);
        setStatus('ready');
      })
      .catch((reason) => {
        if (!active) return;
        setStatus('error');
        setError(reason.message);
      });
    return () => { active = false; };
  }, []);

  const invalidate = useCallback(() => {
    requestSequence.current += 1;
    setSimulation(null);
    setComparison(null);
    setError('');
    setBusy(false);
  }, []);

  const run = useCallback(async (settings) => {
    const requestId = ++requestSequence.current;
    setBusy(true);
    setError('');
    setSimulation(null);
    setComparison(null);
    try {
      const result = await api.simulate(settings);
      if (requestId !== requestSequence.current) return null;
      setSimulation(result);
      return result;
    } catch (reason) {
      if (requestId === requestSequence.current) setError(reason.message);
      throw reason;
    } finally {
      if (requestId === requestSequence.current) setBusy(false);
    }
  }, []);

  const compare = useCallback(async ({ workflow, workers }) => {
    const requestId = ++requestSequence.current;
    setBusy(true);
    setError('');
    setSimulation(null);
    setComparison(null);
    try {
      const result = await api.compare({ workflow, workers });
      if (requestId !== requestSequence.current) return null;
      setComparison(result);
      return result;
    } catch (reason) {
      if (requestId === requestSequence.current) setError(reason.message);
      throw reason;
    } finally {
      if (requestId === requestSequence.current) setBusy(false);
    }
  }, []);

  return { workflows, status, error, setError, simulation, comparison, busy, run, compare, invalidate };
}
