import { verify } from 'node:crypto';
import { GoogleAuth } from 'google-auth-library';
import { ApiError, check } from './db.js';
import { products } from './rules.js';

export function readPurchase(data, playerId) {
  check(data?.purchaseStateContext?.purchaseState === 'PURCHASED', 409, 'purchase_not_completed');
  check(data.obfuscatedExternalAccountId === playerId, 403, 'purchase_owner_mismatch');
  check(data.productLineItem?.length === 1, 422, 'unsupported_purchase');
  const { productId: product, productOfferDetails: offer } = data.productLineItem[0];
  check(typeof product === 'string' && Object.hasOwn(products, product), 422, 'unknown_product');
  const quantity = offer?.refundableQuantity ?? offer?.quantity ?? 1;
  check(Number.isInteger(quantity) && quantity >= 1 && quantity <= 999, 409, 'invalid_purchase_quantity');
  check(offer?.consumptionState !== 'CONSUMPTION_STATE_CONSUMED', 409, 'purchase_already_consumed');
  return { product, quantity };
}

export async function verifyAd(query, unit, getKeys) {
  check(unit, 503, 'admob_unconfigured');
  check(typeof query === 'string' && query.length < 8192, 400, 'invalid_callback');
  const match = /^(.+)&signature=([^&]+)&key_id=(\d+)$/.exec(query);
  check(match, 400, 'invalid_callback');
  const values = new URLSearchParams(query);
  check([...values.keys()].length === new Set(values.keys()).size, 400, 'duplicate_callback_field');
  const keys = await getKeys();
  const key = keys.find(k => String(k.keyId) === match[3]);
  const signature = values.get('signature');
  check(key && /^[\w-]+={0,2}$/.test(signature) &&
    verify('sha256', Buffer.from(match[1]), key.pem, Buffer.from(signature, 'base64url')), 403, 'invalid_ad_signature');
  const playerId = values.get('user_id'), reference = values.get('transaction_id');
  const time = Number(values.get('timestamp'));
  check(values.get('ad_unit') === unit.slice(unit.lastIndexOf('/')+1), 403, 'wrong_ad_unit');
  check(/^[a-f0-9]{8}-[a-f0-9]{4}-4[a-f0-9]{3}-[89ab][a-f0-9]{3}-[a-f0-9]{12}$/.test(playerId || '') &&
    /^[a-fA-F0-9]{1,128}$/.test(reference || '') && Number.isSafeInteger(time) && time > 0 && time <= Date.now()+300000,
    400, 'invalid_ad_event');
  return { playerId, reference, time };
}

export function providers(env = process.env) {
  const auth = new GoogleAuth({ scopes: ['https://www.googleapis.com/auth/androidpublisher'] });
  let cached = [], validUntil = 0, fetching;
  async function loadKeys() {
    try {
      const res = await fetch('https://www.gstatic.com/admob/reward/verifier-keys.json', { signal: AbortSignal.timeout(8000) });
      if (!res.ok) throw new Error();
      const { keys } = await res.json();
      if (!Array.isArray(keys) || !keys.length) throw new Error();
      cached = keys;
      validUntil = Date.now()+3600000;
      return cached;
    } catch { throw new ApiError(503, 'admob_unavailable'); }
    finally { fetching = undefined; }
  }
  return {
    ad: query => verifyAd(query, env.ADMOB_AD_UNIT_ID, async () => {
      if (Date.now() < validUntil) return cached;
      fetching ??= loadKeys();
      return fetching;
    }),
    async purchase(token, playerId) {
      check(env.GOOGLE_PLAY_PACKAGE, 503, 'google_play_unconfigured');
      let data;
      try {
        const client = await auth.getClient();
        const url = 'https://androidpublisher.googleapis.com/androidpublisher/v3/applications/' +
          encodeURIComponent(env.GOOGLE_PLAY_PACKAGE) + '/purchases/productsv2/tokens/' + encodeURIComponent(token);
        ({ data } = await client.request({ url, timeout: 8000 }));
      } catch (err) {
        if ([400,404,410].includes(err.response?.status)) throw new ApiError(422, 'invalid_purchase_token');
        throw new ApiError(503, 'google_play_unavailable');
      }
      return readPurchase(data, playerId);
    }
  };
}
