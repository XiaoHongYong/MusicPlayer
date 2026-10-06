import { useQuery, useQueryClient } from '@tanstack/react-query';
import { api } from '@/api/client';
import { Button } from '@/components/ui/button';
import { useUiStore, type ThemeAccent } from '@/stores/ui-store';
import { cn } from '@/lib/utils';

const ACCENTS: { id: ThemeAccent; label: string; swatch: string }[] = [
  { id: 'violet', label: '紫罗兰', swatch: 'bg-violet-500' },
  { id: 'blue', label: '蓝', swatch: 'bg-blue-500' },
  { id: 'green', label: '绿', swatch: 'bg-emerald-500' },
  { id: 'orange', label: '橙', swatch: 'bg-orange-500' },
  { id: 'rose', label: '玫红', swatch: 'bg-rose-500' },
];

export function SettingsPage() {
  const mode = useUiStore((s) => s.themeMode);
  const setMode = useUiStore((s) => s.setThemeMode);
  const accent = useUiStore((s) => s.themeAccent);
  const setAccent = useUiStore((s) => s.setThemeAccent);
  const qc = useQueryClient();
  const { data } = useQuery({ queryKey: ['scan-status'], queryFn: api.scanStatus });

  return (
    <div className="space-y-8 p-6">
      <h1 className="text-2xl font-semibold">Settings</h1>
      <section>
        <h2 className="mb-3 text-sm font-medium">Appearance</h2>
        <div className="flex gap-2">
          {(['light', 'dark', 'system'] as const).map((m) => (
            <Button key={m} variant={mode === m ? 'default' : 'outline'} onClick={() => setMode(m)}>
              {m}
            </Button>
          ))}
        </div>
        <p className="mb-2 mt-4 text-sm text-muted-foreground">主题色</p>
        <div className="flex flex-wrap gap-2">
          {ACCENTS.map((c) => (
            <button
              key={c.id}
              type="button"
              onClick={() => setAccent(c.id)}
              className={cn(
                'inline-flex items-center gap-2 rounded-md border px-3 py-1.5 text-sm',
                accent === c.id ? 'border-primary' : 'border-border',
              )}
            >
              <span className={cn('h-3 w-3 rounded-full', c.swatch)} />
              {c.label}
            </button>
          ))}
        </div>
      </section>
      <section>
        <h2 className="mb-3 text-sm font-medium">Library</h2>
        <p className="mb-3 text-sm text-muted-foreground">
          扫描状态：{data?.state ?? 'idle'}
          {data?.state === 'running' ? `（${data.scanned}/${data.total}）` : ''}
        </p>
        <Button
          onClick={() => {
            void api.startScan().then((s) => qc.setQueryData(['scan-status'], s));
          }}
        >
          Scan now
        </Button>
      </section>
    </div>
  );
}
