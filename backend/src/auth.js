import { randomBytes, randomUUID, scrypt, timingSafeEqual } from 'node:crypto';
import { promisify } from 'node:util';
import { check, digest, transaction } from './db.js';
import { initialWallet, stages } from './rules.js';

const derive = promisify(scrypt);
const dummy = '0'.repeat(32) + ':' + '0'.repeat(128);
let hashing = 0;

async function passwordKey(password, salt) {
  check(hashing < 4, 429, 'auth_busy');
  hashing++;
  try { return await derive(password, salt, 64, { N: 32768, r: 8, p: 3, maxmem: 64 * 1024 ** 2 }); }
  finally { hashing--; }
}

export async function openSession(pool, input, register) {
  const username = typeof input.username === 'string' ? input.username.toLowerCase() : '';
  check(/^[a-z0-9_]{3,32}$/.test(username), 400, 'invalid_username');
  const password = input.password;
  check(typeof password === 'string' && password.length >= 12 && Buffer.byteLength(password) <= 256,
    400, 'invalid_password_length');
  let playerId, passwordHash;
  if (register) {
    const salt = randomBytes(16).toString('hex');
    passwordHash = salt + ':' + (await passwordKey(password, salt)).toString('hex');
    playerId = randomUUID();
  } else {
    const [[player]] = await pool.execute('SELECT id,password_hash FROM players WHERE username=?', [username]);
    const [salt, hash] = (player?.password_hash || dummy).split(':');
    const key = await passwordKey(password, salt);
    check(timingSafeEqual(key, Buffer.from(hash, 'hex')) && player, 401, 'invalid_credentials');
    playerId = player.id;
  }
  const token = randomBytes(32).toString('base64url');
  return transaction(pool, async db => {
    if (register) {
      try {
        await db.execute('INSERT INTO players (id,username,password_hash,unlocked_stage,wallet) VALUES (?,?,?,?,?)',
          [playerId, username, passwordHash, stages[0].id, JSON.stringify(initialWallet())]);
      } catch (err) {
        if (err.code === 'ER_DUP_ENTRY') check(false, 409, 'username_taken');
        throw err;
      }
    }
    await db.execute('INSERT INTO sessions (token_hash,player_id,expires_at) VALUES (?,?,UTC_TIMESTAMP()+INTERVAL 30 DAY)',
      [digest(token), playerId]);
    return { playerId, token, expiresInSeconds: 2592000 };
  });
}

export function authenticate(pool) {
  return async (req, res, next) => {
    const token = /^Bearer ([A-Za-z0-9_-]{43})$/i.exec(req.get('authorization') || '')?.[1];
    check(token, 401, 'sign_in_required');
    req.tokenHash = digest(token);
    const [[session]] = await pool.execute('SELECT player_id FROM sessions WHERE token_hash=? AND expires_at>UTC_TIMESTAMP()', [req.tokenHash]);
    check(session, 401, 'session_expired');
    req.playerId = session.player_id;
    next();
  };
}
