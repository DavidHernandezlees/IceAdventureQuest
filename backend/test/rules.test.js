import test from 'node:test';
import assert from 'node:assert/strict';
import { stages, stage, initialWallet, scoreStars, applyAction, grantPurchase, grantVideo } from '../src/rules.js';

test('campaign follows the bundled index, score boundaries and map slots', () => {
  assert.equal(stages.length, new Set(stages.map(s => s.id)).size);
  assert.equal(stages[0].id,'level/1.txt');
  const extra = stages.find(s => !s.id.startsWith('level/'));
  assert.equal(extra.mapSlot,3001);
  for (const s of stages) {
    assert.equal(stage(s.id),s);
    for (const n of s.scores) assert.equal(scoreStars(s,n),s.scores.findLastIndex(x => n>=x)+1);
  }
  assert.throws(() => stage('../secrets'));
  assert.throws(() => scoreStars(stages[0],-1));
});
test('seven UTC daily rewards, missed days and level gates', () => {
  const w=initialWallet(), day=86400000;
  const claim = i => applyAction(w,5,'daily',{},i*day);
  assert.throws(() => applyAction(w,4,'daily',{},day));
  for (let i=1;i<=7;i++) {
    assert.equal(claim(i).streak,i);
    assert.throws(()=>claim(i));
  }
  assert.equal(w.coins,1900);
  assert.equal(claim(8).streak,1);
  assert.equal(claim(11).streak,1);
});
test('pets, consumables, stars, bundles and video follow wallet rules', () => {
  const w=initialWallet(), act=(action,input={})=>applyAction(w,31,action,input,0,60);
  act('select-pet',{pet:1});
  assert.throws(()=>act('select-pet',{pet:2}));
  act('care',{activity:'feed'}); act('care',{activity:'feed'}); act('care',{activity:'dress'});
  assert.equal(w.petLevel,2); assert.equal(w.hats[1],1); assert.equal(w.food,1);
  const before=w.coins;
  act('buy-supply',{item:'hammer'}); act('use-booster',{item:'hammer'});
  assert.equal(w.coins,before-100); assert.equal(w.hammer,0); assert.throws(()=>act('use-booster',{item:'hammer'}));
  assert.deepEqual([act('star-chest').items.length,act('star-chest').items.length,act('star-chest').items.length],[1,1,2]);
  assert.throws(()=>act('star-chest')); assert.throws(()=>act('buy-supply',{item:'toString'}));
  assert.equal(grantPurchase(w,'pay_onetime_2',1).coins,2800);
  assert.equal(grantPurchase(w,'pay_onetime_2',1).coins,0);
  assert.equal(grantVideo(w,1000).coins,100);assert.equal(grantVideo(w,3600999).coins,0);
  assert.equal(grantVideo(w,3601000).coins,100);
});
