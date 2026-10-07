import type { Visualizer } from './types';
import { CircleVisualizer } from './visualizers/circle';
import { SpectrumBarsVisualizer } from './visualizers/spectrumBars';
import { WaveformVisualizer } from './visualizers/waveform';

export type VisualizerId = 'spectrum-bars' | 'spectrum-circle' | 'waveform' | 'none';

export const VISUALIZER_IDS: VisualizerId[] = [
  'spectrum-bars',
  'spectrum-circle',
  'waveform',
  'none',
];

export function createVisualizer(id: VisualizerId): Visualizer | null {
  switch (id) {
    case 'none':
      return null;
    case 'spectrum-circle':
      return new CircleVisualizer();
    case 'waveform':
      return new WaveformVisualizer();
    case 'spectrum-bars':
    default:
      return new SpectrumBarsVisualizer();
  }
}

export function isVisualizerId(v: string | null | undefined): v is VisualizerId {
  return !!v && (VISUALIZER_IDS as string[]).includes(v);
}
