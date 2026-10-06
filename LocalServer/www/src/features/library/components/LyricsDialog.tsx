import { useQuery } from '@tanstack/react-query';
import { api } from '@/api/client';
import { Dialog } from '@/components/ui/dialog';
import { LyricsPanel } from '@/features/player/components/LyricsPanel';
import { useUiStore } from '@/stores/ui-store';
import { useT } from '@/i18n';

export function LyricsDialog() {
  const t = useT();
  const songId = useUiStore((s) => s.lyricsSongId);
  const close = useUiStore((s) => s.closeLyrics);
  const { data: snapshot } = useQuery({ queryKey: ['library-snapshot'], queryFn: api.snapshot });
  const song = snapshot?.songs.find((s) => s.id === songId);

  return (
    <Dialog open={songId != null} title={song ? t('{title} · Lyrics', { title: song.title }) : t('Lyrics')} onClose={close} className="max-w-xl">
      <div className="h-[60vh]">
        <LyricsPanel songId={songId} position={0} compact />
      </div>
    </Dialog>
  );
}
