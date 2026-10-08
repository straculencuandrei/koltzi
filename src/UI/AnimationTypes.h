#pragma once
#include "Core/TriageReport.h"
#include <d2d1.h>
#include <vector>

namespace Koltzi {

struct Particle {
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float alpha = 1.0f;
    float size = 2.0f;
    float life = 1.0f;      // 0 to 1
    float maxLife = 1.0f;
    D2D1_COLOR_F color = { 1.0f, 1.0f, 1.0f, 1.0f };
};

struct AnimationState {
    float timeSeconds = 0.0f;
    float deltaSeconds = 0.016f;

    GhostMood currentMood = GhostMood::Idle;
    GhostMood targetMood = GhostMood::Idle;
    float moodTransition = 1.0f; // 0 to 1

    float floatOffsetY = 0.0f;
    float bodyTiltAngle = 0.0f;
    float blinkProgress = 0.0f; // 0 (open) to 1 (shut)
    float blinkTimer = 0.0f;

    // Sniffing scan line progress
    float scanLineY = 0.0f;

    // Panic jitter
    float jitterX = 0.0f;
    float jitterY = 0.0f;

    // Sweat drop
    float sweatY = 0.0f;
    float sweatAlpha = 0.0f;

    // Question mark
    float questionY = 0.0f;
    float questionAlpha = 0.0f;

    std::vector<Particle> motes;
};

} // namespace Koltzi
