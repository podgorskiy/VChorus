#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "ISender.h"

const int kNumPresets = 1;

enum EParams
{
    kGain = 0,
    kDelay = 1,
    kVoices = 2,
    kDamper = 3,
    kMix = 4,
    kHFCut = 5,
    kMaxRate = 6,
    kRateUpdate = 7,
    kCtrlTagDisplay = 8,
    kNumParams
};

using namespace iplug;
using namespace igraphics;

class VChorus final : public Plugin
{
public:
  VChorus(const InstanceInfo& info);

#if IPLUG_DSP // http://bit.ly/2S64BDd
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
  void OnIdle() override;
#endif
  WDL_TypedBuf<float> mBuffer;
  WDL_TypedBuf<float> mDelays;
  WDL_TypedBuf<float> mRates;
  WDL_TypedBuf<float> mMagnitude;
  WDL_TypedBuf<float> mLowPass;

  uint32_t mWriteAddress = 0;
  uint32_t mDTSamples = 0;

  float time_since_last_update = 0;

  int m_old_delay_in_samples = 0;
  float m_old_dumper = 0;
  
  std::array<uint8_t, 200 * 2 + 2> dots;

  ISender<1, 3, std::array<uint8_t, 200 * 2 + 2> > mDisplaySender;
} WDL_FIXALIGN;
