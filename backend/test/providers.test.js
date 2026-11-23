import test from 'node:test';
import assert from 'node:assert/strict';
import { generateKeyPairSync, sign, randomUUID } from 'node:crypto';
import { readPurchase, verifyAd } from '../src/providers.js';

test('AdMob verifies the original query bytes, unit, recipient and duplicate fields', async () => {
  const { privateKey, publicKey } = generateKeyPairSync('ec',{namedCurve:'prime256v1'});
  const player = randomUUID();
  const signed = content => content+'&signature='+sign('sha256',Buffer.from(content),privateKey).toString('base64url')+'&key_id=19';
  const read = s => verifyAd(s,'ca-app-pub-123/456', async () => [{keyId:19,pem:publicKey}]);
  const content = 'ad_unit=456&custom_data=ice%20quest&timestamp='+Date.now()+'&transaction_id=aabbcc&user_id='+player;
  assert.equal((await read(signed(content))).playerId,player);
  await assert.rejects(read(signed(content).replace('%20','+')),/invalid_ad_signature/);
  await assert.rejects(read(signed(content.replace('456','789'))),/wrong_ad_unit/);
  await assert.rejects(read(signed(content+'&ad_unit=456')),/duplicate_callback_field/);
  await assert.rejects(read(content),/invalid_callback/);
});
test('Play binds a purchased product and quantity to the player', () => {
  const offer={quantity:3,refundableQuantity:2,consumptionState:'CONSUMPTION_STATE_YET_TO_BE_CONSUMED'};
  const data={obfuscatedExternalAccountId:'account',purchaseStateContext:{purchaseState:'PURCHASED'},
    productLineItem:[{productId:'pay_gems500',productOfferDetails:offer}]};
  assert.deepEqual(readPurchase(data,'account'),{product:'pay_gems500',quantity:2});
  assert.throws(()=>readPurchase(data,'different'),/owner_mismatch/);
  assert.throws(()=>readPurchase({...data,purchaseStateContext:{purchaseState:'PENDING'}},'account'),/not_completed/);
  offer.refundableQuantity=0; assert.throws(()=>readPurchase(data,'account'),/quantity/);
  offer.refundableQuantity=2; offer.consumptionState='CONSUMPTION_STATE_CONSUMED';
  assert.throws(()=>readPurchase(data,'account'),/already_consumed/);
});
