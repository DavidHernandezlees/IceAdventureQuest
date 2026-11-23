package com.iceadventurequest.game;

import android.content.SharedPreferences;
import android.os.Bundle;
import android.util.Base64;
import com.android.billingclient.api.*;
import com.google.android.gms.ads.*;
import com.google.android.gms.ads.rewarded.*;
import com.google.android.ump.*;
import org.cocos2dx.lib.Cocos2dxActivity;
import java.security.KeyFactory;
import java.security.Signature;
import java.security.spec.X509EncodedKeySpec;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;

public class GameActivity extends Cocos2dxActivity {
    private static final String[] IDS = {"pay_gems100000","pay_gems40000","pay_gems20000","pay_gems6000",
            "pay_gems2800","pay_gems500","pay_onetime_1","pay_onetime_2"};
    private static volatile GameActivity active;
    private final Map<Integer, ProductDetails> catalog = new ConcurrentHashMap<>();
    private BillingClient billing;
    private boolean videoBusy, storeOpened, connecting;
    private int videoRequest;
    private volatile boolean privacyRequired;
    private SharedPreferences journal;

    private static native void serviceStatus(int code);
    private static native boolean deliverPurchase(int index, String token, int count);
    private static native boolean deliverVideo(long earnedAt);

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        journal = getSharedPreferences("reward-delivery", MODE_PRIVATE);
        active = this;
    }
    private boolean alive() { return active == this && !isFinishing() && !isDestroyed(); }
    private void status(int code) {
        if (alive()) runOnGLThread(() -> { if (alive()) serviceStatus(code); });
    }
    @Override protected void onResume() {
        super.onResume();
        if (storeOpened) recover();
    }
    @Override protected void onDestroy() {
        if (active == this) active = null;
        if (billing != null) billing.endConnection();
        super.onDestroy();
    }
    public static void refreshStore() {
        GameActivity a = active;
        if (a != null) a.runOnUiThread(() -> {
            if (!a.alive()) return;
            a.storeOpened = true;
            a.recover();
        });
    }
    private void recover() {
        long earned = journal.getLong("video", 0);
        if (earned != 0) sendVideo(earned);
        if (BuildConfig.BILLING_KEY.isEmpty()) return;
        if (billing == null) {
            billing = BillingClient.newBuilder(this).setListener((result, orders) -> {
                if (result.getResponseCode() == BillingClient.BillingResponseCode.OK && orders != null) {
                    for (Purchase order : orders) deliver(order);
                } else status(result.getResponseCode() == BillingClient.BillingResponseCode.USER_CANCELED ? 3 : 2);
            }).enablePendingPurchases(PendingPurchasesParams.newBuilder().enableOneTimeProducts().build())
                    .enableAutoServiceReconnection().build();
        }
        if (billing.isReady()) { query(); return; }
        if (connecting) return;
        connecting = true;
        billing.startConnection(new BillingClientStateListener() {
            public void onBillingSetupFinished(BillingResult result) {
                connecting = false;
                if (alive() && result.getResponseCode() == BillingClient.BillingResponseCode.OK) query();
            }
            public void onBillingServiceDisconnected() { connecting = false; catalog.clear(); }
        });
    }
    private void query() {
        List<QueryProductDetailsParams.Product> products = new ArrayList<>();
        for (String id : IDS) products.add(QueryProductDetailsParams.Product.newBuilder().setProductId(id)
                .setProductType(BillingClient.ProductType.INAPP).build());
        billing.queryProductDetailsAsync(QueryProductDetailsParams.newBuilder().setProductList(products).build(),
                (result, response) -> {
                    if (!alive()) return;
                    catalog.clear();
                    if (result.getResponseCode() != BillingClient.BillingResponseCode.OK) return;
                    for (ProductDetails p : response.getProductDetailsList()) {
                        int i = Arrays.asList(IDS).indexOf(p.getProductId());
                        if (i >= 0 && p.getOneTimePurchaseOfferDetails() != null) catalog.put(i, p);
                    }
                });
        billing.queryPurchasesAsync(QueryPurchasesParams.newBuilder().setProductType(BillingClient.ProductType.INAPP).build(),
                (result, orders) -> {
                    if (alive() && result.getResponseCode() == BillingClient.BillingResponseCode.OK)
                        for (Purchase order : orders) deliver(order);
                });
    }
    public static String price(int index) {
        GameActivity a = active;
        ProductDetails p = a == null ? null : a.catalog.get(index);
        return p == null ? "" : p.getOneTimePurchaseOfferDetails().getFormattedPrice();
    }
    public static boolean buy(int index) {
        GameActivity a = active;
        if (a == null || !a.catalog.containsKey(index)) return false;
        a.runOnUiThread(() -> {
            ProductDetails p = a.catalog.get(index);
            if (!a.alive() || p == null || !a.billing.isReady()) { a.status(2); return; }
            BillingFlowParams.ProductDetailsParams.Builder detail =
                    BillingFlowParams.ProductDetailsParams.newBuilder().setProductDetails(p);
            String offer = p.getOneTimePurchaseOfferDetails().getOfferToken();
            if (offer != null && !offer.isEmpty()) detail.setOfferToken(offer);
            BillingResult result = a.billing.launchBillingFlow(a, BillingFlowParams.newBuilder()
                    .setProductDetailsParamsList(Collections.singletonList(detail.build())).build());
            if (result.getResponseCode() != BillingClient.BillingResponseCode.OK) { a.status(2); a.recover(); }
        });
        return true;
    }
    private boolean signed(Purchase order) {
        try {
            Signature verifier = Signature.getInstance("SHA1withRSA");
            verifier.initVerify(KeyFactory.getInstance("RSA").generatePublic(
                    new X509EncodedKeySpec(Base64.decode(BuildConfig.BILLING_KEY, Base64.DEFAULT))));
            verifier.update(order.getOriginalJson().getBytes(StandardCharsets.UTF_8));
            return verifier.verify(Base64.decode(order.getSignature(), Base64.DEFAULT));
        } catch (Exception ex) { return false; }
    }
    private void deliver(Purchase p) {
        if (p.getPurchaseState() == Purchase.PurchaseState.PENDING) { status(5); return; }
        if (p.getPurchaseState() != Purchase.PurchaseState.PURCHASED || !getPackageName().equals(p.getPackageName())
                || p.getProducts().size() != 1 || !signed(p)) { status(2); return; }
        int i = Arrays.asList(IDS).indexOf(p.getProducts().get(0));
        if (i < 0) return;
        // Nonconsumable bundles can be restored on another install even if already acknowledged.
        runOnGLThread(() -> {
            if (!alive()) return;
            boolean saved = deliverPurchase(i, p.getPurchaseToken(), p.getQuantity());
            if (!saved) { serviceStatus(6); return; }
            runOnUiThread(() -> {
                if (!alive() || !billing.isReady()) return;
                if (i < 6) {
                    billing.consumeAsync(ConsumeParams.newBuilder().setPurchaseToken(p.getPurchaseToken()).build(),
                            (result, token) -> status(result.getResponseCode()==BillingClient.BillingResponseCode.OK ? 4 : 2));
                } else if (!p.isAcknowledged()) {
                    billing.acknowledgePurchase(AcknowledgePurchaseParams.newBuilder().setPurchaseToken(p.getPurchaseToken()).build(),
                            result -> status(result.getResponseCode()==BillingClient.BillingResponseCode.OK ? 4 : 2));
                }
            });
        });
    }
    public static boolean watch() {
        GameActivity a = active;
        if (a == null || BuildConfig.VIDEO_UNIT.isEmpty()) return false;
        a.runOnUiThread(() -> {
            if (!a.alive() || a.videoBusy) return;
            if (a.journal.getLong("video",0) != 0) { a.recover(); return; }
            a.videoBusy = true;
            a.status(1);
            int request = ++a.videoRequest;
            if (BuildConfig.DEBUG) a.initializeAds(request);
            else a.requestConsent(request);
        });
        return true;
    }
    public static void leaveVideo() {
        GameActivity a = active;
        if (a != null) a.runOnUiThread(() -> { ++a.videoRequest; a.videoBusy = false; });
    }
    public static boolean privacyRequired() {
        GameActivity a = active;
        return a != null && a.privacyRequired;
    }
    public static void privacyOptions() {
        GameActivity a = active;
        if (a != null) a.runOnUiThread(() -> {
            if (a.alive()) UserMessagingPlatform.showPrivacyOptionsForm(a, error -> { if (error != null) a.status(2); });
        });
    }
    private void requestConsent(int request) {
        ConsentInformation info = UserMessagingPlatform.getConsentInformation(this);
        Runnable after = () -> {
            privacyRequired = info.getPrivacyOptionsRequirementStatus() == ConsentInformation.PrivacyOptionsRequirementStatus.REQUIRED;
            if (!alive() || videoRequest != request) return;
            if (info.canRequestAds()) initializeAds(request);
            else { videoBusy = false; status(2); }
        };
        info.requestConsentInfoUpdate(this, new ConsentRequestParameters.Builder().build(),
                () -> { if (alive() && videoRequest == request) UserMessagingPlatform.loadAndShowConsentFormIfRequired(this, error -> after.run()); },
                error -> after.run());
    }
    private void initializeAds(int request) {
        MobileAds.initialize(this, initialization -> runOnUiThread(() -> loadVideo(request)));
    }
    private void loadVideo(int request) {
        if (!alive() || videoRequest != request) return;
        RewardedAd.load(this, BuildConfig.VIDEO_UNIT, new AdRequest.Builder().build(), new RewardedAdLoadCallback() {
            @Override public void onAdFailedToLoad(LoadAdError error) { if (videoRequest == request) { videoBusy=false; status(2); } }
            @Override public void onAdLoaded(RewardedAd ad) {
                if (videoRequest != request) return;
                if (!alive() || !hasWindowFocus()) { videoBusy=false; status(3); return; }
                final boolean[] earned = {false};
                ad.setFullScreenContentCallback(new FullScreenContentCallback() {
                    @Override public void onAdFailedToShowFullScreenContent(AdError error) { videoBusy=false; status(2); }
                    @Override public void onAdDismissedFullScreenContent() { videoBusy=false; if (!earned[0]) status(3); }
                });
                ad.show(GameActivity.this, ignored -> {
                    earned[0] = true;
                    long now = System.currentTimeMillis()/1000;
                    if (journal.edit().putLong("video",now).commit()) sendVideo(now);
                    else status(6);
                });
            }
        });
    }
    private void sendVideo(long earnedAt) {
        runOnGLThread(() -> {
            if (!alive()) return;
            if (!deliverVideo(earnedAt)) { serviceStatus(6); return; }
            if (journal.getLong("video",0) == earnedAt) journal.edit().remove("video").commit();
            serviceStatus(4);
        });
    }
}
