# Ice Adventure Quest

An editable Cocos2d-x 3.17.2 reconstruction of Ice Adventure Quest for Android and Windows. The penguin loading screen opens the animated title. Press **Play** to begin level 1. After your first win, Play opens the scrolling level map. Progress, stars and best scores are stored under the existing original-campaign keys.

## Screens and backgrounds

| Screen | Source from the supplied Android v2.3 game |
| --- | --- |
| Loading | `Resources/dialog/SplashScene.csb` uses `images/background/loading.jpg` and its own progress bar. |
| Title | `Classes/MainMenuScene.cpp` plays the Spine cover in `Resources/spine/fengmian_logo.*`. The Play button comes from `Resources/dialog/FirstScene_0.csb`. |
| Map | `Resources/dialog/LevelSelectScene.csb` supplies the base, four scrolling sections and their level-marker positions. |
| Board | `Classes/GameScene.cpp` loads `Resources/dialog/GameScene.csb`. `gameUI::boardBackground` selects the backdrop using the source level number. |
| Tutorial and results | The hand, instructions, pause, win and loss panels use the corresponding Cocos Studio files under `Resources/dialog/`. |

## Play the campaign

Swipe between adjacent gems or tap two neighboring gems. Level 1 opens with the authored 7 × 7 board, 18 moves and a goal of 20 blue pieces. The hand demonstrates the first swap. Follow the gesture or skip the instruction. The three star thresholds are 20,000, 30,000 and 40,000 points.

A valid swap can set off several matches. Four in a line produces a striped gem. A T or L produces a bomb, and five in a line produces a rainbow. Swapping adjacent special pieces combines their effects. Later boards introduce portals, boxes, jars, bears, ice and cages, covered items, floors to paint, and pieces to deliver to marked exits. Goal counters update as pieces are collected. The result is decided once the board settles, so a final-move cascade can win.

The source archive includes all 3,380 numbered definitions from the supplied resources. The trail map unlocks the next **enabled** definition and displays its original number. Main levels 1–82 are playable in sequence. Level 39 introduces four flying planes. When a plane fires, it hits its neighbors and flies to a target.

| Original folder | Definitions included | Enabled in the map |
| --- | ---: | ---: |
| `Resources/level` | 3,000 | 945 |
| `Resources/level2` | 80 | 1 |
| `Resources/level3` | 300 | 5 |
| **Total** | **3,380** | **951** |

The enabled levels use the supplied layouts, goals, move limits, color weights and score thresholds. The 2,429 other definitions need additional piece behavior, secondary layers, directional or conveyor cells, or linked boards. The reference for the recovered rules was the supplied Android game version 2.3. The goal classifier, ordered collection, result ordering and star comparisons come from its compiled rules. Match combinations, plane target selection and combinations, refill, shuffle, obstacle damage and some score awards are reconstructed.

## Map rewards, shop and pets

Open the map from the pause menu or from the title after clearing level 1. The map keeps your scroll position when you close a feature.

| Button | What you can do |
| --- | --- |
| Daily bonus | Starting at level 5, claim once per local calendar day. The seven days give 100 coins, a hammer, a stripe, 300 coins, a bomb, a rainbow and 500 coins. Missing a day restarts the sequence. |
| Watch Video | On Android, finish a rewarded ad to receive 100 coins. The next reward becomes available one hour later. |
| Shop | Buy food, soap and board boosters with earned coins under **Supplies**. The two one-time Play bundles include gold plus hammers, stripes and bombs. The coin counter’s **+** opens the six coin packs. |
| Pets | Open the pet room at level 6. The other four penguins open at map levels 31, 61, 91 and 121. Feed and wash them to earn smiles. Leveling up unlocks additional headwear. Tap the dress button to cycle accessories. |

A new wallet starts with 1,000 coins. To use a board booster, tap its inventory icon and then an ordinary gem after the tutorial. The four tools clear a gem, a row, a bomb area or a color. Pet supplies, the smile meter and equipped headwear are saved between sessions. Supply prices and pet-care gains are reconstruction choices.

### Android store configuration

Debug builds use Google’s rewarded test ad. For a release, set `admobAppId` and `rewardedAdUnitId` in your local Gradle properties and configure privacy messages in AdMob. The video screen offers privacy options where required. The ad reward is delivered by the SDK’s earned-reward callback.

To test paid items on Google Play, configure the app package `com.iceadventurequest.frosttrail`, copy its base64 licensing public key into the Gradle property `playBillingPublicKey`, and activate these one-time products in Play Console. Provide a backwards-compatible **Buy** option for each. Prices come from Play in the player’s currency.

| Play products | Contents |
| --- | --- |
| `pay_gems500`, `pay_gems2800`, `pay_gems6000`, `pay_gems20000`, `pay_gems40000`, `pay_gems100000` | Repeatable coin packs in the amounts shown in their IDs |
| `pay_onetime_1` | 500 coins and one of each of the first three boosters |
| `pay_onetime_2` | 2,800 coins and five of each of the first three boosters |

This implementation verifies Play signatures locally and records the purchase token with its reward before acknowledging or consuming it. A publisher backend can add server-side verification and cross-device wallet sync. The coin-priced supplies shop works offline on Android and Windows.

## Edit and build for Android on Windows 11

1. Extract the ZIP to a short path, for example `C:\src\IceAdventureQuest`. Open the folder containing `settings.gradle` in **Android Studio**. You can edit the same folder in VS Code.
2. Install a 64-bit **JDK 21** and select it as the Gradle JDK in Android Studio. For the terminal scripts, set `JAVA_HOME` to the JDK folder or put its `bin` directory on PATH.
3. From a PowerShell terminal in the project folder, run `.\setup-android.cmd`. It locates or installs the Android SDK command-line tools and requests the components below. Accept the SDK licenses when prompted. The first setup and dependency download need an internet connection.
4. Connect an unlocked Android phone with USB debugging enabled, and authorize the computer on the phone. Click **Run** with the **app** configuration in Android Studio, or run `.\run-android.cmd` in the terminal. The script chooses the connected device's ABI, builds, installs and starts the game.

| Component | Version used |
| --- | --- |
| Cocos2d-x | 3.17.2, engine source and platform libraries bundled |
| Gradle wrapper / Android Gradle Plugin | 8.9 / 8.7.3 |
| Android compile SDK / Build Tools | 35 / 34.0.0 |
| Android NDK / CMake | 19.2.5345600 (r19c) / 3.22.1 |
| Android minimum / target API | 23 / 28 |

Build a multi-ABI APK without a connected device with this command.

```powershell
.\gradlew.bat :app:assembleDebug -PresourcePreview
```

The result is `proj.android\app\build\outputs\apk\debug\app-debug.apk`. Add `-PgameAbi=arm64-v8a` for a smaller ARM64-only build. For an emulator, select a system image that advertises ARM64, ARMv7 or 32-bit x86. A 64-bit x86-only image needs different native libraries. For multiple connected devices, set `ANDROID_SERIAL` to the desired serial before running the script.

## Build the Windows desktop version

Install **Visual Studio 2022** with **Desktop development with C++**, MSVC v143 x86/x64 build tools, a Windows SDK and **C++ CMake tools for Windows**. Then launch the project from its root folder.

```powershell
.\run-windows.cmd
```

The script configures the **Win32** target, builds Release, copies the resources and runtime DLLs, and launches the game. Use the mouse to click two neighboring gems or drag between them. The generated solution is in `build-win32\`.

## Edit the game

| File or directory | Purpose |
| --- | --- |
| `Classes/LevelRules.cpp` | Original target classifier, ordered goal credit, turn limits, end conditions and star thresholds |
| `Classes/OriginalLevel.cpp` | Decode the original packed level files and identify supported mechanics |
| `Classes/OriginalBoard.cpp` | Opening layouts, blockers, floor layers, gravity, portals and deliveries |
| `Classes/Puzzle.cpp` | Legal swaps, match grouping, combinations, cascades, refills, shuffling and score |
| `Resources/original-index.txt` | Enabled paths in campaign order. A path identifies a stage across folders with overlapping numbers. |
| `Resources/campaign.json` | Prototype board data retained for the standalone regression tests |
| `Classes/GameScene.cpp` | Board drawing, controls, hints, animations and result display |
| `Classes/MainMenuScene.cpp` and `Classes/Interface.cpp` | Animated title, scrolling level map, progress storage, fonts, audio and translations |
| `Resources/art`, `Resources/music` and `Resources/text` | Individual assets and English, Japanese, Simplified Chinese and Traditional Chinese UI strings |

After adding a mechanic, update the decoder and gameplay together. The `index_original` tool reads one asset path per line from standard input and prints the enabled paths to standard output. Run it from the project root. Order the input by folder and then numeric level number. Omission reasons are printed to standard error. The game loads the index at startup.

## Run the rules tests

From the project root on Windows with Visual Studio C++ and CMake installed, or on Linux with a C++14 compiler and CMake, run the following commands.

```sh
cmake -S tests -B build-rules
cmake --build build-rules --config Release
ctest --test-dir build-rules -C Release --output-on-failure
```

The original-campaign test checks the opening tutorial swap, scoring, duplicate goal order, last-turn and timed outcomes, obstacle hits, drops, layered floors, portals and the victory bonus. It runs two seeded attempts per enabled definition through a win or loss. The custom-campaign test runs six attempts per stage. The tests exercise game rules independently of the graphical client. To check a generated APK against every file in the project resource tree, run `python tests/check_resource_apk.py proj.android/app/build/outputs/apk/debug/app-debug.apk` from the project root. Windows desktop execution still requires testing on Windows.

Bundled engine and component notices are in `engine/licenses/`.
