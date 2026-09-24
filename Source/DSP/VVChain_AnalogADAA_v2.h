#pragma once
#include <algorithm>
#include <cmath>

class VVChain_AnalogADAA_v2
{
public:
    enum class Mode { TT, SS };

    VVChain_AnalogADAA_v2() = default;

    void setMode(Mode newMode) noexcept
    {
        if (m_mode != newMode)
        {
            m_mode = newMode;
            resetState();
        }
    }

    inline double processSample(double input, double driveAlpha) noexcept
    {
        const double alpha = std::clamp(driveAlpha, 0.0, 1.25);
        if (alpha <= kMinAlpha)
        {
            m_prevX = input;
            m_hasPrev = true;
            return input;
        }

        if (!std::isfinite(input) || !std::isfinite(alpha))
        {
            resetState();
            return 0.0;
        }

        if (!m_hasPrev)
        {
            m_prevX = input;
            m_hasPrev = true;
            return calcF0(input, alpha);
        }

        const double diff = input - m_prevX;
        double saturated = 0.0;

        if (std::abs(diff) < kTinyDelta)
        {
            saturated = calcF0(0.5 * (input + m_prevX), alpha);
        }
        else
        {
            const double fCurr = calcAntiderivative(input, alpha);
            const double fPrev = calcAntiderivative(m_prevX, alpha);
            saturated = (fCurr - fPrev) / diff;
        }

        m_prevX = input;
        return std::isfinite(saturated) ? saturated : calcF0(input, alpha);
    }

    void resetState() noexcept
    {
        m_prevX = 0.0;
        m_hasPrev = false;
    }

private:
    static constexpr double kTinyDelta = 1.0e-7;
    static constexpr double kMinAlpha = 1.0e-5;
    Mode m_mode { Mode::TT };
    double m_prevX { 0.0 };
    bool m_hasPrev { false };

    inline double calcAntiderivative(double x, double alpha) const noexcept
    {
        const double x2 = x * x;
        const double u = 1.0 + alpha * x2;
        const double sqrtU = std::sqrt(u);
        const double u075 = sqrtU * std::sqrt(sqrtU);
        const double F = (2.0 / (3.0 * alpha)) * (u075 - 1.0);
        return F * unityNorm(alpha);
    }

    inline double calcF0(double x, double alpha) const noexcept
    {
        const double x2 = x * x;
        const double u = 1.0 + alpha * x2;
        const double invU025 = 1.0 / std::sqrt(std::sqrt(u));
        return x * invU025 * unityNorm(alpha);
    }

    static inline double unityNorm(double alpha) noexcept
    {
        return std::sqrt(std::sqrt(1.0 + alpha));
    }
};
