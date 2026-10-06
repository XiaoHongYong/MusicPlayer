import { cn } from '@/lib/utils';

export function Badge({ children, className }: { children: React.ReactNode; className?: string }) {
  return (
    <span
      className={cn(
        'inline-flex items-center rounded-full bg-muted px-2 py-0.5 text-xs text-muted-foreground',
        className,
      )}
    >
      {children}
    </span>
  );
}
