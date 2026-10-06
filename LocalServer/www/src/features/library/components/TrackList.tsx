import { Play } from 'lucide-react';
import type { Song } from '@/api/types';
import { formatDuration } from '@/lib/utils';
import { usePlayerStore } from '@/features/player/store';
import { mediaMenuFromEvent, type MediaMenuTarget } from './MediaContextMenu';

export function TrackList({
  songs,
  onMenu,
}: {
  songs: Song[];
  onMenu: (t: MediaMenuTarget) => void;
}) {
  const playSongs = usePlayerStore((s) => s.playSongs);
  return (
    <div className="space-y-1">
      {songs.map((song, i) => (
        <button
          key={song.id}
          className="flex w-full items-center gap-3 rounded-md px-2 py-2 text-left hover:bg-accent"
          onClick={() => playSongs(songs, i)}
          onContextMenu={(e) => onMenu(mediaMenuFromEvent(e, [song]))}
        >
          <span className="w-8 shrink-0 text-right text-xs text-muted-foreground">{i + 1}</span>
          <Play size={14} className="shrink-0 text-muted-foreground" />
          <span className="min-w-0 flex-1 truncate text-sm">{song.title}</span>
          <span className="hidden min-w-0 max-w-[30%] truncate text-xs text-muted-foreground sm:block">
            {song.artist}
          </span>
          <span className="shrink-0 text-xs text-muted-foreground">{formatDuration(song.duration)}</span>
        </button>
      ))}
      {songs.length === 0 && <p className="px-2 text-sm text-muted-foreground">没有歌曲</p>}
    </div>
  );
}
