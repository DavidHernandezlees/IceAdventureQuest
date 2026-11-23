import test from 'node:test';
import assert from 'node:assert/strict';
import { randomUUID } from 'node:crypto';
import { ApiError, createPool, migrate } from '../src/db.js';
import { createApp } from '../src/app.js';

test('Express and MySQL persist independent player saves, grants and retries', async t => {
  const url=process.env.TEST_MYSQL_URL;
  assert.ok(url && new URL(url).pathname.endsWith('_test'), 'Set TEST_MYSQL_URL to a disposable database ending in _test');
  const pool=createPool(url);
  t.after(()=>pool.end());
  await migrate(pool); await migrate(pool);
  const account='test_'+randomUUID().slice(0,8);
  const second=account+'_b';
  t.after(async () => {
    // Cleanup uses its own connection so it runs independently of pool teardown order.
    const cleanup=createPool(url);
    try { await cleanup.execute('DELETE FROM players WHERE username IN (?,?)',[account,second]); }
    finally { await cleanup.end(); }
  });
  let buyer, verifyCount=0;
  const clock=Date.UTC(2026,0,10);
  const services={
    async purchase(token,id) {
      assert.equal(token,'store-'+account);
      if (id !== buyer) throw new ApiError(403,'purchase_owner_mismatch');
      verifyCount++;
      return {product:'pay_gems500',quantity:2};
    },
    async ad() { return {playerId:buyer,reference:account.replace('test_',''),time:clock}; }
  };
  const app=createApp(pool,{services,allowImport:true,now:()=>clock});
  const server=await new Promise(resolve=>{const s=app.listen(0,'127.0.0.1',()=>resolve(s));});
  t.after(()=>new Promise(resolve=>server.close(resolve)));
  const base='http://127.0.0.1:'+server.address().port+'/v1';
  async function http(method,path,body,token,key) {
    const headers={'content-type':'application/json'};
    if(token)headers.authorization='Bearer '+token;
    if(key)headers['idempotency-key']=key;
    const res=await fetch(base+path,{method,headers,body:body===undefined?undefined:JSON.stringify(body)});
    return [res.status,res.status===204?{}:await res.json()];
  }
  const pass='long-test-passphrase';
  assert.equal((await http('GET','/me'))[0],401);
  const [status,a]=await http('POST','/auth/register',{username:account,password:pass});
  assert.equal(status,201);buyer=a.playerId;
  assert.equal((await http('POST','/auth/register',{username:account,password:pass}))[0],409);
  const [,b]=await http('POST','/auth/register',{username:second,password:pass});
  const post=(path,body={},key=randomUUID())=>http('POST',path,body,a.token,key);
  const state=async token=>(await http('GET','/me',undefined,token))[1];
  assert.equal((await state(a.token)).wallet.coins,1000);
  assert.equal((await post('/progress/complete',{stageId:'level/2.txt',score:99}))[0],409);
  assert.equal((await post('/rewards/daily'))[0],409);
  for(let n=1;n<=7;n++) assert.equal((await post('/progress/complete',{stageId:'level/'+n+'.txt',score:900000}))[0],200);
  await post('/progress/complete',{stageId:'level/1.txt',score:100});
  const after=await state(a.token);
  assert.equal(after.unlockedStageId,'level/8.txt');assert.equal(after.levels.find(row=>row.stageId==='level/1.txt').score,900000);
  const [,device2]=await http('POST','/auth/login',{username:account,password:pass});
  const claims=await Promise.all([post('/rewards/daily'),http('POST','/rewards/daily',{},device2.token,randomUUID())]);
  assert.deepEqual(claims.map(r=>r[0]).sort(),[200,409]);
  const sameKey=randomUUID();
  const buys=await Promise.all([post('/inventory/buy',{item:'hammer'},sameKey),post('/inventory/buy',{item:'hammer'},sameKey)]);
  assert.deepEqual(buys[0],buys[1]);assert.equal(buys[0][0],200);
  assert.equal((await post('/inventory/buy',{item:'soap'},sameKey))[0],409);
  assert.equal((await state(a.token)).wallet.coins,1000);
  assert.equal((await post('/pets/care',{activity:'feed'}))[0],200);
  assert.equal((await post('/pets/select',{pet:1}))[0],409);
  assert.equal((await post('/rewards/star-chest'))[0],200);
  assert.equal((await post('/rewards/star-chest'))[0],409);
  const receipt={purchaseToken:'store-'+account};
  const paid=await post('/purchases/google-play',receipt);
  assert.equal(paid[1].finalize,'consume');assert.deepEqual(await post('/purchases/google-play',receipt),paid);
  assert.equal(verifyCount,1);
  assert.equal((await http('POST','/purchases/google-play',receipt,b.token))[0],403);
  assert.equal((await state(b.token)).wallet.coins,1000);
  const videos=await Promise.all([http('GET','/callbacks/admob'),http('GET','/callbacks/admob')]);
  assert.deepEqual(videos[0],videos[1]);assert.equal((await state(a.token)).wallet.coins,2100);
  const oldSave={unlockedStageId:'level/8.txt',levels:after.levels};
  assert.equal((await post('/progress/import',oldSave))[0],409);
  assert.equal((await http('POST','/progress/import',oldSave,b.token,randomUUID()))[0],200);
  assert.equal((await state(b.token)).wallet.coins,1000);
  assert.equal((await http('POST','/rewards/star-chest',{},b.token,randomUUID()))[0],409);
  const [,login]=await http('POST','/auth/login',{username:account,password:pass});
  assert.equal((await state(login.token)).wallet.coins,2100);
  assert.equal((await http('DELETE','/auth/session',undefined,a.token))[0],204);
  assert.equal((await http('GET','/me',undefined,a.token))[0],401);
  assert.equal((await state(login.token)).unlockedStageId,'level/8.txt');
});
