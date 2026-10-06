import { ChevronLeft, ChevronRight, ChevronsLeft, ChevronsRight } from 'lucide-react';
import { FilterSelect } from '@/features/library/components/FilterSelect';
import {
  STATS_PAGE_SIZES,
  clampPage,
  pageCount,
  pageWindow,
  parseStatsPageSize,
  type StatsPageSize,
} from '@/features/statistics/pagination';
import { useT } from '@/i18n';
import { Button } from './ui/button';

export function TablePagination({
  total,
  page,
  pageSize,
  onPageChange,
  onPageSizeChange,
}: {
  total: number;
  page: number;
  pageSize: StatsPageSize;
  onPageChange: (page: number) => void;
  onPageSizeChange: (size: StatsPageSize) => void;
}) {
  const t = useT();
  const pages = pageCount(total, pageSize);
  const current = clampPage(page, total, pageSize);
  const from = total === 0 ? 0 : (current - 1) * pageSize + 1;
  const to = Math.min(current * pageSize, total);
  const tokens = pageWindow(current, pages);

  return (
    <div className="mt-3 flex flex-wrap items-center justify-between gap-3 text-sm text-muted-foreground">
      <span>
        {t('{from}–{to} of {n}', {
          from,
          to,
          n: total,
        })}
      </span>
      <div className="flex flex-wrap items-center gap-1">
        <Button
          type="button"
          variant="ghost"
          className="h-8 w-8 px-0"
          disabled={current <= 1}
          aria-label={t('First page')}
          onClick={() => onPageChange(1)}
        >
          <ChevronsLeft size={16} />
        </Button>
        <Button
          type="button"
          variant="ghost"
          className="h-8 px-2"
          disabled={current <= 1}
          onClick={() => onPageChange(current - 1)}
        >
          <ChevronLeft size={16} />
          {t('Previous')}
        </Button>
        {tokens.map((token, i) =>
          token === 'ellipsis' ? (
            <span key={`e-${i}`} className="px-1" aria-hidden>
              …
            </span>
          ) : (
            <Button
              key={token}
              type="button"
              variant={token === current ? 'default' : 'ghost'}
              className="h-8 min-w-8 px-2 tabular-nums"
              aria-current={token === current ? 'page' : undefined}
              aria-label={t('Go to page {n}', { n: token })}
              onClick={() => onPageChange(token)}
            >
              {token}
            </Button>
          ),
        )}
        <Button
          type="button"
          variant="ghost"
          className="h-8 px-2"
          disabled={current >= pages}
          onClick={() => onPageChange(current + 1)}
        >
          {t('Next')}
          <ChevronRight size={16} />
        </Button>
        <Button
          type="button"
          variant="ghost"
          className="h-8 w-8 px-0"
          disabled={current >= pages}
          aria-label={t('Last page')}
          onClick={() => onPageChange(pages)}
        >
          <ChevronsRight size={16} />
        </Button>
      </div>
      <FilterSelect
        aria-label={t('Per page')}
        value={String(pageSize)}
        onChange={(v) => onPageSizeChange(parseStatsPageSize(v, pageSize))}
      >
        {STATS_PAGE_SIZES.map((n) => (
          <option key={n} value={String(n)}>
            {t('{n} / page', { n })}
          </option>
        ))}
      </FilterSelect>
    </div>
  );
}
