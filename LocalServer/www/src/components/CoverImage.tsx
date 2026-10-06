import { coverUrl } from '@/api/client';
import { cn } from '@/lib/utils';
import defaultArtist from '@/assets/default-artist.jpg';
import defaultCover from '@/assets/default-cover.jpg';

type CoverKind = 'album' | 'artist';

export function CoverImage({
  songId,
  kind = 'album',
  className,
  alt = '',
}: {
  songId?: number | null;
  kind?: CoverKind;
  className?: string;
  alt?: string;
}) {
  const fallback = kind === 'artist' ? defaultArtist : defaultCover;
  const remote = songId != null && songId > 0 ? coverUrl(songId) : null;

  return (
    <div
      className={cn('relative overflow-hidden bg-muted bg-cover bg-center', className)}
      style={{ backgroundImage: `url(${fallback})` }}
      role={alt ? 'img' : undefined}
      aria-label={alt || undefined}
    >
      {remote ? (
        <img
          key={remote}
          src={remote}
          alt=""
          className="absolute inset-0 h-full w-full object-cover"
          onError={(e) => {
            e.currentTarget.style.visibility = 'hidden';
          }}
        />
      ) : null}
    </div>
  );
}
