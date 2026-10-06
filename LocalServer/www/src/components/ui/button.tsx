import { cn } from '@/lib/utils';
import type { ButtonHTMLAttributes } from 'react';

export function Button({
  className,
  variant = 'default',
  ...props
}: ButtonHTMLAttributes<HTMLButtonElement> & { variant?: 'default' | 'ghost' | 'outline' }) {
  const styles = {
    default: 'bg-primary text-primary-foreground hover:opacity-90',
    ghost: 'hover:bg-accent text-foreground',
    outline: 'border border-border hover:bg-accent',
  }[variant];
  return (
    <button
      className={cn(
        'inline-flex h-9 w-auto shrink-0 items-center justify-center whitespace-nowrap rounded-md px-3 text-sm font-medium transition disabled:opacity-50',
        styles,
        className,
      )}
      {...props}
    />
  );
}
