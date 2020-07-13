#include "Reverb.h"
#include "IPlug_include_in_plug_src.h"
#include "IControls.h"


#include "Oscillator.h"
#include "ADSREnvelope.h"
#include "Smoothers.h"
#include "LFO.h"
#include <math.h>

#include <iostream>

#define _OPENMP

#if defined(_OPENMP)
#define T4_USE_OMP 1
#else
#define T4_USE_OMP 0
#endif

#if T4_USE_OMP
#include <omp.h>
#endif

#ifdef _MSC_VER
#define T4_Pragma(X) __pragma(X)
#else
#define T4_Pragma(X) _Pragma(#X)
#endif

#if T4_USE_OMP
#define OMP_THREAD_ID omp_get_thread_num()
#define OMP_MAX_THREADS omp_get_max_threads()
#define parallel_for T4_Pragma(omp parallel for) for
#else
#define OMP_THREAD_ID 0
#define OMP_MAX_THREADS 1
#define parallel_for for
#endif


void CreateConsole()
{
#ifdef WIN32
    static bool already_created = false;
    if (already_created)
        return;
    already_created = true;

    if (!AttachConsole(ATTACH_PARENT_PROCESS))
    {
        AllocConsole();
    }

    freopen("CONIN$", "r", stdin);
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);

    std::wcout.clear();
    std::cout.clear();
    std::wcerr.clear();
    std::cerr.clear();
    std::wcin.clear();
    std::cin.clear();

    std::ios::sync_with_stdio();
#endif
}


Reverb::Reverb(const InstanceInfo& info)
: Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
    // CreateConsole();

    GetParam(kGain)->InitGain("Gain");
    GetParam(kDelay)->InitDoubleExp("Delay", 20., 1.0, 1000.0, 0.1);
    GetParam(kVoices)->InitDouble("Voices", 50, 1, 200.0, 0.1);
    GetParam(kDamper)->InitDouble("Damper", 0.0, 0.0, 10.0, 0.1);
    GetParam(kMix)->InitPercentage("Mix", 100.0);
    GetParam(kHFCut)->InitPercentage("High cut", 100.0);
    GetParam(kMaxRate)->InitDouble("MaxRate", 330.0, 0.0, 1000.0, 0.1, "ms");
    GetParam(kRateUpdate)->InitDouble("Rate Update", 1000.0, 0.0, 2000.0, 0.1, "ms");


#if IPLUG_EDITOR // http://bit.ly/2S64BDd
    mMakeGraphicsFunc = [&]() {
        return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_HEIGHT));
    };

    mLayoutFunc = [&](IGraphics* pGraphics) {
        pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
        pGraphics->AttachPanelBackground(COLOR_GRAY);
        pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
        const IRECT b = pGraphics->GetBounds();
        // pGraphics->AttachControl(new ITextControl(b.GetMidVPadded(50), "Hello iPlug 2!", IText(50)));
        pGraphics->AttachControl(new IVKnobControl(b.GetCentredInside(100).GetHShifted(400), kGain));
        pGraphics->AttachControl(new IVKnobControl(b.GetCentredInside(100).GetHShifted(-100), kDelay));
        pGraphics->AttachControl(new IVKnobControl(b.GetCentredInside(100).GetHShifted(0), kVoices));
        pGraphics->AttachControl(new IVKnobControl(b.GetCentredInside(100).GetHShifted(100), kDamper));
        pGraphics->AttachControl(new IVKnobControl(b.GetCentredInside(100).GetHShifted(200), kMix));
        pGraphics->AttachControl(new IVKnobControl(b.GetCentredInside(100).GetHShifted(300), kHFCut));
        pGraphics->AttachControl(new IVKnobControl(b.GetCentredInside(100).GetHShifted(-200), kMaxRate));
        pGraphics->AttachControl(new IVKnobControl(b.GetCentredInside(100).GetHShifted(-300), kRateUpdate));
    };
#endif
}

inline float randf()
{
    return ((rand() % RAND_MAX) * 1.0f / RAND_MAX);
}

inline float srandf()
{
    return randf() * 2.0f - 1.0f;
}

#if IPLUG_DSP
void Reverb::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
    const float gain = pow(2.0, GetParam(kGain)->Value() / 6.0);
    const float delay_ms = GetParam(kDelay)->Value();
    const int voices_count = GetParam(kVoices)->Value();
    const float damper = GetParam(kDamper)->Value();
    const float mix = GetParam(kMix)->Value() / 100.0;
    const float hf = GetParam(kHFCut)->Value() / 100.0;
    const int nChans = NOutChansConnected();
    const int delay_in_samples = GetSampleRate() * delay_ms / 1000;
    const float max_delay_rate_samples_per_samples = GetParam(kMaxRate)->Value() / 1000;
    const float update_time = GetParam(kRateUpdate)->Value();

    time_since_last_update += nFrames / GetSampleRate() * 1000.0;


    int buffer_size = 0;
    {
        uint64_t v = delay_in_samples + 1024;
        v--;
        v |= v >> 1U;
        v |= v >> 2U;
        v |= v >> 4U;
        v |= v >> 8U;
        v |= v >> 16U;
        v |= v >> 32U;
        v++;
        buffer_size = static_cast<size_t>(v);
    }

    bool need_resize = false;

    if (nChans * voices_count != mDelays.GetSize() || m_old_delay_in_samples != delay_in_samples || m_old_dumper != damper)
    {
        need_resize = nChans * voices_count != mDelays.GetSize();

        m_old_delay_in_samples = delay_in_samples;
        m_old_dumper = damper;

        if (need_resize)
        {
            mDelays.Resize(nChans * voices_count);
            mMagnitude.Resize(nChans * voices_count);
            mLowPass.Resize(nChans * voices_count);
            mRates.Resize(nChans * voices_count);
            mLowPass.Set(nullptr, nChans * voices_count);
        }

        auto* delays = mDelays.Get();
        auto* magnitude = mMagnitude.Get();
        auto* rates = mRates.Get();

        for (int i = 0; i < nChans * voices_count; ++i)
        {
            if (delay_in_samples != 0)
            {
                delays[i] = randf() * delay_in_samples;
                rates[i] = srandf() * max_delay_rate_samples_per_samples;
                magnitude[i] = randf() / powf(2.0f, delays[i] / delay_ms * damper);
            }
            else
            {
                delays[i] = 0;
                rates[i] = 0;
                magnitude[i] = .5;
            }
        }
    }

    if (time_since_last_update > update_time)
    {
        time_since_last_update = 0;

        auto* rates = mRates.Get();

        for (int i = 0; i < nChans * voices_count; ++i)
        {
            if (delay_in_samples != 0)
            {
                rates[i] = srandf() * max_delay_rate_samples_per_samples;
            }
            else
            {
                rates[i] = 0;
            }
        }
    }

    if (mBuffer.GetSize() != buffer_size * nChans)
    {
        mBuffer.Resize(buffer_size * nChans);
        mBuffer.Set(nullptr, buffer_size * nChans);
    }

    auto* buffer = mBuffer.Get();
    auto* delays = mDelays.Get();
    const auto* magnitude = mMagnitude.Get();
    auto* low_pass = mLowPass.Get();
    const auto* rates = mRates.Get();

    for (int s = 0; s < nFrames; s++) {
        mWriteAddress %= buffer_size;

        //#pragma omp parallel for
        for (int c = 0; c < nChans; c++) {
            auto input = inputs[c][s];
            const int offset = c * buffer_size;

            buffer[offset + mWriteAddress] = input;

            float out = 0.0;

            for(int j = 0; j < voices_count; ++j)
            {
                int v = j + voices_count * c;
                float lp = low_pass[v];
                float mag = magnitude[v];
                float delta = delays[v];
                delta += rates[v];

                if (delta < 0)
                {
                    delta += delay_in_samples;
                }
                else if (delta >= delay_in_samples)
                {
                    delta -= delay_in_samples;
                }

                int32_t readAddress = mWriteAddress - int(delta);
                readAddress = (readAddress + buffer_size * 100) % buffer_size;

                float input = buffer[offset + readAddress] * mag;

                lp = lp * (1.0 - hf * 0.99) + input * hf * 0.99;
                out += lp;

                low_pass[v] = lp;
                delays[v] = delta;
            }

            outputs[c][s] = gain * (out * mix + input * (1.0 - mix)) ;
        }

        mWriteAddress++;
    }
}
#endif
