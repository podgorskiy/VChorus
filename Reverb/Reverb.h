#pragma once

#include "IPlug_include_in_plug_hdr.h"

const int kNumPresets = 1;

enum EParams
{
    kGain = 0,
    kDelay = 1,
    kVoices = 2,
    kDamper = 3,
    kMix = 4,
    kHFCut = 5,
    kNumParams
};

using namespace iplug;
using namespace igraphics;

class Reverb final : public Plugin
{
public:
  Reverb(const InstanceInfo& info);

#if IPLUG_DSP // http://bit.ly/2S64BDd
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
#endif
  WDL_TypedBuf<float> mBuffer;
  WDL_TypedBuf<float> mDelays;
  WDL_TypedBuf<float> mMagnitude;
  WDL_TypedBuf<float> mLowPass;

  uint32_t mWriteAddress = 0;
  uint32_t mDTSamples = 0;

  int m_old_delay_in_samples = 0;
  float m_old_dumper = 0;
} WDL_FIXALIGN;
