import { check, digest, transaction } from './db.js';
import { scoreStars, stage, stages } from './rules.js';

export async function snapshot(pool, id) {
  return transaction(pool, async db => {
    const [[p]] = await db.execute('SELECT username,unlocked_stage,wallet,revision,imported FROM players WHERE id=? FOR SHARE', [id]);
    const [levels] = await db.execute('SELECT stage_id AS stageId,best_score AS score,best_stars AS stars FROM level_progress WHERE player_id=?', [id]);
    return { playerId: id, username: p.username, revision: p.revision, unlockedStageId: p.unlocked_stage,
      imported: Boolean(p.imported), wallet: p.wallet, levels };
  });
}

function canonical(value, depth = 0) {
  check(depth <= 10, 400, 'input_too_deep');
  if (Array.isArray(value)) return value.map(item => canonical(item, depth+1));
  if (value && typeof value === 'object')
    return Object.fromEntries(Object.keys(value).sort().map(key => [key, canonical(value[key], depth+1)]));
  return value;
}

// Every writer takes the same player lock before consulting the journal or advancing the save.
export async function mutate(pool, id, key, operation, input, run) {
  check(typeof key === 'string' && /^[a-zA-Z0-9:_.-]{1,128}$/.test(key), 400, 'invalid_idempotency_key');
  const fingerprint = digest(JSON.stringify([operation, canonical(input)]));
  return transaction(pool, async db => {
    const [[p]] = await db.execute('SELECT id,unlocked_stage,wallet,revision,imported FROM players WHERE id=? FOR UPDATE', [id]);
    check(p, 404, 'unknown_player');
    const [[prior]] = await db.execute('SELECT fingerprint,result FROM player_events WHERE player_id=? AND request_id=?', [id,key]);
    if (prior) {
      check(prior.fingerprint.equals(fingerprint), 409, 'idempotency_key_reused');
      return prior.result;
    }
    const before = structuredClone(p.wallet);
    const result = { revision: p.revision + 1, ...await run(db, p) };
    const changes = {};
    for (const [name, after] of Object.entries(p.wallet)) {
      if (JSON.stringify(before[name]) !== JSON.stringify(after)) changes[name] = { before: before[name], after };
    }
    await db.execute('UPDATE players SET unlocked_stage=?,wallet=?,imported=?,revision=? WHERE id=?',
      [p.unlocked_stage, JSON.stringify(p.wallet), p.imported, result.revision, id]);
    await db.execute('INSERT INTO player_events (player_id,request_id,operation,fingerprint,result,wallet_changes) VALUES (?,?,?,?,?,?)',
      [id,key,operation,fingerprint,JSON.stringify(result),JSON.stringify(changes)]);
    return result;
  });
}

export async function complete(db, p, input) {
  const level = stage(input.stageId);
  const unlocked = stage(p.unlocked_stage);
  check(level.index <= unlocked.index, 409, 'stage_locked');
  const stars = scoreStars(level, input.score);
  await db.execute(`INSERT INTO level_progress (player_id,stage_id,best_score,best_stars) VALUES (?,?,?,?) AS new
    ON DUPLICATE KEY UPDATE best_score=GREATEST(level_progress.best_score,new.best_score),best_stars=GREATEST(level_progress.best_stars,new.best_stars)`,
    [p.id,level.id,input.score,stars]);
  if (level.index === unlocked.index) p.unlocked_stage = stages[Math.min(level.index+1, stages.length-1)].id;
  return { stageId: level.id, stars, unlockedStageId: p.unlocked_stage };
}

export async function totalStars(db, id) {
  const [[row]] = await db.execute('SELECT COALESCE(SUM(best_stars),0) AS total FROM level_progress WHERE player_id=?', [id]);
  return Number(row.total);
}

export async function importProgress(db, p, input) {
  check(!p.imported && p.revision === 0, 409, 'save_already_started');
  const unlocked = stage(input.unlockedStageId);
  check(Array.isArray(input.levels) && input.levels.length <= stages.length, 400, 'invalid_levels');
  const seen = new Set(), rows = [];
  let stars = 0;
  for (const record of input.levels) {
    const level = stage(record?.stageId);
    check(level.index <= unlocked.index && !seen.has(level.id), 400, 'invalid_import_stage');
    const earned = scoreStars(level, record.score);
    rows.push([p.id, level.id, record.score, earned]);
    seen.add(level.id);
    stars += earned;
  }
  if (rows.length) await db.query('INSERT INTO level_progress (player_id,stage_id,best_score,best_stars) VALUES ?', [rows]);
  p.imported = true;
  p.unlocked_stage = unlocked.id;
  // Completed legacy milestones do not mint a second set of cloud rewards.
  p.wallet.starChests = Math.floor(stars/20);
  return { unlockedStageId: unlocked.id, importedLevels: rows.length };
}
