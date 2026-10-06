import { useEffect, useState } from 'react';
import { sseManager, type SseStatus } from './sse';

/** 顶栏连接灯：只看 SSE，不轮询 HTTP。 */
export function useRealtimeStatus() {
  const [status, setStatus] = useState<SseStatus>(() => sseManager.getStatus());

  useEffect(() => {
    sseManager.start();
    return sseManager.subscribe(setStatus);
  }, []);

  return {
    status,
    sseStatus: status,
    reconnect: () => sseManager.reconnectNow(),
  };
}
