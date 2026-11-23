# Ice Adventure Quest game service

The Node.js API runs on Express 5 and MySQL 8.4 LTS. It stores player accounts, cloud progress and the game economy. Run it from the repository checkout so it can read the same stage index and level files as the Cocos2d game. The existing Android build uses local saves. Its integration points are described below.

## Run on Windows

Install Node.js 22.12 or later and MySQL 8.4. From the MySQL command-line client or MySQL Workbench, create a database and a development user. Replace `change_me` in both this SQL and the configuration file.

```sql
CREATE DATABASE ice_quest CHARACTER SET utf8mb4;
CREATE USER 'ice_game'@'127.0.0.1' IDENTIFIED BY 'change_me';
GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, REFERENCES ON ice_quest.* TO 'ice_game'@'127.0.0.1';
```

From the IceAdventureQuest project folder in Windows CMD, run these commands.

```cmd
cd backend
copy .env.example .env
notepad .env
npm ci
npm run migrate
npm start
```

Check the server and register a test player in a second CMD window.

```cmd
curl.exe http://127.0.0.1:3000/v1/health
curl.exe -X POST http://127.0.0.1:3000/v1/auth/register -H "Content-Type: application/json" -d "{\"username\":\"snowplayer\",\"password\":\"my-long-test-passphrase\"}"
```

Copy the returned token into the next command in your own terminal.

```cmd
set "TOKEN=your-returned-token"
curl.exe http://127.0.0.1:3000/v1/me -H "Authorization: Bearer %TOKEN%"
curl.exe -X POST http://127.0.0.1:3000/v1/progress/complete -H "Authorization: Bearer %TOKEN%" -H "Content-Type: application/json" -H "Idempotency-Key: first-win-001" -d "{\"stageId\":\"level/1.txt\",\"score\":40000}"
```

The example win earns three stars and opens level 2. Usernames are case-insensitive and contain 3–32 ASCII letters, numbers or underscores. Passwords have a 12-character minimum and a 256-byte maximum. Players use their username and password to sign in on another device. Sessions expire after 30 days and can be revoked with logout.

## API

Paths are relative to `/v1`. Send the bearer token on all player routes. Registration, login, health and the signed AdMob callback are public.

| Method and path | Input or behavior |
| --- | --- |
| `GET /health` | Checks the MySQL connection |
| `POST /auth/register`, `POST /auth/login` | `{"username":"snowplayer","password":"…"}` returns a player ID and token |
| `DELETE /auth/session` | Revokes the current token |
| `GET /me` | Returns revision, stable unlocked stage ID, scores, stars, inventory, pet state and reward timestamps |
| `POST /progress/complete` | `{"stageId":"level/1.txt","score":40000}` records a client-reported win on an unlocked stage. The server calculates stars and retains the best result. |
| `POST /progress/import` | Imports a legacy save on a fresh account when enabled |
| `POST /inventory/buy` | `{"item":"food"}`. Available items are food, soap, hammer, stripe, bomb and rainbow. |
| `POST /inventory/use` | `{"item":"hammer"}` spends one owned booster |
| `POST /pets/select` | `{"pet":0}` selects an unlocked penguin, indexed from 0 to 4 |
| `POST /pets/care` | `{"activity":"feed"}`. Activities are feed, wash and dress. |
| `POST /rewards/daily` | `{}` claims the next daily prize starting at map level 5. Daily reset is midnight UTC. |
| `POST /rewards/star-chest` | `{}` claims the next 20-star milestone |
| `POST /purchases/google-play` | `{"purchaseToken":"…"}` verifies the order and returns the Android finalization action |
| `GET /callbacks/admob` | Receives the original signed query directly from Google |

Every progress, inventory, pet or daily/chest command requires an `Idempotency-Key` header. Generate a UUID per player action and keep it until the response arrives. Retrying the same action and key returns its original result and revision. A reused key with different input returns HTTP 409. After reconnecting, fetch `GET /me` for the latest save. Purchases and ads use the provider transaction identifier for deduplication.

The player row, progress, wallet changes and request receipt commit together. Successful events retain the before/after values of changed wallet fields. The server sets the starting balance, prices, grants, pet unlocks and cooldowns. Match play runs on the client. The completion endpoint checks stage order and derives stars from score. A competitive leaderboard would also need independent win and score verification.

## Connect the Android client

- Use HTTPS for the public API. The development server binds to loopback by default. Use Android emulator address `10.0.2.2:3000` for local development, or use `adb reverse tcp:3000 tcp:3000` on a connected device. Android may require a debug-only cleartext allowance for local HTTP testing.
- At sign-in, fetch `GET /me`. Store the session token in platform-protected storage. When `gameUI::saveWin` records a victory, submit the catalog path from `Resources/original-index.txt` and the final score. Retain pending wins in order if the device goes offline.
- For an online player, send wallet and pet commands through the API and update the UI from the committed save. Make the server the sole writer of online rewards. The existing offline grant callbacks must be switched over when integrating.
- Legacy import is opt-in because local save files can be edited. Set `ALLOW_LEGACY_IMPORT=true` for a migration period. Send, for example, `{"unlockedStageId":"level/2.txt","levels":[{"stageId":"level/1.txt","score":40000}]}` before the account's first mutation. It imports stage progress once. Online balances start at the normal defaults, and previously reached star milestones are marked claimed. Disable the import when migration ends.

## Google Play and AdMob

For Google Play, set `GOOGLE_PLAY_PACKAGE` to the app's actual package and supply Application Default Credentials for a service account with Android Publisher access for that app. Keep credential files outside the repository. Set `BillingFlowParams.setObfuscatedAccountId` to the signed-in player's opaque UUID when starting checkout. The server checks the purchase state, matching account, SKU and available quantity. After the server returns success, Android must consume a coin pack or acknowledge a permanent bundle according to the returned `finalize` field. Retry unfinished deliveries when billing reconnects. Existing unbound offline purchases require a separate reconciliation policy. Refund or chargeback reconciliation after fulfillment is outside this release.

For AdMob, set `ADMOB_AD_UNIT_ID` to the rewarded unit, register the public HTTPS callback URL ending in `/v1/callbacks/admob`, and pass the player UUID as the SSV user ID on the rewarded ad. The backend checks Google's ECDSA signature on the original query, the configured unit, timestamp, player and transaction ID. Each eligible verified event grants 100 coins. The one-hour cooldown uses Google's signed event time. Refresh the wallet after a callback arrives. A device-side ad completion alone does not authorize coins.

Provider configuration is optional for local development. Unconfigured provider endpoints return HTTP 503. Keep MySQL private, configure TLS with `MYSQL_SSL_CA_FILE` for a remote database, and terminate public HTTPS at your reverse proxy. Set `TRUSTED_PROXY_IPS` to the exact addresses of proxies that overwrite incoming forwarded headers, so login limits apply per player IP. The example is intended for a single API instance. For multiple replicas, use a shared login rate-limit store.

Google documentation for these flows is available here. [Play purchase verification](https://developer.android.com/google/play/billing/security) · [ProductPurchaseV2](https://developers.google.com/android-publisher/api-ref/rest/v3/purchases.productsv2) · [AdMob SSV](https://developers.google.com/admob/android/ssv).

## Tests

`npm test` checks the campaign and economy rules and exercises AdMob signatures and Google Play response validation. To run the HTTP and database integration test, create a separate MySQL database whose name ends in `_test`, give the test user schema permissions there, put its URL in `TEST_MYSQL_URL`, and run `npm run test:integration`. The test starts an ephemeral Express listener, creates its own test players and checks concurrent commands, account isolation, purchases, ad rewards, legacy import and login from a second session. Provider calls in the integration test use fixtures.
