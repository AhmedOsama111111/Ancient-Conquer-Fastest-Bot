#pragma once
#include <windows.h>
#include "BotEngine.h"

class VIPMenu {
public:
    static HWND Create(BotEngine* pBot);
    static void Run(BotEngine* pBot);
};
