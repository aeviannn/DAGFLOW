import { Activity, Boxes, GitBranch, Layers3, Workflow } from 'lucide-react';

const navItems = [
  { id: 'command', label: 'Command Center', icon: Activity },
  { id: 'comparison', label: 'Policy Comparison', icon: GitBranch },
  { id: 'workflows', label: 'Workflows', icon: Boxes },
];

export default function Sidebar({ page, onNavigate, engineReady }) {
  return <aside className="sidebar">
    <div className="brand-lockup"><div className="brand-mark"><Workflow size={20} strokeWidth={2.4} /></div>
      <div><span className="brand-name">DAGFlow</span><span className="brand-caption">WORKFLOW LAB</span></div></div>
    <div className="workspace-switcher"><div className="workspace-avatar">D</div>
      <div className="workspace-copy"><strong>Local workspace</strong><span>Scheduling engine</span></div><Layers3 size={15} className="muted-icon" /></div>
    <div className="nav-label">WORKSPACE</div>
    <nav className="sidebar-nav" aria-label="Primary navigation">{navItems.map(({ id, label, icon: Icon }) =>
      <button key={id} aria-label={label} title={label} className={`nav-item ${page === id ? 'active' : ''}`} onClick={() => onNavigate(id)}>
        <Icon size={17} strokeWidth={1.9} /><span>{label}</span>{id === 'command' && <span className="nav-live-dot" />}
      </button>)}</nav>
    <div className="sidebar-footer"><div className={`engine-status ${engineReady ? 'online' : 'offline'}`}><span className="status-dot" />
      <div><strong>{engineReady ? 'Engine connected' : 'Engine unavailable'}</strong><span>{engineReady ? 'C++17 · local process' : 'Check local API'}</span></div></div>
      <div className="sidebar-version"><span>DAGFlow</span><span>v1.0.0</span></div></div>
  </aside>;
}
