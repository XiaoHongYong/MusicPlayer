import { useEffect, useMemo, useRef, useState } from 'react';
import { createPortal } from 'react-dom';
import { useQuery, useQueryClient } from '@tanstack/react-query';
import { ChevronRight } from 'lucide-react';
import { api } from '@/api/client';
import type { Song } from '@/api/types';
import { usePlayerStore } from '@/features/player/store';
import { useToastStore } from '@/stores/toast-store';
import { useUiStore } from '@/stores/ui-store';

export interface MediaMenuTarget {
  x: number;
  y: number;
  songs: Song[];
}

export function mediaMenuFromEvent(
  e: React.MouseEvent,
  songs: Song[],
): MediaMenuTarget {
  e.preventDefault();
  e.stopPropagation();
  return { x: e.clientX, y: e.clientY, songs };
}

const itemClass = 'block w-full px-3 py-2 text-left text-sm hover:bg-accent';

export function MediaContextMenu({
  target,
  onClose,
}: {
  target: MediaMenuTarget;
  onClose: () => void;
}) {
  const ref = useRef<HTMLDivElement>(null);
  const playImmediately = useUiStore((s) => s.playImmediately);
  const addToQueueFront = useUiStore((s) => s.addToQueueFront);
  const setPlayImmediately = useUiStore((s) => s.setPlayImmediately);
  const setAddToQueueFront = useUiStore((s) => s.setAddToQueueFront);
  const [playlistOpen, setPlaylistOpen] = useState(false);
  const [creating, setCreating] = useState(false);
  const [newName, setNewName] = useState('');
  const qc = useQueryClient();
  const { data: playlists = [] } = useQuery({ queryKey: ['playlists'], queryFn: api.playlists });

  const recent = useMemo(
    () => [...playlists].sort((a, b) => b.time_modified - a.time_modified).slice(0, 10),
    [playlists],
  );

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (e.key === 'Escape') onClose();
    };
    const onDown = (e: MouseEvent) => {
      if (ref.current && !ref.current.contains(e.target as Node)) onClose();
    };
    window.addEventListener('keydown', onKey);
    window.addEventListener('mousedown', onDown);
    return () => {
      window.removeEventListener('keydown', onKey);
      window.removeEventListener('mousedown', onDown);
    };
  }, [onClose]);

  const songs = target.songs;
  const run = (mode: 'addThis' | 'addAll') => {
    const ui = useUiStore.getState();
    const result = usePlayerStore.getState().applyQueueAction({
      mode,
      thisSongs: songs,
      allSongs: songs,
      startIndex: 0,
      playNow: ui.playImmediately,
      addToFront: ui.addToQueueFront,
    });
    const verb = result.kind === 'replace' ? '已替换' : '已添加';
    useToastStore.getState().show(`${verb} ${result.count} 首歌曲`);
    onClose();
  };

  const ids = songs.map((s) => s.id);
  const addToSaved = async (playlistId: number, name: string) => {
    if (!ids.length) return;
    await api.addPlaylistSongs(playlistId, ids);
    void qc.invalidateQueries({ queryKey: ['playlists'] });
    void qc.invalidateQueries({ queryKey: ['playlist', playlistId] });
    useToastStore.getState().show(`已将 ${ids.length} 首添加到「${name}」`);
    onClose();
  };

  const createAndAdd = async () => {
    const name = newName.trim();
    if (!name || !ids.length) return;
    const pl = await api.createPlaylist(name);
    await api.addPlaylistSongs(pl.id, ids);
    void qc.invalidateQueries({ queryKey: ['playlists'] });
    useToastStore.getState().show(`已创建「${name}」并添加 ${ids.length} 首`);
    onClose();
  };

  const menuW = 256;
  const subW = 256;
  let left = Math.max(8, target.x);
  if (left + menuW > window.innerWidth - 8) left = window.innerWidth - menuW - 8;
  const top = Math.min(Math.max(8, target.y), window.innerHeight - 340);
  const flyLeft = left + menuW + subW > window.innerWidth - 8;
  const showSub = playlistOpen || creating;

  return createPortal(
    <div
      ref={ref}
      className="fixed z-[60] w-64 rounded-lg border border-border bg-card py-1 shadow-xl"
      style={{ left: Math.max(8, left), top: Math.max(8, top) }}
      onContextMenu={(e) => e.preventDefault()}
    >
      <button className={itemClass} onClick={() => run('addThis')}>
        添加此歌曲到播放列表
      </button>
      <button className={itemClass} onClick={() => run('addAll')}>
        添加所有歌曲到播放列表
      </button>
      <button
        className={itemClass}
        onClick={() => {
          usePlayerStore.getState().playNext(songs);
          useToastStore.getState().show(`已将 ${songs.length} 首设为下一首`);
          onClose();
        }}
      >
        下一首播放
      </button>
      <div
        className="relative"
        onPointerEnter={() => setPlaylistOpen(true)}
        onPointerLeave={() => {
          if (!creating) setPlaylistOpen(false);
        }}
      >
        <button type="button" className={`${itemClass} flex items-center justify-between`}>
          添加到播放列表
          <ChevronRight size={14} className="text-muted-foreground" />
        </button>
        {showSub && (
          <div
            className="absolute top-0 z-[61] w-64 py-0"
            style={
              flyLeft
                ? { right: '100%', paddingRight: 4 }
                : { left: '100%', paddingLeft: 4 }
            }
          >
            <div className="rounded-lg border border-border bg-card py-1 shadow-xl">
              {creating ? (
                <form
                  className="flex gap-1 px-3 py-2"
                  onSubmit={(e) => {
                    e.preventDefault();
                    void createAndAdd().catch(() =>
                      useToastStore.getState().show('创建播放列表失败'),
                    );
                  }}
                >
                  <input
                    autoFocus
                    className="h-8 min-w-0 flex-1 rounded-md border border-border bg-background px-2 text-sm"
                    placeholder="新播放列表名称"
                    value={newName}
                    onChange={(e) => setNewName(e.target.value)}
                  />
                  <button
                    type="submit"
                    className="shrink-0 text-sm text-primary disabled:opacity-50"
                    disabled={!newName.trim()}
                  >
                    创建
                  </button>
                </form>
              ) : (
                <button className={itemClass} onClick={() => setCreating(true)}>
                  创建播放列表
                </button>
              )}
              {recent.length > 0 && <div className="my-1 border-t border-border" />}
              {recent.map((p) => (
                <button
                  key={p.id}
                  className={`${itemClass} flex items-center justify-between gap-2`}
                  onClick={() =>
                    void addToSaved(p.id, p.name).catch(() =>
                      useToastStore.getState().show('添加到播放列表失败'),
                    )
                  }
                >
                  <span className="truncate">{p.name}</span>
                  <span className="shrink-0 text-xs text-muted-foreground">{p.count}</span>
                </button>
              ))}
              {recent.length === 0 && !creating && (
                <p className="px-3 py-2 text-xs text-muted-foreground">还没有播放列表</p>
              )}
            </div>
          </div>
        )}
      </div>
      <div className="my-1 border-t border-border" />
      <label className="flex cursor-pointer items-center gap-2 px-3 py-2 text-sm">
        <input
          type="checkbox"
          checked={playImmediately}
          onChange={(e) => setPlayImmediately(e.target.checked)}
        />
        立即播放
      </label>
      <label className="flex cursor-pointer items-center gap-2 px-3 py-2 text-sm">
        <input
          type="checkbox"
          checked={addToQueueFront}
          onChange={(e) => setAddToQueueFront(e.target.checked)}
        />
        添加到队首
      </label>
    </div>,
    document.body,
  );
}
