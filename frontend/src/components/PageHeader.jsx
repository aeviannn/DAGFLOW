import { ChevronRight } from 'lucide-react';

export default function PageHeader({ eyebrow, title, description, action }) {
  return <header className="page-header"><div>
    <div className="breadcrumbs"><span>DAGFlow</span><ChevronRight size={13} /><span>{eyebrow}</span></div>
    <h1>{title}</h1><p>{description}</p></div>
    {action && <div className="page-header-actions">{action}</div>}
  </header>;
}
