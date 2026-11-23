import mysql from 'mysql2/promise';
import { readFileSync } from 'node:fs';
import { createHash } from 'node:crypto';

export class ApiError extends Error {
  constructor(status, code) { super(code); this.status = status; this.code = code; }
}

export function check(condition, status, code) {
  if (!condition) throw new ApiError(status, code);
}

export function digest(value) { return createHash('sha256').update(value).digest(); }

export function createPool(uri = process.env.MYSQL_URL) {
  if (!uri) throw new Error('Set MYSQL_URL in backend/.env');
  const url = new URL(uri);
  if (url.protocol !== 'mysql:' || url.pathname.length < 2) throw new Error('Invalid MYSQL_URL');
  return mysql.createPool({
    host: url.hostname, port: Number(url.port || 3306), user: decodeURIComponent(url.username),
    password: decodeURIComponent(url.password), database: decodeURIComponent(url.pathname.slice(1)),
    connectionLimit: 10, queueLimit: 100, timezone: 'Z', charset: 'utf8mb4',
    ssl: process.env.MYSQL_SSL_CA_FILE ? { ca: readFileSync(process.env.MYSQL_SSL_CA_FILE), rejectUnauthorized: true, verifyIdentity: true } : undefined
  });
}

export async function transaction(pool, run) {
  const db = await pool.getConnection();
  try {
    await db.beginTransaction();
    const result = await run(db);
    await db.commit();
    return result;
  } catch (err) {
    try { await db.rollback(); } catch { db.destroy(); }
    throw err;
  } finally { db.release(); }
}

export async function migrate(pool) {
  const sql = readFileSync(new URL('../schema.sql', import.meta.url), 'utf8');
  for (const statement of sql.split(';').map(s => s.trim()).filter(Boolean))
    await pool.query(statement);
}
