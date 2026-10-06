import { create } from 'zustand';

interface ToastItem {
  id: number;
  message: string;
}

interface ToastState {
  items: ToastItem[];
  show: (message: string) => void;
  dismiss: (id: number) => void;
}

let nextId = 1;

export const useToastStore = create<ToastState>((set) => ({
  items: [],
  show: (message) => {
    const id = nextId++;
    set((s) => ({ items: [...s.items, { id, message }] }));
    window.setTimeout(() => {
      set((s) => ({ items: s.items.filter((t) => t.id !== id) }));
    }, 2800);
  },
  dismiss: (id) => set((s) => ({ items: s.items.filter((t) => t.id !== id) })),
}));
