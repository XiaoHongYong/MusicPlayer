import { NavLink } from 'react-router-dom';
import { Home, Library, ListMusic, Settings } from 'lucide-react';
import { cn } from '@/lib/utils';
import { useT, type LabelFn } from '@/i18n';

const items: { to: string; label: LabelFn; icon: typeof Home; end: boolean }[] = [
  { to: '/', label: (t) => t('Home'), icon: Home, end: true },
  { to: '/library/songs', label: (t) => t('Songs'), icon: Library, end: false },
  { to: '/playlists', label: (t) => t('Playlists'), icon: ListMusic, end: false },
  { to: '/settings', label: (t) => t('Settings'), icon: Settings, end: false },
];

export function BottomNav() {
  const t = useT();
  return (
    <nav className="flex border-t border-border bg-card md:hidden">
      {items.map((item) => (
        <NavLink
          key={item.to}
          to={item.to}
          end={item.end}
          className={({ isActive }) =>
            cn(
              'flex flex-1 flex-col items-center gap-0.5 py-2 text-[10px] text-muted-foreground',
              isActive && 'text-primary',
            )
          }
        >
          <item.icon size={18} />
          {item.label(t)}
        </NavLink>
      ))}
    </nav>
  );
}
