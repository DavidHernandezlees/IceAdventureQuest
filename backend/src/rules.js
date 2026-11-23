import { readFileSync } from 'node:fs';
import { randomInt } from 'node:crypto';
import { check } from './db.js';

const resources = new URL('../../Resources/', import.meta.url);
const paths = readFileSync(new URL('original-index.txt', resources), 'utf8').trim().split(/\s+/);
const firstExtra = paths.findIndex(id => !id.startsWith('level/'));
export const stages = paths.map((id, index) => {
  const scores = JSON.parse(readFileSync(new URL(id, resources), 'utf8')).scores;
  const mapSlot = id.startsWith('level/') ? Number(id.slice(6, -4)) : 3001 + index - firstExtra;
  return { id, index, scores, mapSlot };
});
const stageById = new Map(stages.map(s => [s.id, s]));
export function stage(id) {
  const found = stageById.get(id);
  check(found, 400, 'unknown_stage');
  return found;
}
export function scoreStars(level, score) {
  check(Number.isInteger(score) && score >= 0 && score <= 2147483647, 400, 'invalid_score');
  return level.scores.findLastIndex(threshold => score >= threshold) + 1;
}

const boosts = ['hammer', 'stripe', 'bomb', 'rainbow'];
const prices = { food: 25, soap: 15, hammer: 100, stripe: 100, bomb: 100, rainbow: 100 };
const daily = [['coins',100],['hammer',1],['stripe',1],['coins',300],['bomb',1],['rainbow',1],['coins',500]];
const petUnlocks = [6,31,61,91,121];
const smileGoals = [6,20,30,30,30,40,40,40,50,50,50,50,60,60,60,60,70,70,70,70,70,
  80,80,80,80,80,90,90,90,90,90,100,100,100,110,110,120,130,140,150,160,170,180,190,200,210,
  220,230,240,250,260,270,280,290,300,320,340,360,380,400,420,440,460,480,500,520,540,560,580,
  600,635,670,705,740,775,810,845,880,915,950,985,1020,1055,1090,1125,1160,1195,1230,1265,
  1300,1335,1370,1405,1440,1475,1510,1545,1580,1615,1650,1685,1720,1755,1790,1825,1860,
  1895,1930,1965,2000,2040,2080,2120,2160,2200];
export const products = Object.freeze({
  pay_gems100000: 100000, pay_gems40000: 40000, pay_gems20000: 20000, pay_gems6000: 6000,
  pay_gems2800: 2800, pay_gems500: 500, pay_onetime_1: 500, pay_onetime_2: 2800
});
export function initialWallet() {
  return { coins: 1000, food: 3, soap: 3, hammer: 0, stripe: 0, bomb: 0, rainbow: 0,
    pet: 0, petLevel: 1, smiles: 0, hats: [0,0,0,0,0], bundles: [], lastDailyDay: null,
    streak: 0, lastVideoAt: null, starChests: 0 };
}
function add(w, item, count) { w[item] = Math.min(2147483647, w[item] + count); }

export function applyAction(w, slot, action, input, now, totalStars = 0) {
  switch (action) {
    case 'daily': {
      check(slot >= 5, 409, 'daily_locked');
      const day = Math.floor(now / 86400000);
      check(w.lastDailyDay === null || day > w.lastDailyDay, 409, 'already_claimed_today');
      const i = w.lastDailyDay === day - 1 ? w.streak % 7 : 0;
      const [item, count] = daily[i];
      add(w, item, count);
      w.lastDailyDay = day;
      w.streak = i + 1;
      return { item, count, streak: w.streak };
    }
    case 'buy-supply': {
      const { item } = input;
      check(typeof item === 'string' && Object.hasOwn(prices, item), 400, 'unknown_item');
      check(w.coins >= prices[item] && w[item] < 2147483647, 409, 'cannot_buy');
      w.coins -= prices[item];
      w[item]++;
      return { item, balance: w.coins };
    }
    case 'use-booster': {
      check(boosts.includes(input.item), 400, 'unknown_booster');
      check(w[input.item] > 0, 409, 'out_of_stock');
      return { item: input.item, remaining: --w[input.item] };
    }
    case 'select-pet':
      check(Number.isInteger(input.pet) && input.pet >= 0 && input.pet < petUnlocks.length, 400, 'unknown_pet');
      check(slot >= petUnlocks[input.pet], 409, 'pet_locked');
      w.pet = input.pet;
      return { pet: w.pet };
    case 'care': {
      check(slot >= petUnlocks[w.pet], 409, 'pet_locked');
      const { activity } = input;
      check(['feed','wash','dress'].includes(activity), 400, 'unknown_activity');
      if (activity === 'dress') {
        w.hats[w.pet] = (w.hats[w.pet]+1) % (1 + Math.min(3, w.petLevel));
      } else {
        const item = activity === 'feed' ? 'food' : 'soap';
        check(w[item] > 0, 409, 'out_of_stock');
        w[item]--;
        w.smiles += activity === 'feed' ? 3 : 2;
        const goal = smileGoals[w.petLevel-1];
        if (w.smiles >= goal) {
          w.smiles -= goal;
          w.petLevel = Math.min(115, w.petLevel+1);
        }
      }
      return { pet: w.pet, level: w.petLevel, smiles: w.smiles, hat: w.hats[w.pet] };
    }
    case 'star-chest': {
      check(totalStars >= (w.starChests+1)*20, 409, 'insufficient_stars');
      const count = ++w.starChests % 3 === 0 ? 2 : 1;
      const items = Array.from({ length: count }, () => boosts[randomInt(boosts.length)]);
      for (const item of items) add(w, item, 1);
      return { items, claimed: w.starChests };
    }
    default: throw new Error('Unknown wallet operation');
  }
}

export function grantPurchase(w, product, quantity) {
  check(Object.hasOwn(products, product) && Number.isInteger(quantity) && quantity >= 1 && quantity <= 999,
    400, 'invalid_product');
  if (product.startsWith('pay_onetime')) {
    check(quantity === 1, 400, 'invalid_bundle_quantity');
    if (w.bundles.includes(product)) return { product, coins: 0, alreadyOwned: true };
    w.bundles.push(product);
    for (const item of boosts.slice(0,3)) add(w, item, product === 'pay_onetime_1' ? 1 : 5);
  }
  const coins = products[product]*quantity;
  add(w, 'coins', coins);
  return { product, quantity, coins };
}

export function grantVideo(w, timestamp) {
  if (w.lastVideoAt !== null && timestamp < w.lastVideoAt + 3600000) return { coins: 0, reason: 'cooldown' };
  w.lastVideoAt = timestamp;
  add(w, 'coins', 100);
  return { coins: 100 };
}
