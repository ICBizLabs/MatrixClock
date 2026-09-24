// Minimal service worker: makes the page installable on secure origins; all requests go straight to the clock.
self.addEventListener('install', () => self.skipWaiting());
self.addEventListener('activate', e => e.waitUntil(self.clients.claim()));
self.addEventListener('fetch', () => {});
