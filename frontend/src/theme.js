// Read CSS custom property from :root, used by Recharts components so that
// chart colors track the active theme reactively.
export function readVar(name, fallback = '#888') {
  if (typeof window === 'undefined') return fallback;
  const v = getComputedStyle(document.documentElement)
    .getPropertyValue(name)
    .trim();
  return v || fallback;
}

export const DEFAULT_THEME = 'amber';

export const THEMES = [
  'amber',
  'dark',
  'cyber',
  'light',
  'matrix',
  'synthwave',
  'solarized',
  'onedark',
  'tokyonight',
  'nord',
  'catppuccin',
];
