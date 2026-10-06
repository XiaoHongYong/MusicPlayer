type WsStatus = 'connecting' | 'connected' | 'disconnected';

type Listener = (status: WsStatus) => void;

function wsUrl() {
  const proto = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
  const host = window.location.hostname || '127.0.0.1';
  // LocalServer WebSocket 默认端口 12121（与 HTTP 12120 分离）
  return `${proto}//${host}:12121`;
}

class WebSocketManager {
  private ws: WebSocket | null = null;
  private status: WsStatus = 'disconnected';
  private listeners = new Set<Listener>();
  private reconnectTimer: number | null = null;
  private stopped = true;
  private attempt = 0;

  start() {
    if (!this.stopped && this.ws) return;
    this.stopped = false;
    this.connect();
  }

  stop() {
    this.stopped = true;
    if (this.reconnectTimer != null) {
      window.clearTimeout(this.reconnectTimer);
      this.reconnectTimer = null;
    }
    if (this.ws) {
      this.ws.onopen = null;
      this.ws.onclose = null;
      this.ws.onerror = null;
      this.ws.onmessage = null;
      try {
        this.ws.close();
      } catch {
        /* ignore */
      }
      this.ws = null;
    }
    this.setStatus('disconnected');
  }

  getStatus() {
    return this.status;
  }

  subscribe(fn: Listener): () => void {
    this.listeners.add(fn);
    fn(this.status);
    return () => {
      this.listeners.delete(fn);
    };
  }

  private setStatus(status: WsStatus) {
    if (this.status === status) return;
    this.status = status;
    this.listeners.forEach((fn) => fn(status));
  }

  private connect() {
    if (this.stopped) return;
    this.setStatus(this.attempt === 0 ? 'connecting' : 'connecting');
    try {
      const ws = new WebSocket(wsUrl());
      this.ws = ws;
      ws.binaryType = 'arraybuffer';
      ws.onopen = () => {
        this.attempt = 0;
        this.setStatus('connected');
      };
      ws.onclose = () => {
        this.ws = null;
        this.setStatus('disconnected');
        this.scheduleReconnect();
      };
      ws.onerror = () => {
        // onclose 会接着触发
      };
    } catch {
      this.setStatus('disconnected');
      this.scheduleReconnect();
    }
  }

  private scheduleReconnect() {
    if (this.stopped || this.reconnectTimer != null) return;
    const delay = Math.min(10_000, 800 * 2 ** Math.min(this.attempt, 4));
    this.attempt += 1;
    this.reconnectTimer = window.setTimeout(() => {
      this.reconnectTimer = null;
      this.connect();
    }, delay);
  }
}

export type { WsStatus };
export const wsManager = new WebSocketManager();
