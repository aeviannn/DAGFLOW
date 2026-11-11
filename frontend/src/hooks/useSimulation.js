import { useCallback, useEffect, useState } from 'react';
import { api } from '../services/api.js';

export function useSimulation() {
  const [workflows, setWorkflows] = useState([]);
  const [status, setStatus] = useState('loading');
  const [error, setError] = useState('');
  const [simulation, setSimulation] = useState(null);
  const [comparison, setComparison] = useState(null);
  const [busy, setBusy] = useState(false);

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

  const run = useCallback(async (settings) => {
    setBusy(true);
    setError('');
    setComparison(null);
    try {
      const result = await api.simulate(settings);
      setSimulation(result);
      return result;
    } catch (reason) {
      setError(reason.message);
      throw reason;
    } finally {
      setBusy(false);
    }
  }, []);

  const compare = useCallback(async ({ workflow, workers }) => {
    setBusy(true);
    setError('');
    setSimulation(null);
    try {
      const result = await api.compare({ workflow, workers });
      setComparison(result);
      return result;
    } catch (reason) {
      setError(reason.message);
      throw reason;
    } finally {
      setBusy(false);
    }
  }, []);

  return { workflows, status, error, setError, simulation, comparison, busy, run, compare };
}
