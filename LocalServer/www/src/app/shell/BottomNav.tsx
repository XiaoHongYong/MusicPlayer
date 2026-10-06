import { NavLink } from 'react-router-dom';
import { Home, Library, ListMusic, Settings } from 'lucide-react';
import { cn } from '@/lib/utils';

const items = [
  { to: '/', label: 'Home', icon: Home, end: true },
  { to: '/library/songs', label: 'Songs', icon: Library, end: false },
  { to: '/playlists', label: 'Playlists', icon: ListMusic, end: false },
  { to: '/settings', label: 'Settings', icon: Settings, end: false },
];

export function BottomNav() {
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
          {item.label}
        </NavLink>
      ))}
    </nav>
  );
}
