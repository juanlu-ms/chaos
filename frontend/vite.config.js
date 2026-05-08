import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

export default defineConfig({
  plugins: [react()],
  base: '/',
  build: {
    outDir: '../orchestrator/src/interfaces/web/static',
    emptyOutDir: true,
  },
  server: {
    proxy: {
      '/api': {
        target: 'http://127.0.0.1:8080',
        changeOrigin: true,
      },
      '/events': {
        target: 'http://127.0.0.1:8080',
        changeOrigin: true,
      },
      '/containers': {
        target: 'http://127.0.0.1:8080',
        changeOrigin: true,
      },
    },
  },
});
