// Service Worker for 智能家居工作台 PWA
const CACHE_NAME = 'smarthome-workbench-v2';

// Only cache static assets, NOT HTML
const STATIC_ASSETS = [
  './manifest.json',
  './icon-192.png',
  './icon-512.png',
  './apple-touch-icon.png',
  './favicon.ico'
];

self.addEventListener('install', event => {
  event.waitUntil(
    caches.open(CACHE_NAME).then(cache => {
      return cache.addAll(STATIC_ASSETS).catch(() => {});
    })
  );
  self.skipWaiting();
});

self.addEventListener('activate', event => {
  event.waitUntil(
    caches.keys().then(keys => {
      return Promise.all(
        keys.filter(key => key !== CACHE_NAME).map(key => caches.delete(key))
      );
    })
  );
  self.clients.claim();
});

self.addEventListener('fetch', event => {
  if (event.request.method !== 'GET') return;
  const url = new URL(event.request.url);

  // HTML files: ALWAYS network first, never cache
  if (event.request.destination === 'document' || url.pathname.endsWith('.html') || url.pathname === '/' || url.pathname === '') {
    event.respondWith(
      fetch(event.request).catch(() => {
        return caches.match(event.request);
      })
    );
    return;
  }

  // Static assets: cache-first for performance
  event.respondWith(
    caches.match(event.request).then(cached => {
      return cached || fetch(event.request).then(response => {
        if (response.status === 200) {
          const cloned = response.clone();
          caches.open(CACHE_NAME).then(cache => cache.put(event.request, cloned));
        }
        return response;
      });
    })
  );
});
