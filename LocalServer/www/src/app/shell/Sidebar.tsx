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
import { useT, type LabelFn } from '@/i18n';

export const navItems: { to: string; label: LabelFn; icon: typeof Home }[] = [
  { to: '/', label: (t) => t('Home'), icon: Home },
  { to: '/library/songs', label: (t) => t('Songs'), icon: Library },
  { to: '/library/albums', label: (t) => t('Albums'), icon: Library },
  { to: '/library/artists', label: (t) => t('Artists'), icon: Library },
  { to: '/library/genres', label: (t) => t('Genres'), icon: Library },
  { to: '/playlists', label: (t) => t('Playlists'), icon: ListMusic },
  { to: '/history', label: (t) => t('History'), icon: History },
  { to: '/statistics', label: (t) => t('Statistics'), icon: BarChart3 },
  { to: '/settings', label: (t) => t('Settings'), icon: Settings },
];

export function Sidebar() {
  const t = useT();
  const collapsed = useUiStore((s) => s.sidebarCollapsed);
  const setCollapsed = useUiStore((s) => s.setSidebarCollapsed);
  return (
    <aside
      className={cn(
        'hidden flex-col border-r border-border bg-card py-4 md:flex',
        collapsed ? 'w-16' : 'w-56',
      )}
    >
      <button
        type="button"
        className={cn(
          'mb-6 flex items-center gap-2 rounded-md px-3 text-sm font-semibold tracking-tight hover:bg-accent',
          collapsed ? 'mx-2 justify-center px-0 py-1' : 'mx-2 px-2 py-1',
        )}
        title={collapsed ? t('Expand sidebar') : t('Collapse sidebar')}
        aria-label={collapsed ? t('Expand sidebar') : t('Collapse sidebar')}
        onClick={() => setCollapsed(!collapsed)}
      >
        <img src="/app-icon.png" alt="" width={28} height={28} className="h-7 w-7 shrink-0 rounded-md" />
        {!collapsed && <span>{t('Music Center')}</span>}
      </button>
      <nav className="flex flex-1 flex-col gap-0.5 px-2">
        {navItems.map((item) => (
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
            {!collapsed && item.label(t)}
          </NavLink>
        ))}
      </nav>
    </aside>
  );
}
