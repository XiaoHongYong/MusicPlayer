import type { ReactNode } from 'react';

export function FilterSelect({
  value,
  onChange,
  children,
  'aria-label': ariaLabel,
}: {
  value: string;
  onChange: (v: string) => void;
  children: ReactNode;
  'aria-label'?: string;
}) {
  return (
    <select
      className="h-9 rounded-md border border-border bg-background px-2 text-sm"
      value={value}
      aria-label={ariaLabel}
      onChange={(e) => onChange(e.target.value)}
    >
      {children}
    </select>
  );
}
