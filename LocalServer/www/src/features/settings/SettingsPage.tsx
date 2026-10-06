import { useQuery, useQueryClient } from '@tanstack/react-query';
import { api } from '@/api/client';
import { Button } from '@/components/ui/button';
import { useUiStore } from '@/stores/ui-store';

export function SettingsPage() {
  const mode = useUiStore((s) => s.themeMode);
  const setMode = useUiStore((s) => s.setThemeMode);
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
