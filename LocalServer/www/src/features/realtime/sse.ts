type SseStatus = 'connecting' | 'connected' | 'disconnected';

type Listener = (status: SseStatus) => void;

export type SseEnvelope = {
  event: string;
  state_version: number;
  data: unknown;
};

type EventHandler = (envelope: SseEnvelope) => void;

class EventSourceManager {
  private es: EventSource | null = null;
  private status: SseStatus = 'disconnected';
  private listeners = new Set<Listener>();
  private eventHandlers = new Set<EventHandler>();
  private stopped = true;
  private lastVersion = 0;

  start() {
    if (!this.stopped && this.es) return;
    this.stopped = false;
    this.connect();
  }

  reconnectNow() {
    this.stopped = false;
    this.disconnectSocket();
    this.connect();
  }

  stop() {
    this.stopped = true;
    this.disconnectSocket();
    this.setStatus('disconnected');
  }

  getStatus() {
    return this.status;
  }

  lastStateVersion() {
    return this.lastVersion;
  }

  subscribe(fn: Listener): () => void {
    this.listeners.add(fn);
    fn(this.status);
    return () => {
      this.listeners.delete(fn);
    };
  }

  onEvent(fn: EventHandler): () => void {
    this.eventHandlers.add(fn);
    return () => {
      this.eventHandlers.delete(fn);
    };
  }

  private setStatus(status: SseStatus) {
    if (this.status === status) return;
    this.status = status;
    this.listeners.forEach((fn) => fn(status));
  }

  private disconnectSocket() {
    if (this.es) {
      this.es.close();
      this.es = null;
    }
  }

  private connect() {
    if (this.stopped) return;
    this.setStatus('connecting');
    this.disconnectSocket();
    const es = new EventSource('/api/v1/events');
    this.es = es;

    const names = [
      'player.state_changed',
      'player.song_changed',
      'player.queue_changed',
      'library.scan_started',
      'library.scan_progress',
      'library.scan_finished',
      'library.updated',
      'rating.changed',
      'playlist.updated',
      'history.updated',
    ];
    for (const name of names) {
      es.addEventListener(name, (ev) => this.dispatch(ev as MessageEvent));
    }
    es.onmessage = (ev) => this.dispatch(ev);
    es.onopen = () => {
      this.setStatus('connected');
    };
    es.onerror = () => {
      if (this.stopped) return;
      this.setStatus('disconnected');
    };
  }

  private dispatch(ev: MessageEvent) {
    if (!ev.data) return;
    try {
      const envelope = JSON.parse(ev.data) as SseEnvelope;
      if (typeof envelope.state_version === 'number') {
        if (envelope.state_version < this.lastVersion) return;
        this.lastVersion = envelope.state_version;
      }
      this.eventHandlers.forEach((fn) => fn(envelope));
    } catch {
      /* 心跳或非 JSON */
    }
  }
}

export type { SseStatus };
export const sseManager = new EventSourceManager();
