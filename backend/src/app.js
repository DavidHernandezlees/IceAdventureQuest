import express from 'express';
import { rateLimit } from 'express-rate-limit';
import { authenticate, openSession } from './auth.js';
import { ApiError, check, digest } from './db.js';
import { applyAction, grantPurchase, grantVideo, stage } from './rules.js';
import { complete, importProgress, mutate, snapshot, totalStars } from './store.js';
import { providers } from './providers.js';

async function recordReceipt(db, provider, reference, player, product, quantity) {
  try {
    await db.execute('INSERT INTO provider_receipts (provider,reference_hash,player_id,product,quantity) VALUES (?,?,?,?,?)',
      [provider,digest(reference),player,product,quantity]);
  } catch (err) {
    if (err.code === 'ER_DUP_ENTRY') throw new ApiError(409, 'receipt_already_used');
    throw err;
  }
}

export function createApp(pool, { services = providers(), allowImport = false, now = Date.now } = {}) {
  const app = express();
  app.disable('x-powered-by');
  const router = express.Router();
  app.use('/v1', router);
  router.use((req,res,next) => { res.set('Cache-Control','no-store'); next(); });
  router.use(express.json({ limit: '128kb' }));
  router.get('/health', async (_req,res) => {
    await pool.query('SELECT 1');
    res.json({ status: 'ok' });
  });
  const authLimit = rateLimit({ windowMs: 60000, limit: 20, standardHeaders: 'draft-7', legacyHeaders: false,
    message: { error: 'too_many_auth_requests' } });
  for (const action of ['register','login']) {
    router.post('/auth/'+action, authLimit, async (req,res) => {
      const register = action === 'register';
      res.status(register ? 201 : 200).json(await openSession(pool, req.body || {}, register));
    });
  }
  router.get('/callbacks/admob', async (req,res) => {
    const event = await services.ad(req.originalUrl.split('?').slice(1).join('?'));
    const { reference, playerId, time } = event;
    const result = await mutate(pool, playerId, 'ad:'+digest(reference).toString('hex'), 'rewarded-video', { time }, async (db,p) => {
      await recordReceipt(db,'admob',reference,p.id,'rewarded-video',1);
      return grantVideo(p.wallet, time);
    });
    res.json(result);
  });
  router.use(authenticate(pool));
  router.delete('/auth/session', async (req,res) => {
    await pool.execute('DELETE FROM sessions WHERE token_hash=?', [req.tokenHash]);
    res.sendStatus(204);
  });
  router.get('/me', async (req,res) => res.json(await snapshot(pool, req.playerId)));

  function command(path, name, execute) {
    router.post(path, async (req,res) => {
      const key = req.get('Idempotency-Key');
      check(typeof key === 'string' && /^[\w.-]{1,96}$/.test(key), 400, 'invalid_idempotency_key');
      const input = req.body ?? {};
      check(input && typeof input === 'object' && !Array.isArray(input), 400, 'invalid_body');
      const result = await mutate(pool, req.playerId, 'client:'+key, name, input, (db,p) => execute(db,p,input));
      res.json(result);
    });
  }
  command('/progress/complete', 'complete', complete);
  command('/progress/import', 'import', (db,p,input) => {
    check(allowImport, 403, 'legacy_import_disabled');
    return importProgress(db,p,input);
  });
  for (const [path, name] of [
    ['/rewards/daily','daily'], ['/inventory/buy','buy-supply'], ['/inventory/use','use-booster'],
    ['/pets/select','select-pet'], ['/pets/care','care'], ['/rewards/star-chest','star-chest']
  ]) {
    command(path, name, async (db,p,input) => applyAction(p.wallet, stage(p.unlocked_stage).mapSlot, name, input,
      now(), name === 'star-chest' ? await totalStars(db,p.id) : 0));
  }
  router.post('/purchases/google-play', async (req,res) => {
    const token = req.body?.purchaseToken;
    check(typeof token === 'string' && token.length > 0 && token.length <= 4096, 400, 'invalid_purchase_token');
    const key = 'play:'+digest(token).toString('hex');
    const [[prior]] = await pool.execute('SELECT result FROM player_events WHERE player_id=? AND request_id=?', [req.playerId,key]);
    if (prior) return res.json(prior.result);
    const { product, quantity } = await services.purchase(token, req.playerId);
    const result = await mutate(pool, req.playerId, key, 'google-play', {}, async (db,p) => {
      await recordReceipt(db,'play',token,p.id,product,quantity);
      return { ...grantPurchase(p.wallet,product,quantity), finalize: product.startsWith('pay_onetime') ? 'acknowledge' : 'consume' };
    });
    res.json(result);
  });
  app.use((_req,res) => res.status(404).json({ error: 'route_not_found' }));
  app.use((err,_req,res,_next) => {
    if (err instanceof ApiError) return res.status(err.status).json({ error: err.code });
    if (err.type === 'entity.parse.failed') return res.status(400).json({ error: 'invalid_json' });
    if (err.type === 'entity.too.large') return res.status(413).json({ error: 'body_too_large' });
    // Do not log purchase tokens, SQL parameters, passwords or bearer tokens.
    console.error('Request failed', err.code || err.name);
    if (['ECONNREFUSED','PROTOCOL_CONNECTION_LOST','ETIMEDOUT','ER_LOCK_WAIT_TIMEOUT','ER_LOCK_DEADLOCK'].includes(err.code))
      return res.status(503).json({ error: 'temporarily_unavailable' });
    return res.status(500).json({ error: 'internal_error' });
  });
  return app;
}
