import { isIP } from 'node:net';
import { createPool } from './db.js';
import { createApp } from './app.js';

const pool = createPool();
await pool.query('SELECT id FROM players LIMIT 0');
const app = createApp(pool, { allowImport: process.env.ALLOW_LEGACY_IMPORT === 'true' });
if (process.env.TRUSTED_PROXY_IPS) {
  const trusted = process.env.TRUSTED_PROXY_IPS.split(',').map(ip => ip.trim());
  if (trusted.some(ip => !isIP(ip))) throw new Error('TRUSTED_PROXY_IPS requires literal IP addresses');
  app.set('trust proxy', trusted);
}
const port = Number(process.env.PORT || 3000), host = process.env.HOST || '127.0.0.1';
const server = app.listen(port, host, err => {
  if (err) { console.error('Could not listen', err.code); process.exitCode = 1; return void pool.end(); }
  console.log('Ice Adventure Quest API listening on', host, port);
});
function shutdown() { server.close(async () => { await pool.end(); }); }
process.once('SIGTERM', shutdown);
process.once('SIGINT', shutdown);
