#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include "Input.h"
#include "Projection.h"
#include "LootFilter.h"
#include "HotbarAutoUse.h"

enum BotState {
    BOT_STATE_IDLE,
    BOT_STATE_PATROL,
    BOT_STATE_LOOTING,
    BOT_STATE_PAUSED,
    BOT_STATE_NAVIGATING,
    BOT_STATE_RETURNING_SPOT,
    BOT_STATE_TEAMMATE_SWEEP,
    BOT_STATE_SECONDARY_LOOT
};

enum HuntingMode {
    MODE_VIP_SPOT_LOCK = 0, // Stays locked on Anchor Spot, 360 scatter, loots on screen, snaps back
    MODE_CIRCUIT_PATROL = 1 // Original moving patrol across Desert
};

struct MapWaypoint {
    int x;
    int y;
};

struct ScreenLootTarget {
    int boxX, boxY, boxW, boxH;
    int clickX, clickY;
    int goldenPixels;
    int priority; // 100=CPBag, 90=Meteor/GreenCP, 85=(2)ExpBall, 80=ExpBall, 50=Sycee
    int itemCount; // 1 for CPBag / 1 ExpBall, 2 for (2) ExpBall, etc.
    std::string name;
    double dist;
};

class BotEngine {
private:
    std::atomic<bool> isRunning;
    std::atomic<bool> stopFlag;
    std::atomic<BotState> currentState;
    std::thread workerThread;

    HWND gameHwnd;

    // Anchor Coordinates & VIP Spot Lock Configuration
    std::atomic<int> anchorX;
    std::atomic<int> anchorY;
    std::atomic<bool> hasAnchor;
    std::atomic<int> maxLeashRadius; // Default: 6 tiles
    std::atomic<HuntingMode> huntingMode;

    // Multi-Client Secondary Account Background Watcher (1 CP or >= 2 Exp Balls)
    std::atomic<bool> enableSecondaryWatcher;
    std::atomic<int> secondaryCpThreshold;   // Default: 1 (at least 1 CP Bag)
    std::atomic<int> secondaryExpThreshold;  // Default: 2 (at least 2 Exp Balls)
    std::atomic<int> watcherCheckIntervalSec;// Default: 2 seconds
    std::chrono::steady_clock::time_point lastSecondaryCheckTime;
    std::atomic<bool> forceSecondaryCheckNow;
    std::atomic<int> lastFoundSecondaryCp;
    std::atomic<int> lastFoundSecondaryExp;

    // Legacy/Compatibility state for UI
    std::atomic<int> teammateX;
    std::atomic<int> teammateY;
    std::atomic<int> sweepIntervalSec;
    std::atomic<bool> isTeammateNear;
    std::atomic<bool> isBeaconVisible;
    std::atomic<int> lastBeaconX;
    std::atomic<int> lastBeaconY;

    int currentWaypointIndex;
    int totalDragonBallsLooted;
    int totalCpbagsLooted;
    int totalItemsLooted;
    std::string lastLootedName;
    std::string characterName;
    std::chrono::steady_clock::time_point lastCaptchaTime;
    std::chrono::steady_clock::time_point walkOnlyUntil;
    bool wasWalkingOnly;

    void MainLoop();
    bool SafeSleep(int ms);
    bool ReadCoordsFromTitle(HWND hwnd, int& outX, int& outY);
    bool CheckSecurityDialog(POINT& outYesClick);
    bool IsSecurityDialogVisible(POINT* pYesCenter = nullptr, bool* pIsNonSecurity = nullptr);
    void HandleCaptchaPause(const POINT& ptYes);
    bool AutoSolveBrowserCaptcha();
    HWND FindBrowserWindow();
    void ActivateTargetWindow(HWND hwnd);
    void RestoreAndFocusGame(HWND hwnd);
    void SwitchToGameClient();
    int lastVisibleMonsterPixels;
    bool CheckAndActivateSpeedSkill();
    bool CheckAndHandleRevive();
    void NavigateToHotspot();

    // Multi-Client Background Watcher Engine
    void CheckSecondaryClientsBackground();
    std::vector<HWND> FindSecondaryConquerWindows();
    bool ScanSecondaryClient(HWND hSec, int& outCp, int& outExp, std::vector<ScreenLootTarget>& outTargets);
    void ExecuteSecondaryAccountLoot(HWND hSec, const std::vector<ScreenLootTarget>& initialTargets);
    void SwitchWindowHardware(HWND targetHwnd);

    std::vector<ScreenLootTarget> ScanScreenForGroundDrops(HWND hwnd, int winW, int winH);
    int CountMonsterGreenPixels(HWND hwnd, int winW, int winH);
    bool VerifyLootDisappeared(HWND hwnd, int boxX, int boxY, int boxW, int boxH);
    bool IsLoginScreenVisible();
    bool CheckAndDismissEquipmentLock();
    void PressF7Scatter();
    bool IsScatterEquipped();
    void WaitForLoginAndEquipScatter();

    // Hotbar OCR Auto-Use: F4 Green Bag (starts strictly at 5 until 0) & F5 Yellow Chest (starts strictly at 1 until 0)
    void CheckAndHandleHotbarItems();
    std::atomic<bool> isUsingGreenBag{ false };
    std::atomic<bool> isUsingYellowChest{ false };
    ULONGLONG lastHotbarCheckTick{ 0 };
    ULONGLONG lastF4PressTick{ 0 };
    ULONGLONG lastF5PressTick{ 0 };

    enum ObstacleType {
        OBSTACLE_NONE = 0,
        OBSTACLE_STOP_HERE = 1,  // "[System]You can't stop here!" -> change mouse trajectory / escape tile
        OBSTACLE_JUMP_FAR = 2,   // "[System]You can't jump that far!" -> release Ctrl for 3.0s and walk
        OBSTACLE_REST_WHILE = 3  // "[System]Please rest for a while." -> do nothing, continue hunting normally
    };
    ObstacleType CheckObstacleMessage(HWND hwnd, int winW, int winH);
    bool IsObstacleMsgVisible(HWND hwnd, int winW, int winH);
    void DetectMonsterClusterBias(HWND hwnd, int winW, int winH, double& outBiasX, double& outBiasY);

    std::atomic<int> selectedHotspot; // 0 = Heartland (742, 673), 1 = East Gate (577, 672), 2 = Desert Trail (673, 676), 3 = Custom

public:
    BotEngine();
    ~BotEngine();

    bool Start();
    void Stop();
    void Pause();
    void Resume();
    void TogglePause();
    bool IsRunning() const { return isRunning.load(); }
    BotState GetState() const { return currentState.load(); }
    void FocusGameWindow(HWND hwnd);
    HWND FindGameWindow();

    // Anchor & Spot Lock API
    void SetAnchor(int x, int y);
    bool CaptureCurrentSpotAsAnchor();
    bool GetAnchor(int& outX, int& outY) const;
    void SetHuntingMode(HuntingMode mode);
    HuntingMode GetHuntingMode() const;
    void SetLeashRadius(int radius);
    int GetLeashRadius() const;
    bool ReadCurrentCoords(int& outX, int& outY);
    double GetDistanceToAnchor() const;

    // Hotspot Selection API
    void SetSelectedHotspot(int idx);
    int GetSelectedHotspot() const;

    // Multi-Client Secondary Account Watcher API
    void SetSecondaryWatcherEnabled(bool en);
    bool IsSecondaryWatcherEnabled() const;
    void SetSecondaryCpThreshold(int t);
    int GetSecondaryCpThreshold() const;
    void SetSecondaryExpThreshold(int t);
    int GetSecondaryExpThreshold() const;
    void TriggerSecondaryCheckNow();
    int GetLastFoundSecondaryCp() const { return lastFoundSecondaryCp.load(); }
    int GetLastFoundSecondaryExp() const { return lastFoundSecondaryExp.load(); }

    // UI Compatibility Aliases
    void SetTeammateSpot(int x, int y) { teammateX.store(x); teammateY.store(y); }
    bool CaptureCurrentSpotAsTeammate() { return true; }
    bool GetTeammateSpot(int& outX, int& outY) const { outX = teammateX.load(); outY = teammateY.load(); return true; }
    void SetTeammateSweepEnabled(bool en) { SetSecondaryWatcherEnabled(en); }
    bool IsTeammateSweepEnabled() const { return IsSecondaryWatcherEnabled(); }
    void SetSweepInterval(int sec) { sweepIntervalSec.store(sec); watcherCheckIntervalSec.store(sec > 10 ? 2 : sec); }
    int GetSweepInterval() const { return sweepIntervalSec.load(); }
    int GetSecondsUntilNextSweep() const;
    void TriggerTeammateSweepNow() { TriggerSecondaryCheckNow(); }
    bool IsTeammateNear() const { return isTeammateNear.load(); }
    bool IsBeaconVisible() const { return isBeaconVisible.load(); }
    void GetLastBeaconPos(int& x, int& y) const { x = lastBeaconX.load(); y = lastBeaconY.load(); }

    int GetDragonBallsCount() const { return totalDragonBallsLooted; }
    int GetCpbagsCount() const { return totalCpbagsLooted; }
    int GetTotalLootCount() const { return totalItemsLooted; }
    int GetCurrentWaypoint() const { return currentWaypointIndex; }
    std::string GetLastLootName() const { return lastLootedName; }
    HWND GetGameHwnd() const { return gameHwnd; }
};
