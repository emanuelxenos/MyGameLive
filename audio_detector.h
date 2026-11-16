#pragma once

#include <atomic>

class AudioDetector {
public:
    static bool Init();
    static void Shutdown();
    static float GetLevel();
};
