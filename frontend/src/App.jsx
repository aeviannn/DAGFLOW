import { useState } from 'react';
import { ArrowUpRight, LoaderCircle, Workflow } from 'lucide-react';
import Sidebar from './components/Sidebar.jsx';
import CommandCenter from './pages/CommandCenter.jsx';
import PolicyComparison from './pages/PolicyComparison.jsx';
import WorkflowLibrary from './pages/WorkflowLibrary.jsx';
import { useSimulation } from './hooks/useSimulation.js';

export default function App() {
  const [page, setPage] = useState('command');
  const [workflow, setWorkflow] = useState('software-build-pipeline');
  const [workers, setWorkers] = useState(4);
  const [policy, setPolicy] = useState('cpf');
  const state = useSimulation();
  const updateWorkflow = (value) => { state.invalidate(); setWorkflow(value); };
  const updateWorkers = (value) => { state.invalidate(); setWorkers(value); };
  const updatePolicy = (value) => { state.invalidate(); setPolicy(value); };
  const onRun = async () => {
    try { await state.run({ workflow, workers, policy }); } catch { /* The shared error state is rendered below. */ }
  };
  const onCompare = async () => {
    try { await state.compare({ workflow, workers }); } catch { /* The shared error state is rendered below. */ }
  };

  if (state.status === 'loading') return <div className="boot-screen"><div className="boot-mark"><Workflow size={21} /></div><LoaderCircle className="spin" size={19} /><span>Connecting to the local engine…</span></div>;
  return <div className="app-shell">
    <Sidebar page={page} onNavigate={setPage} engineReady={state.status === 'ready'} />
    <main className="main-area">
      <div className="topbar"><div className="topbar-label"><span className="topbar-mark"><Workflow size={14} /></span>Workflow orchestration <span className="topbar-divider">/</span>
        <span className="topbar-current">{page === 'command' ? 'Command Center' : page === 'comparison' ? 'Policy Comparison' : 'Workflows'}</span></div>
        <div className="topbar-right"><span className="local-only"><span />Running locally</span><span className="topbar-separator" /><span className="user-chip" aria-label="Local workspace">N</span></div></div>
      <div className="content-container">
        {state.status === 'error' && <div className="connection-error"><div className="connection-symbol">!</div><div><strong>Can’t reach the DAGFlow engine</strong><p>{state.error}. Start the local Express bridge and refresh this page.</p></div>
          <button onClick={() => window.location.reload()}>Retry <ArrowUpRight size={14} /></button></div>}
        {page === 'command' && <CommandCenter workflows={state.workflows} workflow={workflow} setWorkflow={updateWorkflow} workers={workers} setWorkers={updateWorkers} policy={policy} setPolicy={updatePolicy}
          simulation={state.simulation} comparison={state.comparison} busy={state.busy} error={state.status === 'ready' ? state.error : ''} onRun={onRun} onClearError={() => state.setError('')} />}
        {page === 'comparison' && <PolicyComparison workflows={state.workflows} workflow={workflow} setWorkflow={updateWorkflow} workers={workers} setWorkers={updateWorkers}
          comparison={state.comparison} busy={state.busy} error={state.error} onCompare={onCompare} onClearError={() => state.setError('')} />}
        {page === 'workflows' && <WorkflowLibrary workflows={state.workflows} onOpen={(id) => { updateWorkflow(id); setPage('command'); }} />}
        <footer className="app-footer"><span>Built around deterministic scheduling.</span><span>Powered by the C++17 DAGFlow engine</span></footer>
      </div>
    </main>
  </div>;
}
