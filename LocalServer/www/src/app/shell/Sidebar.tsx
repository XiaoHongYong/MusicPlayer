import { NavLink } from 'react-router-dom';
import {
  Home,
  Library,
  ListMusic,
  History,
  BarChart3,
  Settings,
} from 'lucide-react';
import { cn } from '@/lib/utils';
import { useUiStore } from '@/stores/ui-store';

const items = [
  { to: '/', label: 'Home', icon: Home },
  { to: '/library/songs', label: 'Songs', icon: Library },
  { to: '/library/albums', label: 'Albums', icon: Library },
  { to: '/library/artists', label: 'Artists', icon: Library },
  { to: '/library/genres', label: 'Genres', icon: Library },
  { to: '/playlists', label: 'Playlists', icon: ListMusic },
  { to: '/history', label: 'History', icon: History },
  { to: '/statistics', label: 'Statistics', icon: BarChart3 },
  { to: '/settings', label: 'Settings', icon: Settings },
];

export function Sidebar() {
  const collapsed = useUiStore((s) => s.sidebarCollapsed);
  return (
    <aside className={cn('flex flex-col border-r border-border bg-card py-4', collapsed ? 'w-16' : 'w-56')}>
      <div className="mb-6 px-4 text-sm font-semibold tracking-tight">{collapsed ? 'MC' : 'Music Center'}</div>
      <nav className="flex flex-1 flex-col gap-0.5 px-2">
        {items.map((item) => (
          <NavLink
            key={item.to}
            to={item.to}
            end={item.to === '/'}
            className={({ isActive }) =>
              cn(
                'flex items-center gap-3 rounded-md px-3 py-2 text-sm text-muted-foreground hover:bg-accent hover:text-foreground',
                isActive && 'bg-accent text-foreground',
              )
            }
          >
            <item.icon size={16} />
            {!collapsed && item.label}
          </NavLink>
        ))}
      </nav>
    </aside>
  );
}
