import { useToastStore } from '@/stores/toast-store';

export function ToastHost() {
  const items = useToastStore((s) => s.items);
  if (!items.length) return null;
  return (
    <div className="pointer-events-none fixed bottom-24 right-4 z-[70] flex flex-col gap-2">
      {items.map((t) => (
        <div
          key={t.id}
          role="status"
          className="rounded-md bg-foreground px-3 py-2 text-sm text-background shadow-lg"
        >
          {t.message}
        </div>
      ))}
    </div>
  );
}
