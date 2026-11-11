import { ArrowRight, GitBranch, Layers3 } from 'lucide-react';
import PageHeader from '../components/PageHeader.jsx';

const descriptions = {
  empty: 'A zero-task workflow for exploring safe edge-case behavior.',
  chain: 'A simple sequential pipeline with three dependent tasks.',
  'fork-join': 'Two parallel prerequisites converge on a shared final task.',
  simultaneous: 'Concurrent completions release a zero-duration join task.',
  'software-build-pipeline': 'A ten-task build with parallel compilation, linking, tests, and packaging.',
};

export default function WorkflowLibrary({ workflows, onOpen }) {
  return <div className="page-stack">
    <PageHeader eyebrow="Workspace" title="Workflow library" description="Choose an in-memory example maintained by the C++ engine." />
    <div className="library-intro surface"><div className="library-icon"><GitBranch size={19} /></div><div><strong>Built-in workflow examples</strong>
      <p>Definitions and dependencies are owned by DAGFlow. Select one to open it in the Command Center.</p></div><span className="library-count">{workflows.length} PRESETS</span></div>
    <div className="workflow-grid">{workflows.map((workflow, index) => <button className="workflow-card surface" key={workflow.id} onClick={() => onOpen(workflow.id)}>
      <div className="workflow-card-top"><span className={`workflow-number workflow-number-${index}`}>0{index + 1}</span><span className="workflow-open"><ArrowRight size={15} /></span></div>
      <h2>{workflow.name}</h2><p>{descriptions[workflow.id] ?? 'A validated workflow preset.'}</p>
      <div className="workflow-card-meta"><span><Layers3 size={14} />{workflow.task_count} tasks</span><span className="workflow-select-label">Open in command center</span></div>
    </button>)}</div>
  </div>;
}
