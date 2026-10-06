import { useQuery, useQueryClient } from '@tanstack/react-query';
import { api } from '@/api/client';
import { Button } from '@/components/ui/button';
import { useUiStore, type ThemeAccent } from '@/stores/ui-store';
import { cn } from '@/lib/utils';
import { LOCALES, useI18nStore, useT, type LabelFn } from '@/i18n';

const ACCENTS: { id: ThemeAccent; label: LabelFn; swatch: string }[] = [
  { id: 'violet', label: (t) => t('Violet'), swatch: 'bg-violet-500' },
  { id: 'blue', label: (t) => t('Blue'), swatch: 'bg-blue-500' },
  { id: 'green', label: (t) => t('Green'), swatch: 'bg-emerald-500' },
  { id: 'orange', label: (t) => t('Orange'), swatch: 'bg-orange-500' },
  { id: 'rose', label: (t) => t('Rose'), swatch: 'bg-rose-500' },
];

const THEME_MODES: { id: 'light' | 'dark' | 'system'; label: LabelFn }[] = [
  { id: 'light', label: (t) => t('Light') },
  { id: 'dark', label: (t) => t('Dark') },
  { id: 'system', label: (t) => t('System') },
];

export function SettingsPage() {
  const t = useT();
  const locale = useI18nStore((s) => s.locale);
  const setLocale = useI18nStore((s) => s.setLocale);
  const mode = useUiStore((s) => s.themeMode);
  const setMode = useUiStore((s) => s.setThemeMode);
  const accent = useUiStore((s) => s.themeAccent);
  const setAccent = useUiStore((s) => s.setThemeAccent);
  const qc = useQueryClient();
  const { data } = useQuery({ queryKey: ['scan-status'], queryFn: api.scanStatus });

  return (
    <div className="space-y-8 p-6">
      <h1 className="text-2xl font-semibold">{t('Settings')}</h1>
      <section>
        <h2 className="mb-3 text-sm font-medium">{t('Language')}</h2>
        <div className="flex gap-2">
          {LOCALES.map((item) => (
            <Button
              key={item.id}
              variant={locale === item.id ? 'default' : 'outline'}
              onClick={() => setLocale(item.id)}
            >
              {item.label(t)}
            </Button>
          ))}
        </div>
      </section>
      <section>
        <h2 className="mb-3 text-sm font-medium">{t('Appearance')}</h2>
        <div className="flex gap-2">
          {THEME_MODES.map((m) => (
            <Button key={m.id} variant={mode === m.id ? 'default' : 'outline'} onClick={() => setMode(m.id)}>
              {m.label(t)}
            </Button>
          ))}
        </div>
        <p className="mb-2 mt-4 text-sm text-muted-foreground">{t('Accent color')}</p>
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
              {c.label(t)}
            </button>
          ))}
        </div>
      </section>
      <section>
        <h2 className="mb-3 text-sm font-medium">{t('Library')}</h2>
        <p className="mb-3 text-sm text-muted-foreground">
          {t('Scan status: {state}', { state: data?.state ?? 'idle' })}
          {data?.state === 'running' ? t(' ({scanned}/{total})', { scanned: data.scanned, total: data.total }) : ''}
        </p>
        <Button
          onClick={() => {
            void api.startScan().then((s) => qc.setQueryData(['scan-status'], s));
          }}
        >
          {t('Scan now')}
        </Button>
      </section>
    </div>
  );
}
