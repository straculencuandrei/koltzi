#pragma once
#include "Common.h"
#include "AnimationTypes.h"
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>

namespace Koltzi {

class GhostRenderer {
public:
    GhostRenderer();
    ~GhostRenderer();

    HRESULT Initialize(ID2D1RenderTarget* renderTarget, IDWriteFactory* dwriteFactory);
    void DiscardDeviceResources();

    void Update(float deltaSeconds, GhostMood mood);
    void Render(ID2D1RenderTarget* renderTarget, float centerX, float centerY);

    const AnimationState& GetAnimationState() const { return m_anim; }

private:
    void RenderAura(ID2D1RenderTarget* rt, float cx, float cy, D2D1_COLOR_F auraColor);
    void RenderBody(ID2D1RenderTarget* rt, float cx, float cy);
    void RenderFace(ID2D1RenderTarget* rt, float cx, float cy);
    void RenderArms(ID2D1RenderTarget* rt, float cx, float cy);
    void RenderSpecialEffects(ID2D1RenderTarget* rt, float cx, float cy);
    void RenderParticles(ID2D1RenderTarget* rt);

    void UpdateParticles(float dt);

    AnimationState m_anim;

    // Direct2D Resources
    ID2D1Factory* m_d2dFactory = nullptr;
    IDWriteFactory* m_dwriteFactory = nullptr;

    ID2D1SolidColorBrush* m_whiteBrush = nullptr;
    ID2D1SolidColorBrush* m_eyeBrush = nullptr;
    ID2D1SolidColorBrush* m_cheekBrush = nullptr;
    ID2D1SolidColorBrush* m_accentBrush = nullptr;
    ID2D1SolidColorBrush* m_glowBrush = nullptr;
    ID2D1LinearGradientBrush* m_bodyGradient = nullptr;
    ID2D1RadialGradientBrush* m_auraGradient = nullptr;

    IDWriteTextFormat* m_symbolFormat = nullptr;
};

} // namespace Koltzi
