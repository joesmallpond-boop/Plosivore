// PlosiveEngine.h — Plosivore plosive / mic-pop remover DSP core.
// Plain C++17, no framework dependency, real-time safe (no allocation in process()).
//
// How it works
// ------------
// 1. Each channel is split into three bands with phase-matched 4th-order Linkwitz-Riley
//    crossovers (sum is flat):  LOW  (< Freq)      — where pops and capsule thumps live
//                               CHEST (Freq..2.2x) — pop "body" that bleeds above Freq
//                               VOICE (> 2.2x Freq) — never touched
// 2. A fast detector compares low-frequency energy against the voice above it. A plosive
//    is a sudden LF burst far louder than the voice; normal speech/rap keeps them in
//    balance. A "burst" term (fast vs slow LF envelope) favours sudden onsets, so
//    sustained low notes and deep voices are left alone.
// 3. When a pop is confirmed, LOW is ducked hard (up to Depth) and CHEST by half as many dB.
// 4. Lookahead (0–10 ms) delays the audio so the duck lands before the pop's first cycle.
#pragma once
#include <cmath>
#include <vector>
#include <algorithm>

namespace plosivore
{

// Portable constants (MSVC doesn't define M_PI / M_SQRT1_2 without _USE_MATH_DEFINES)
constexpr double kPi = 3.14159265358979323846;
constexpr double kSqrtHalf = 0.70710678118654752440;

static inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
static inline float gainToDb (float g)  { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }

// ---------------------------------------------------------------- Biquad (TDF-II)
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;

    void reset() { z1 = z2 = 0; }

    inline float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setLowPass (double fs, double fc, double q = kSqrtHalf)
    {
        const double w = std::tan (kPi * fc / fs);
        const double n = 1.0 / (1.0 + w / q + w * w);
        b0 = (float) (w * w * n); b1 = 2.0f * b0; b2 = b0;
        a1 = (float) (2.0 * (w * w - 1.0) * n);
        a2 = (float) ((1.0 - w / q + w * w) * n);
    }
    void setHighPass (double fs, double fc, double q = kSqrtHalf)
    {
        const double w = std::tan (kPi * fc / fs);
        const double n = 1.0 / (1.0 + w / q + w * w);
        b0 = (float) n; b1 = -2.0f * b0; b2 = b0;
        a1 = (float) (2.0 * (w * w - 1.0) * n);
        a2 = (float) ((1.0 - w / q + w * w) * n);
    }
};

// 4th-order Linkwitz-Riley split: lp + hp == allpass(x)
struct LR4
{
    Biquad lp[2], hp[2];
    void set (double fs, double fc)
    {
        for (auto& b : lp) b.setLowPass (fs, fc);
        for (auto& b : hp) b.setHighPass (fs, fc);
    }
    void reset() { for (auto& b : lp) b.reset(); for (auto& b : hp) b.reset(); }
    inline void split (float x, float& lo, float& hi) noexcept
    {
        lo = lp[1].process (lp[0].process (x));
        hi = hp[1].process (hp[0].process (x));
    }
};

// ---------------------------------------------------------------- Envelope follower
struct Envelope
{
    float att = 0, rel = 0, v = 0;
    void set (double fs, double attackMs, double releaseMs)
    {
        att = (float) std::exp (-1.0 / (fs * attackMs  * 0.001));
        rel = (float) std::exp (-1.0 / (fs * releaseMs * 0.001));
    }
    inline float process (float x) noexcept
    {
        const float c = x > v ? att : rel;
        v = x + c * (v - x);
        return v;
    }
    void reset() { v = 0; }
};

// ---------------------------------------------------------------- Parameters
struct Params
{
    float sensitivity = 60.0f;   // 0..100 %
    float frequency   = 150.0f;  // crossover Hz, 60..300
    float depthDb     = 24.0f;   // max low-band reduction, 0..40 dB
    float releaseMs   = 40.0f;   // 5..250 ms
    float lookaheadMs = 0.0f;    // 0..10 ms (plugin offers 0 or 3)
    bool  listen      = false;   // output only what is removed
    float outputDb    = 0.0f;
};

// One telemetry frame per ~1.33 ms of audio, for the UI.
struct Frame
{
    float inPeak  = 0;  // input peak (time-aligned with output)
    float outPeak = 0;  // output peak
    float remPeak = 0;  // peak of what was removed
    float grDb    = 0;  // deepest low-band gain reduction in the frame (<= 0)
};

// ---------------------------------------------------------------- Engine
class Engine
{
public:
    static constexpr int maxChannels = 8;
    static constexpr float maxLookaheadMs = 10.0f;

    // Tuning (validated with the synthetic fast-rap tests in Tests/)
    float  kSlope = 3.0f;       // dB of cut per dB of detector excess
    double kLowRel = 25.0;      // LF envelope release (rides the pop's tail)
    double kHold = 0.010;       // s; hold reduction through pop cycles
    float  kChestAmount = 0.5f; // chest band gets this fraction of the low band's dB cut

    void prepare (double sampleRate, int numChannels)
    {
        fs = sampleRate;
        chans = std::clamp (numChannels, 1, maxChannels);
        bufLen = (int) std::ceil (fs * maxLookaheadMs * 0.001) + 4;
        for (auto& c : ch)
            for (auto& d : c.delay) d.assign ((size_t) bufLen, 0.0f);
        hfEnv.set (fs, 1.0, 60.0);
        frameLen = std::max (16, (int) std::round (fs / 750.0));
        currentFreq = lastSetFreq = -1;
        setParams (params);
        reset();
        snapCrossover();
    }

    void reset()
    {
        for (auto& c : ch)
        {
            c.xLow.reset(); c.xHigh.reset(); c.apLow.reset(); c.detLp.reset(); c.detHp.reset();
            for (auto& d : c.delay) std::fill (d.begin(), d.end(), 0.0f);
        }
        writePos = 0;
        lowFast.reset(); lowSlow.reset(); hfEnv.reset();
        gr = 1.0f; holdCount = 0;
        frame = {}; frameCount = 0;
    }

    void setParams (const Params& p)
    {
        params = p;
        if (fs <= 0) return;

        lookSamples = std::clamp ((int) std::round (p.lookaheadMs * 0.001 * fs), 0, bufLen - 2);

        // Detector threshold: (LF level - voice level [+ burst]) in dB.
        // 0 % -> needs +20 dB of excess LF; 100 % -> +1 dB.
        const float sens = std::clamp (p.sensitivity, 0.0f, 100.0f);
        thresholdDb = 20.0f - 0.19f * sens;
        floorLin = dbToGain (-60.0f + 0.15f * (100.0f - sens)); // ignore rumble / silence

        lowFast.set (fs, 0.15, kLowRel);
        lowSlow.set (fs, 35.0, 180.0);

        const double attMs = lookSamples > 0 ? std::clamp (p.lookaheadMs * 0.3, 0.08, 1.5) : 0.08;
        grAttack  = (float) std::exp (-1.0 / (fs * attMs * 0.001));
        grRelease = (float) std::exp (-1.0 / (fs * std::max (5.0f, p.releaseMs) * 0.001));
        holdSamples = (int) (fs * kHold);

        maxCutDb = std::max (0.0f, p.depthDb);
        outGain = dbToGain (p.outputDb);
    }

    int getLatencySamples() const { return lookSamples; }

    // Smoothly retune crossovers (call once per block; cheap).
    void updateCrossover (float targetHz)
    {
        targetHz = std::clamp (targetHz, 40.0f, 400.0f);
        if (currentFreq < 0) currentFreq = targetHz;
        else currentFreq += 0.3f * (targetHz - currentFreq);
        if (std::abs (currentFreq - lastSetFreq) < 0.05f) return;
        lastSetFreq = currentFreq;
        const double f1 = currentFreq, f2 = std::min (currentFreq * 2.2, fs * 0.45);
        for (auto& c : ch)
        {
            c.xLow.set (fs, f1);
            c.xHigh.set (fs, f2);
            c.apLow.set (fs, f2);            // phase-match LOW to the second crossover
            c.detLp.setLowPass (fs, f1 * 1.15);  // 12 dB/oct: faster than LR4 for detection
            c.detHp.setHighPass (fs, f2);
        }
    }
    void snapCrossover() { for (int i = 0; i < 64; ++i) updateCrossover (params.frequency); }

    // In-place processing. `onFrame(const Frame&)` is called every ~1.33 ms of audio.
    template <typename FrameSink>
    void process (float* const* data, int numChannels, int numSamples, FrameSink&& onFrame) noexcept
    {
        const int nc = std::min (numChannels, chans);
        updateCrossover (params.frequency);

        for (int i = 0; i < numSamples; ++i)
        {
            // ---- split & detect (linked across channels: loudest wins)
            float detLow = 0.0f, detVoice = 0.0f;
            for (int c = 0; c < nc; ++c)
            {
                auto& s = ch[c];
                const float x = data[c][i];
                float lo, rest, chest, voice, la, lb;
                s.xLow.split (x, lo, rest);
                s.xHigh.split (rest, chest, voice);
                s.apLow.split (lo, la, lb);
                s.delay[0][(size_t) writePos] = la + lb;  // LOW (phase-matched)
                s.delay[1][(size_t) writePos] = chest;
                s.delay[2][(size_t) writePos] = voice;
                detLow   = std::max (detLow,   std::abs (s.detLp.process (x)));
                detVoice = std::max (detVoice, std::abs (s.detHp.process (x)));
            }

            const float lf = lowFast.process (detLow);
            const float ls = lowSlow.process (detLow);
            const float hf = hfEnv.process (detVoice);

            float targetDb = 0.0f;
            if (lf > floorLin)
            {
                const float lfDb    = gainToDb (lf);
                const float ratioDb = lfDb - gainToDb (hf + 1.0e-6f);
                const float burstDb = std::max (0.0f, lfDb - gainToDb (ls + 1.0e-6f));
                const float excess  = ratioDb + 0.5f * burstDb - thresholdDb;
                if (excess > 0.0f)
                {
                    // Soft knee over the first 2 dB, then steep: a confirmed pop is pulled
                    // hard toward full Depth instead of just down to the threshold.
                    const float cut = excess < 2.0f ? excess * excess * kSlope * 0.25f
                                                    : (excess - 1.0f) * kSlope;
                    targetDb = -std::min (maxCutDb, cut);
                }
            }
            const float target = dbToGainFast (targetDb);

            // ---- smooth gain with hold
            if (target < gr)        { gr = target + grAttack * (gr - target); holdCount = holdSamples; }
            else if (holdCount > 0) { --holdCount; }
            else                    { gr = target + grRelease * (gr - target); }

            const float grLowDb = gainToDb (gr);
            const float gChest  = dbToGainFast (grLowDb * kChestAmount);

            // ---- read delayed bands, apply, recombine
            const int readPos = (writePos - lookSamples + bufLen) % bufLen;
            float inPk = 0, outPk = 0, remPk = 0;
            for (int c = 0; c < nc; ++c)
            {
                auto& s = ch[c];
                const float lo    = s.delay[0][(size_t) readPos];
                const float chest = s.delay[1][(size_t) readPos];
                const float voice = s.delay[2][(size_t) readPos];
                const float dry = lo + chest + voice;
                const float wet = lo * gr + chest * gChest + voice;
                const float removed = dry - wet;
                data[c][i] = (params.listen ? removed : wet) * outGain;
                inPk  = std::max (inPk,  std::abs (dry));
                outPk = std::max (outPk, std::abs (wet));
                remPk = std::max (remPk, std::abs (removed));
            }
            writePos = (writePos + 1) % bufLen;

            // ---- telemetry
            frame.inPeak  = std::max (frame.inPeak,  inPk);
            frame.outPeak = std::max (frame.outPeak, outPk);
            frame.remPeak = std::max (frame.remPeak, remPk);
            frame.grDb    = std::min (frame.grDb,    grLowDb);
            if (++frameCount >= frameLen)
            {
                onFrame (frame);
                frame = {}; frameCount = 0;
            }
        }
    }

    float getCurrentReductionDb() const { return gainToDb (gr); }
    double getFrameRateHz() const { return fs / frameLen; }

private:
    static inline float dbToGainFast (float db) noexcept { return db >= 0.0f ? 1.0f : std::exp (db * 0.11512925f); }

    struct Channel
    {
        LR4 xLow, xHigh, apLow;
        Biquad detLp, detHp;
        std::vector<float> delay[3];
    };

    Channel ch[maxChannels];
    Params params;
    double fs = 0;
    int chans = 2, bufLen = 8, lookSamples = 0, writePos = 0;
    float currentFreq = -1, lastSetFreq = -1;

    Envelope lowFast, lowSlow, hfEnv;
    float thresholdDb = 8, floorLin = 0.001f;
    float gr = 1, grAttack = 0, grRelease = 0, maxCutDb = 24, outGain = 1;
    int holdSamples = 0, holdCount = 0;

    Frame frame; int frameCount = 0, frameLen = 64;
};

} // namespace plosivore
