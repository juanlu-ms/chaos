import { defineConfig } from 'vite';

export default defineConfig({
  base: '/',
  build: {
    outDir: '../orchestrator/src/interfaces/web/static',
    emptyOutDir: true,
  },
});
