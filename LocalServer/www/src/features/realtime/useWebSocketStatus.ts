import { useEffect, useState } from 'react';
import { wsManager, type WsStatus } from './websocket';

export function useWebSocketStatus() {
  const [status, setStatus] = useState<WsStatus>(() => wsManager.getStatus());

  useEffect(() => {
    wsManager.start();
    const unsub = wsManager.subscribe(setStatus);
    return () => {
      unsub();
    };
  }, []);

  return status;
}
