#include <windows.h>
#include <iostream>
#include <iomanip>
#include "BotEngine.h"
#include "VIPMenu.h"

void PrintBanner() {
    system("cls");
    std::cout << "========================================================================\n";
    std::cout << "          CONQUER BOT NEW 1 - 100% HARDWARE DRIVER & SMART LOOTER       \n";
    std::cout << "  [Full Stealth Mode]: 100% Hardware Interception Driver + Screen Vision \n";
    std::cout << "========================================================================\n";
    std::cout << "  Operation Mode:\n";
    std::cout << "    1. VIP Spot Lock: Stays strictly on spot (696, 730), 360 scatter, loots\n";
    std::cout << "    2. GUI Render Menu: Set Spot, Live Screen Coords, Mode Switch, Start/Stop\n";
    std::cout << "    3. Teammate Loot Sweep: Periodic 5-min sweep of 2nd account ground bags\n";
    std::cout << "    4. Auto-Loot Priority Drops (TanCPBag, GreenCPBag, Meteor, DB, etc.)\n";
    std::cout << "------------------------------------------------------------------------\n";
    std::cout << "  Global Hotkeys:\n";
    std::cout << "    [F5] : LOCK TEAMMATE SPOT (2nd Account for Periodic Loot Sweep)\n";
    std::cout << "    [F6] : LOCK / SET ANCHOR SPOT (Home Hunting Coords)\n";
    std::cout << "    [F7] : START / RESUME Bot (VIP Spot Guard / Circuit)\n";
    std::cout << "    [F8] : PAUSE / UNPAUSE Bot (Safe Captcha & AFK Mode)\n";
    std::cout << "    [F9] : STOP Bot (Emergency Stop & Driver Release)\n";
    std::cout << "========================================================================\n\n";
}

int main() {
    SetConsoleTitleA("Host Process for Windows Services");
    HWND hConsole = GetConsoleWindow();
    if (hConsole) {
        SetWindowPos(hConsole, HWND_NOTOPMOST, 980, 520, 840, 480, SWP_SHOWWINDOW);
    }

    PrintBanner();

    BotEngine bot;

    std::cout << "[*] Launching ConquerBot VIP Render Menu GUI...\n";
    std::cout << "[*] Controls are accessible via the Graphical Menu or Global Hotkeys:\n";
    std::cout << "    -> [F5] Lock Teammate Spot (2nd account)\n";
    std::cout << "    -> [F6] Lock Anchor Spot (Home spot)\n";
    std::cout << "    -> [F7] Start Hunting\n";
    std::cout << "    -> [F8] Pause Bot\n";
    std::cout << "    -> [F9] Stop Bot\n\n";

    // Launch GUI Render Menu (Runs Win32 Message Loop)
    VIPMenu::Run(&bot);

    if (bot.IsRunning()) bot.Stop();
    std::cout << "\n[*] Exiting ConquerBot. Goodbye!\n";
    return 0;
}
