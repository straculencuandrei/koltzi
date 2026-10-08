#include "GhostRenderer.h"
#include <random>

namespace Koltzi {

GhostRenderer::GhostRenderer() {
    m_anim.currentMood = GhostMood::Idle;
    m_anim.targetMood = GhostMood::Idle;
}

GhostRenderer::~GhostRenderer() {
    DiscardDeviceResources();
}

HRESULT GhostRenderer::Initialize(ID2D1RenderTarget* rt, IDWriteFactory* dwriteFactory) {
    DiscardDeviceResources();
    m_dwriteFactory = dwriteFactory;

    rt->GetFactory(&m_d2dFactory);

    HRESULT hr = S_OK;

    // Solid Brushes
    hr = rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &m_whiteBrush);
    if (FAILED(hr)) return hr;

    hr = rt->CreateSolidColorBrush(D2D1::ColorF(0.09f, 0.12f, 0.18f, 1.0f), &m_eyeBrush);
    if (FAILED(hr)) return hr;

    hr = rt->CreateSolidColorBrush(D2D1::ColorF(0.98f, 0.44f, 0.52f, 0.55f), &m_cheekBrush);
    if (FAILED(hr)) return hr;

    hr = rt->CreateSolidColorBrush(D2D1::ColorF(0.22f, 0.74f, 0.97f, 1.0f), &m_accentBrush);
    if (FAILED(hr)) return hr;

    hr = rt->CreateSolidColorBrush(D2D1::ColorF(0.22f, 0.74f, 0.97f, 0.4f), &m_glowBrush);
    if (FAILED(hr)) return hr;

    // Linear Gradient for Ghost Body
    D2D1_GRADIENT_STOP stops[2];
    stops[0].color = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.97f);
    stops[0].position = 0.0f;
    stops[1].color = D2D1::ColorF(0.85f, 0.91f, 1.0f, 0.90f);
    stops[1].position = 1.0f;

    ID2D1GradientStopCollection* stopCol = nullptr;
    hr = rt->CreateGradientStopCollection(stops, 2, &stopCol);
    if (SUCCEEDED(hr)) {
        hr = rt->CreateLinearGradientBrush(
            D2D1::LinearGradientBrushProperties(D2D1::Point2F(0, 0), D2D1::Point2F(0, 100)),
            stopCol,
            &m_bodyGradient
        );
        SafeRelease(stopCol);
    }

    // Radial Gradient for Aura
    D2D1_GRADIENT_STOP auraStops[3];
    auraStops[0].color = D2D1::ColorF(0.22f, 0.74f, 0.97f, 0.45f);
    auraStops[0].position = 0.0f;
    auraStops[1].color = D2D1::ColorF(0.22f, 0.74f, 0.97f, 0.15f);
    auraStops[1].position = 0.6f;
    auraStops[2].color = D2D1::ColorF(0.22f, 0.74f, 0.97f, 0.0f);
    auraStops[2].position = 1.0f;

    ID2D1GradientStopCollection* auraCol = nullptr;
    hr = rt->CreateGradientStopCollection(auraStops, 3, &auraCol);
    if (SUCCEEDED(hr)) {
        hr = rt->CreateRadialGradientBrush(
            D2D1::RadialGradientBrushProperties(D2D1::Point2F(0, 0), D2D1::Point2F(0, 0), 90.0f, 90.0f),
            auraCol,
            &m_auraGradient
        );
        SafeRelease(auraCol);
    }

    // Text Format for question mark and symbols
    if (m_dwriteFactory) {
        m_dwriteFactory->CreateTextFormat(
            L"Segoe UI",
            nullptr,
            DWRITE_FONT_WEIGHT_BOLD,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            24.0f,
            L"en-us",
            &m_symbolFormat
        );
    }

    return S_OK;
}

void GhostRenderer::DiscardDeviceResources() {
    SafeRelease(m_whiteBrush);
    SafeRelease(m_eyeBrush);
    SafeRelease(m_cheekBrush);
    SafeRelease(m_accentBrush);
    SafeRelease(m_glowBrush);
    SafeRelease(m_bodyGradient);
    SafeRelease(m_auraGradient);
    SafeRelease(m_symbolFormat);
    SafeRelease(m_d2dFactory);
}

void GhostRenderer::Update(float dt, GhostMood mood) {
    m_anim.deltaSeconds = dt;
    m_anim.timeSeconds += dt;
    m_anim.targetMood = mood;

    // Mood transition
    if (m_anim.currentMood != m_anim.targetMood) {
        m_anim.moodTransition += dt * 3.5f;
        if (m_anim.moodTransition >= 1.0f) {
            m_anim.currentMood = m_anim.targetMood;
            m_anim.moodTransition = 1.0f;
        }
    }

    // Vertical floating sinusoidal bobbing
    float floatFreq = (m_anim.currentMood == GhostMood::Sniffing) ? 5.0f :
                      (m_anim.currentMood == GhostMood::Alarmed)  ? 8.0f : 2.4f;
    float floatAmp  = (m_anim.currentMood == GhostMood::Alarmed)  ? 4.0f : 6.5f;
    m_anim.floatOffsetY = std::sin(m_anim.timeSeconds * floatFreq) * floatAmp;

    // Head tilt angle
    float targetTilt = 0.0f;
    if (m_anim.currentMood == GhostMood::Puzzled) {
        targetTilt = 0.22f; // ~13 degrees
    } else if (m_anim.currentMood == GhostMood::Sniffing) {
        targetTilt = std::sin(m_anim.timeSeconds * 6.0f) * 0.12f;
    }
    m_anim.bodyTiltAngle += (targetTilt - m_anim.bodyTiltAngle) * std::min(1.0f, dt * 6.0f);

    // Natural eye blink timer
    m_anim.blinkTimer += dt;
    if (m_anim.blinkTimer > 3.5f) {
        float blinkPhase = m_anim.blinkTimer - 3.5f;
        if (blinkPhase < 0.18f) {
            m_anim.blinkProgress = std::sin((blinkPhase / 0.18f) * 3.14159f);
        } else {
            m_anim.blinkProgress = 0.0f;
            m_anim.blinkTimer = 0.0f;
        }
    } else {
        m_anim.blinkProgress = 0.0f;
    }

    // Sniffing laser scan progress
    if (m_anim.currentMood == GhostMood::Sniffing) {
        m_anim.scanLineY += dt * 90.0f;
        if (m_anim.scanLineY > 110.0f) m_anim.scanLineY = -30.0f;
    }

    // Panic jitter for Alarmed state
    if (m_anim.currentMood == GhostMood::Alarmed) {
        static std::mt19937 rng(1337);
        std::uniform_real_distribution<float> dist(-1.8f, 1.8f);
        m_anim.jitterX = dist(rng);
        m_anim.jitterY = dist(rng);

        // Dripping sweat
        m_anim.sweatY += dt * 45.0f;
        if (m_anim.sweatY > 30.0f) m_anim.sweatY = 0.0f;
        m_anim.sweatAlpha = 1.0f - (m_anim.sweatY / 30.0f);
    } else {
        m_anim.jitterX = 0.0f;
        m_anim.jitterY = 0.0f;
        m_anim.sweatAlpha = 0.0f;
    }

    // Question mark for Puzzled state
    if (m_anim.currentMood == GhostMood::Puzzled) {
        m_anim.questionY = std::sin(m_anim.timeSeconds * 3.0f) * 4.0f;
        m_anim.questionAlpha = std::min(1.0f, m_anim.questionAlpha + dt * 3.0f);
    } else {
        m_anim.questionAlpha = std::max(0.0f, m_anim.questionAlpha - dt * 4.0f);
    }

    UpdateParticles(dt);
}

void GhostRenderer::UpdateParticles(float dt) {
    // Spawn particles occasionally
    if (m_anim.motes.size() < 18) {
        Particle p;
        p.x = ((rand() % 120) - 60.0f);
        p.y = 50.0f + (rand() % 40);
        p.vx = ((rand() % 40) - 20.0f) * 0.4f;
        p.vy = -18.0f - (rand() % 20);
        p.life = 0.0f;
        p.maxLife = 1.5f + ((rand() % 100) / 100.0f) * 1.5f;
        p.size = 1.5f + (rand() % 3);

        if (m_anim.currentMood == GhostMood::Alarmed) {
            p.color = D2D1::ColorF(0.94f, 0.27f, 0.27f, 0.7f);
        } else if (m_anim.currentMood == GhostMood::Happy) {
            p.color = D2D1::ColorF(0.06f, 0.73f, 0.51f, 0.7f);
        } else if (m_anim.currentMood == GhostMood::Puzzled) {
            p.color = D2D1::ColorF(0.96f, 0.62f, 0.04f, 0.7f);
        } else {
            p.color = D2D1::ColorF(0.22f, 0.74f, 0.97f, 0.6f);
        }
        m_anim.motes.push_back(p);
    }

    for (auto it = m_anim.motes.begin(); it != m_anim.motes.end();) {
        it->life += dt;
        if (it->life >= it->maxLife) {
            it = m_anim.motes.erase(it);
        } else {
            it->x += it->vx * dt;
            it->y += it->vy * dt;
            float progress = it->life / it->maxLife;
            it->alpha = (progress < 0.3f) ? (progress / 0.3f) : (1.0f - progress);
            ++it;
        }
    }
}

void GhostRenderer::Render(ID2D1RenderTarget* rt, float centerX, float centerY) {
    if (!rt) return;

    float drawX = centerX + m_anim.jitterX;
    float drawY = centerY + m_anim.floatOffsetY + m_anim.jitterY;

    // Mood Aura Color
    D2D1_COLOR_F auraColor;
    switch (m_anim.currentMood) {
    case GhostMood::Alarmed:
        auraColor = D2D1::ColorF(0.94f, 0.27f, 0.27f, 0.65f);
        break;
    case GhostMood::Puzzled:
        auraColor = D2D1::ColorF(0.96f, 0.62f, 0.04f, 0.50f);
        break;
    case GhostMood::Happy:
        auraColor = D2D1::ColorF(0.06f, 0.73f, 0.51f, 0.55f);
        break;
    case GhostMood::Sniffing:
        auraColor = D2D1::ColorF(0.18f, 0.83f, 0.75f, 0.60f);
        break;
    case GhostMood::Idle:
    default:
        auraColor = D2D1::ColorF(0.22f, 0.74f, 0.97f, 0.38f);
        break;
    }

    // Save previous transform
    D2D1_MATRIX_3X2_F origTransform;
    rt->GetTransform(&origTransform);

    // Render Aura (without tilt)
    RenderAura(rt, drawX, drawY + 10.0f, auraColor);

    // Apply Head/Body Tilt
    D2D1_MATRIX_3X2_F tiltMatrix = D2D1::Matrix3x2F::Rotation(
        m_anim.bodyTiltAngle * (180.0f / 3.14159f),
        D2D1::Point2F(drawX, drawY + 20.0f)
    );
    rt->SetTransform(origTransform * tiltMatrix);

    // Render Ghost Body and Face
    RenderBody(rt, drawX, drawY);
    RenderArms(rt, drawX, drawY);
    RenderFace(rt, drawX, drawY);
    RenderSpecialEffects(rt, drawX, drawY);

    // Restore transform for particles
    rt->SetTransform(origTransform);
    RenderParticles(rt);
}

void GhostRenderer::RenderAura(ID2D1RenderTarget* rt, float cx, float cy, D2D1_COLOR_F auraColor) {
    float pulse = 1.0f + 0.08f * std::sin(m_anim.timeSeconds * 4.0f);
    float radius = 85.0f * pulse;

    D2D1_GRADIENT_STOP stops[3];
    stops[0].color = auraColor;
    stops[0].position = 0.0f;
    stops[1].color = D2D1::ColorF(auraColor.r, auraColor.g, auraColor.b, auraColor.a * 0.35f);
    stops[1].position = 0.55f;
    stops[2].color = D2D1::ColorF(auraColor.r, auraColor.g, auraColor.b, 0.0f);
    stops[2].position = 1.0f;

    ID2D1GradientStopCollection* stopCol = nullptr;
    if (SUCCEEDED(rt->CreateGradientStopCollection(stops, 3, &stopCol))) {
        ID2D1RadialGradientBrush* auraBrush = nullptr;
        if (SUCCEEDED(rt->CreateRadialGradientBrush(
            D2D1::RadialGradientBrushProperties(D2D1::Point2F(cx, cy), D2D1::Point2F(0, 0), radius, radius * 1.15f),
            stopCol,
            &auraBrush
        ))) {
            rt->FillEllipse(
                D2D1::Ellipse(D2D1::Point2F(cx, cy), radius, radius * 1.15f),
                auraBrush
            );
            SafeRelease(auraBrush);
        }
        SafeRelease(stopCol);
    }
}

void GhostRenderer::RenderBody(ID2D1RenderTarget* rt, float cx, float cy) {
    if (!m_d2dFactory) return;

    ID2D1PathGeometry* path = nullptr;
    HRESULT hr = m_d2dFactory->CreatePathGeometry(&path);
    if (FAILED(hr)) return;

    ID2D1GeometrySink* sink = nullptr;
    hr = path->Open(&sink);
    if (FAILED(hr)) {
        SafeRelease(path);
        return;
    }

    // Geometry Dimensions
    const float headR = 38.0f;
    const float topY = cy - 42.0f;
    const float skirtY = cy + 48.0f;
    const float leftX = cx - 42.0f;
    const float rightX = cx + 42.0f;

    // Start at top center of head dome
    sink->BeginFigure(D2D1::Point2F(cx, topY - headR), D2D1_FIGURE_BEGIN_FILLED);

    // Left arc of head dome
    sink->AddBezier(D2D1::BezierSegment(
        D2D1::Point2F(cx - headR * 0.8f, topY - headR),
        D2D1::Point2F(cx - headR, topY - headR * 0.4f),
        D2D1::Point2F(cx - headR, topY)
    ));

    // Down the left side of skirt with gentle flare
    sink->AddBezier(D2D1::BezierSegment(
        D2D1::Point2F(cx - headR, topY + 30.0f),
        D2D1::Point2F(leftX - 4.0f, skirtY - 20.0f),
        D2D1::Point2F(leftX, skirtY)
    ));

    // Animated bottom wavy skirt (3 Bézier wave scallops)
    float wavePhase = m_anim.timeSeconds * 5.0f;
    float waveAmp = (m_anim.currentMood == GhostMood::Alarmed) ? 6.5f : 4.5f;

    float stepX = (rightX - leftX) / 3.0f;
    for (int i = 0; i < 3; ++i) {
        float x0 = leftX + i * stepX;
        float x1 = x0 + stepX;

        float ripple = std::sin(wavePhase + i * 2.0f) * waveAmp;
        float valleyY = skirtY - 8.0f + ripple;
        float crestY = skirtY + 6.0f - ripple;

        sink->AddBezier(D2D1::BezierSegment(
            D2D1::Point2F(x0 + stepX * 0.25f, crestY),
            D2D1::Point2F(x0 + stepX * 0.75f, valleyY),
            D2D1::Point2F(x1, skirtY)
        ));
    }

    // Up the right side of skirt
    sink->AddBezier(D2D1::BezierSegment(
        D2D1::Point2F(rightX + 4.0f, skirtY - 20.0f),
        D2D1::Point2F(cx + headR, topY + 30.0f),
        D2D1::Point2F(cx + headR, topY)
    ));

    // Right arc of head dome to top center
    sink->AddBezier(D2D1::BezierSegment(
        D2D1::Point2F(cx + headR, topY - headR * 0.4f),
        D2D1::Point2F(cx + headR * 0.8f, topY - headR),
        D2D1::Point2F(cx, topY - headR)
    ));

    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();
    SafeRelease(sink);

    // Update body gradient position
    if (m_bodyGradient) {
        m_bodyGradient->SetStartPoint(D2D1::Point2F(cx, topY - headR));
        m_bodyGradient->SetEndPoint(D2D1::Point2F(cx, skirtY + 8.0f));
    }

    // Fill Ghost Body
    rt->FillGeometry(path, m_bodyGradient ? (ID2D1Brush*)m_bodyGradient : (ID2D1Brush*)m_whiteBrush);

    // Outer subtle spectral stroke
    if (m_accentBrush) {
        if (m_anim.currentMood == GhostMood::Alarmed) {
            m_accentBrush->SetColor(D2D1::ColorF(0.94f, 0.27f, 0.27f, 0.6f));
        } else if (m_anim.currentMood == GhostMood::Happy) {
            m_accentBrush->SetColor(D2D1::ColorF(0.06f, 0.73f, 0.51f, 0.5f));
        } else if (m_anim.currentMood == GhostMood::Puzzled) {
            m_accentBrush->SetColor(D2D1::ColorF(0.96f, 0.62f, 0.04f, 0.5f));
        } else {
            m_accentBrush->SetColor(D2D1::ColorF(0.56f, 0.76f, 0.98f, 0.45f));
        }
        rt->DrawGeometry(path, m_accentBrush, 1.8f);
    }

    SafeRelease(path);
}

void GhostRenderer::RenderArms(ID2D1RenderTarget* rt, float cx, float cy) {
    if (!m_whiteBrush) return;

    float armFloat = std::sin(m_anim.timeSeconds * 3.5f) * 3.0f;
    float leftArmX = cx - 44.0f;
    float leftArmY = cy + 6.0f + armFloat;
    float rightArmX = cx + 44.0f;
    float rightArmY = cy + 6.0f - armFloat;

    if (m_anim.currentMood == GhostMood::Alarmed) {
        // Arms up in panic!
        leftArmY = cy - 8.0f + armFloat * 1.5f;
        rightArmY = cy - 8.0f - armFloat * 1.5f;
    } else if (m_anim.currentMood == GhostMood::Sniffing) {
        // Outstretched forward sniffing
        leftArmX = cx - 40.0f;
        rightArmX = cx + 48.0f;
    }

    // Left arm nub
    rt->FillRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(leftArmX - 10.0f, leftArmY - 6.0f, leftArmX + 10.0f, leftArmY + 12.0f), 7.0f, 7.0f),
        m_whiteBrush
    );

    // Right arm nub
    rt->FillRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(rightArmX - 10.0f, rightArmY - 6.0f, rightArmX + 10.0f, rightArmY + 12.0f), 7.0f, 7.0f),
        m_whiteBrush
    );
}

void GhostRenderer::RenderFace(ID2D1RenderTarget* rt, float cx, float cy) {
    if (!m_eyeBrush || !m_whiteBrush || !m_cheekBrush) return;

    const float eyeY = cy - 24.0f;
    const float leftEyeX = cx - 15.0f;
    const float rightEyeX = cx + 15.0f;

    // Blushing cheeks
    rt->FillEllipse(
        D2D1::Ellipse(D2D1::Point2F(cx - 24.0f, cy - 10.0f), 8.5f, 5.0f),
        m_cheekBrush
    );
    rt->FillEllipse(
        D2D1::Ellipse(D2D1::Point2F(cx + 24.0f, cy - 10.0f), 8.5f, 5.0f),
        m_cheekBrush
    );

    // EYES BASED ON MOOD
    if (m_anim.currentMood == GhostMood::Happy) {
        // Curved happy crescent eyes (^ ^)
        m_eyeBrush->SetColor(D2D1::ColorF(0.09f, 0.12f, 0.18f, 1.0f));

        // Draw cute arch lines
        rt->DrawLine(D2D1::Point2F(leftEyeX - 6.0f, eyeY + 2.0f), D2D1::Point2F(leftEyeX, eyeY - 4.0f), m_eyeBrush, 2.5f);
        rt->DrawLine(D2D1::Point2F(leftEyeX, eyeY - 4.0f), D2D1::Point2F(leftEyeX + 6.0f, eyeY + 2.0f), m_eyeBrush, 2.5f);

        rt->DrawLine(D2D1::Point2F(rightEyeX - 6.0f, eyeY + 2.0f), D2D1::Point2F(rightEyeX, eyeY - 4.0f), m_eyeBrush, 2.5f);
        rt->DrawLine(D2D1::Point2F(rightEyeX, eyeY - 4.0f), D2D1::Point2F(rightEyeX + 6.0f, eyeY + 2.0f), m_eyeBrush, 2.5f);

        // Cute smiling mouth arc
        rt->DrawLine(D2D1::Point2F(cx - 5.0f, cy - 8.0f), D2D1::Point2F(cx, cy - 5.0f), m_eyeBrush, 2.0f);
        rt->DrawLine(D2D1::Point2F(cx, cy - 5.0f), D2D1::Point2F(cx + 5.0f, cy - 8.0f), m_eyeBrush, 2.0f);
    }
    else if (m_anim.currentMood == GhostMood::Alarmed) {
        // Wide shocked eyes (O_O)
        float shockR = 9.0f;
        // White sclera
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(leftEyeX, eyeY), shockR, shockR * 1.1f), m_whiteBrush);
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(rightEyeX, eyeY), shockR, shockR * 1.1f), m_whiteBrush);
        rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(leftEyeX, eyeY), shockR, shockR * 1.1f), m_eyeBrush, 1.8f);
        rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(rightEyeX, eyeY), shockR, shockR * 1.1f), m_eyeBrush, 1.8f);

        // Tiny dilated pupils shaking
        float pupilShake = std::sin(m_anim.timeSeconds * 20.0f) * 1.2f;
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(leftEyeX + pupilShake, eyeY), 3.2f, 3.2f), m_eyeBrush);
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(rightEyeX + pupilShake, eyeY), 3.2f, 3.2f), m_eyeBrush);

        // O-shaped mouth
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy - 6.0f), 3.5f, 5.0f), m_eyeBrush);
    }
    else if (m_anim.currentMood == GhostMood::Puzzled) {
        // Left eye normal, right eye arched up (o . O)
        float openScale = 1.0f - m_anim.blinkProgress;

        // Left eye
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(leftEyeX, eyeY), 4.5f, 7.0f * openScale), m_eyeBrush);
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(leftEyeX + 1.5f, eyeY - 2.5f * openScale), 1.8f, 2.2f * openScale), m_whiteBrush);

        // Right eye raised up high
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(rightEyeX, eyeY - 5.0f), 6.0f, 8.5f * openScale), m_eyeBrush);
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(rightEyeX + 2.0f, eyeY - 7.5f * openScale), 2.2f, 2.8f * openScale), m_whiteBrush);

        // Wavy puzzled mouth
        rt->DrawLine(D2D1::Point2F(cx - 4.0f, cy - 6.0f), D2D1::Point2F(cx, cy - 8.0f), m_eyeBrush, 1.8f);
        rt->DrawLine(D2D1::Point2F(cx, cy - 8.0f), D2D1::Point2F(cx + 4.0f, cy - 6.0f), m_eyeBrush, 1.8f);
    }
    else if (m_anim.currentMood == GhostMood::Sniffing) {
        // Focused determined squint eyes (> <)
        rt->DrawLine(D2D1::Point2F(leftEyeX - 6.0f, eyeY - 2.0f), D2D1::Point2F(leftEyeX + 5.0f, eyeY + 1.0f), m_eyeBrush, 2.2f);
        rt->DrawLine(D2D1::Point2F(rightEyeX - 5.0f, eyeY + 1.0f), D2D1::Point2F(rightEyeX + 6.0f, eyeY - 2.0f), m_eyeBrush, 2.2f);

        // Sniffing nose dots
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx - 2.0f, cy - 12.0f), 1.2f, 1.2f), m_eyeBrush);
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx + 2.0f, cy - 12.0f), 1.2f, 1.2f), m_eyeBrush);
    }
    else {
        // IDLE: Big expressive cute glossy eyes with blink
        float openScale = std::max(0.1f, 1.0f - m_anim.blinkProgress);

        // Left Eye
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(leftEyeX, eyeY), 5.5f, 8.5f * openScale), m_eyeBrush);
        if (openScale > 0.4f) {
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(leftEyeX + 2.0f, eyeY - 3.0f), 2.2f, 3.0f * openScale), m_whiteBrush);
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(leftEyeX - 1.5f, eyeY + 3.0f), 1.2f, 1.2f * openScale), m_whiteBrush);
        }

        // Right Eye
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(rightEyeX, eyeY), 5.5f, 8.5f * openScale), m_eyeBrush);
        if (openScale > 0.4f) {
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(rightEyeX + 2.0f, eyeY - 3.0f), 2.2f, 3.0f * openScale), m_whiteBrush);
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(rightEyeX - 1.5f, eyeY + 3.0f), 1.2f, 1.2f * openScale), m_whiteBrush);
        }

        // Small soft smile
        rt->DrawLine(D2D1::Point2F(cx - 3.0f, cy - 8.0f), D2D1::Point2F(cx, cy - 6.5f), m_eyeBrush, 1.8f);
        rt->DrawLine(D2D1::Point2F(cx, cy - 6.5f), D2D1::Point2F(cx + 3.0f, cy - 8.0f), m_eyeBrush, 1.8f);
    }
}

void GhostRenderer::RenderSpecialEffects(ID2D1RenderTarget* rt, float cx, float cy) {
    // 1. Sniffing Laser / Radar Scan Lines
    if (m_anim.currentMood == GhostMood::Sniffing && m_glowBrush) {
        float lineY = cy - 40.0f + m_anim.scanLineY;
        m_glowBrush->SetColor(D2D1::ColorF(0.18f, 0.83f, 0.75f, 0.75f));

        // Horizontal scan line with gradient fade
        rt->DrawLine(
            D2D1::Point2F(cx - 45.0f, lineY),
            D2D1::Point2F(cx + 45.0f, lineY),
            m_glowBrush,
            2.5f
        );

        // Radar ping arcs radiating forward
        float pingR = std::fmod(m_anim.timeSeconds * 40.0f, 35.0f);
        m_glowBrush->SetColor(D2D1::ColorF(0.18f, 0.83f, 0.75f, 1.0f - (pingR / 35.0f)));
        rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy - 10.0f), pingR, pingR * 0.7f), m_glowBrush, 1.5f);
    }

    // 2. Alarmed Dripping Sweat Drops
    if (m_anim.currentMood == GhostMood::Alarmed && m_anim.sweatAlpha > 0.05f) {
        ID2D1SolidColorBrush* sweatBrush = nullptr;
        rt->CreateSolidColorBrush(D2D1::ColorF(0.38f, 0.74f, 0.98f, m_anim.sweatAlpha), &sweatBrush);
        if (sweatBrush) {
            float sweatX = cx + 32.0f;
            float sweatY = cy - 35.0f + m_anim.sweatY;
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(sweatX, sweatY), 2.8f, 4.0f), sweatBrush);
            SafeRelease(sweatBrush);
        }
    }

    // 3. Puzzled Floating Question Mark
    if (m_anim.questionAlpha > 0.05f && m_symbolFormat) {
        ID2D1SolidColorBrush* qBrush = nullptr;
        rt->CreateSolidColorBrush(D2D1::ColorF(0.96f, 0.62f, 0.04f, m_anim.questionAlpha), &qBrush);
        if (qBrush) {
            D2D1_RECT_F qRect = D2D1::RectF(cx + 25.0f, cy - 80.0f + m_anim.questionY, cx + 55.0f, cy - 50.0f + m_anim.questionY);
            rt->DrawText(L"?", 1, m_symbolFormat, qRect, qBrush);
            SafeRelease(qBrush);
        }
    }
}

void GhostRenderer::RenderParticles(ID2D1RenderTarget* rt) {
    if (!rt) return;

    for (const auto& p : m_anim.motes) {
        ID2D1SolidColorBrush* pBrush = nullptr;
        D2D1_COLOR_F c = p.color;
        c.a = p.alpha;
        rt->CreateSolidColorBrush(c, &pBrush);
        if (pBrush) {
            rt->FillEllipse(
                D2D1::Ellipse(D2D1::Point2F(p.x + 110.0f, p.y + 160.0f), p.size, p.size),
                pBrush
            );
            SafeRelease(pBrush);
        }
    }
}

} // namespace Koltzi
