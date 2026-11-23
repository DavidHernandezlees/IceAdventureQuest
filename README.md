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

The enabled levels use the supplied layouts, goals, move limits, color weights and score thresholds. The 2,429 other definitions need additional piece behavior, secondary layers, directional or conveyor cells, or linked boards. The reference for the recovered rules was the supplied Android game version 2.3. The goal classifier, ordered collection, result ordering and star comparisons come from its compiled rules. Match combinations, plane target selection and combinations, refill, shuffle, obstacle damage and some score awards are reconstructed. Pet progression, paid boosters and online services are outside this implementation.

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
