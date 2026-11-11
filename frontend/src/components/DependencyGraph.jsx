import { Background, Controls, Handle, MarkerType, Position, ReactFlow } from '@xyflow/react';
import { useMemo } from 'react';
import { GitBranch } from 'lucide-react';

function TaskNode({ data }) {
  return <div className={`task-node ${data.critical ? 'critical' : ''}`} title={`Task ${data.id} · ${data.duration} time units${data.critical ? ' · Critical path' : ''}`}>
    <Handle type="target" position={Position.Left} />
    <div className="task-node-top"><span className="task-node-id">TASK {data.id}</span>{data.critical && <span className="critical-pill">CRITICAL</span>}</div>
    <div className="task-node-duration">{data.duration}<span> time units</span></div>
    <Handle type="source" position={Position.Right} />
  </div>;
}

const nodeTypes = { task: TaskNode };

export default function DependencyGraph({ analysis, selectedWorkflow }) {
  const { nodes, edges, height } = useMemo(() => {
    const tasks = analysis?.tasks ?? [];
    const critical = new Set(analysis?.critical_path_task_ids ?? []);
    const depth = new Map();
    for (const id of analysis?.topological_order ?? []) {
      const task = tasks.find((entry) => entry.id === id);
      depth.set(id, Math.max(-1, ...(task?.dependencies ?? []).map((parent) => depth.get(parent) ?? -1)) + 1);
    }
    const columns = new Map();
    for (const task of tasks) {
      const level = depth.get(task.id) ?? 0;
      columns.set(level, [...(columns.get(level) ?? []), task]);
    }
    const graphNodes = [];
    let tallest = 0;
    for (const [level, column] of columns) {
      tallest = Math.max(tallest, column.length);
      column.forEach((task, row) => graphNodes.push({
        id: String(task.id), type: 'task', position: { x: 42 + level * 256, y: 26 + row * 112 },
        data: { ...task, critical: critical.has(task.id) },
      }));
    }
    const graphEdges = tasks.flatMap((task) => (task.dependencies ?? []).map((dependency) => {
      const criticalEdge = critical.has(task.id) && critical.has(dependency);
      return { id: `${dependency}-${task.id}`, source: String(dependency), target: String(task.id),
        type: 'smoothstep', markerEnd: { type: MarkerType.ArrowClosed, color: criticalEdge ? '#8064dc' : '#c8c5d3' },
        style: { stroke: criticalEdge ? '#8064dc' : '#c8c5d3', strokeWidth: criticalEdge ? 2.2 : 1.5 } };
    }));
    return { nodes: graphNodes, edges: graphEdges, height: Math.max(280, tallest * 112 + 54) };
  }, [analysis]);

  return <section className="surface graph-panel">
    <div className="panel-heading"><div className="panel-title-wrap"><span className="panel-icon violet-icon"><GitBranch size={16} /></span>
      <div><h2>Dependency graph</h2><p>{analysis?.tasks?.length ?? 0} tasks · directed acyclic graph</p></div></div>
      <div className="graph-legend"><span><i className="legend-dot critical-dot" />Critical path</span><span><i className="legend-line" />Dependency</span></div></div>
    <div className="graph-canvas" style={{ height }}>
      {nodes.length ? <ReactFlow nodes={nodes} edges={edges} nodeTypes={nodeTypes} fitView fitViewOptions={{ padding: 0.12 }}
        onInit={(instance) => requestAnimationFrame(() => instance.fitView({ padding: 0.12 }))} minZoom={0.6} maxZoom={1.4}
        nodesDraggable nodesConnectable={false} proOptions={{ hideAttribution: true }}>
        <Background color="#e8e6ee" gap={22} size={1} /><Controls showInteractive={false} />
      </ReactFlow> : <div className="empty-visual"><GitBranch size={26} /><strong>{selectedWorkflow?.task_count === 0 ? 'This workflow is empty' : 'Run a simulation to view the graph'}</strong>
        <span>Task dependencies and critical paths will appear here.</span></div>}
    </div>
    <div className="graph-footer"><span><b>{analysis?.critical_path_length ?? 0}</b> time units on the global critical path</span>
      {analysis?.critical_path_task_ids?.length > 0 && <span className="critical-summary">{analysis.critical_path_task_ids.length} critical task{analysis.critical_path_task_ids.length === 1 ? '' : 's'}</span>}</div>
  </section>;
}
