// Read CSS custom property from :root, used by Recharts components so that
// chart colors track the active theme reactively.
export function readVar(name, fallback = '#888') {
  if (typeof window === 'undefined') return fallback;
  const v = getComputedStyle(document.documentElement)
    .getPropertyValue(name)
    .trim();
  return v || fallback;
}

export const THEMES = ['amber', 'dark', 'cyber'];
