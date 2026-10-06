import { Star } from 'lucide-react';
import { cn } from '@/lib/utils';

export function RatingStars({
  value,
  onChange,
  size = 14,
}: {
  value: number;
  onChange?: (rating: number) => void;
  size?: number;
}) {
  return (
    <div className="flex items-center gap-0.5" title={`${value.toFixed(1)} / 5`}>
      {[1, 2, 3, 4, 5].map((n) => {
        const filled = value >= n - 0.25;
        const half = !filled && value >= n - 0.75;
        return (
          <button
            key={n}
            type="button"
            className="text-muted-foreground hover:text-primary disabled:hover:text-muted-foreground"
            disabled={!onChange}
            onClick={() => onChange?.(value === n ? 0 : n)}
          >
            <Star
              size={size}
              className={cn(filled || half ? 'fill-primary text-primary' : '', half && 'opacity-50')}
            />
          </button>
        );
      })}
    </div>
  );
}
