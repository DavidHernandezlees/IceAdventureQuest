import { createPool, migrate } from './db.js';
const pool = createPool();
try { await migrate(pool); console.log('MySQL schema ready.'); }
finally { await pool.end(); }
