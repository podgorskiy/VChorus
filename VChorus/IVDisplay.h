#pragma once

#include "IControl.h"
#include "ISender.h"
#include "IPlugStructs.h"

BEGIN_IPLUG_NAMESPACE
BEGIN_IGRAPHICS_NAMESPACE

class IVDotsDisplayControl : public IControl
                     , public IVectorBase
{
public:
    IVDotsDisplayControl(const IRECT& bounds, const char* label = "", const IVStyle& style = DEFAULT_STYLE)
  : IControl(bounds)
  , IVectorBase(style)
  {
    AttachIControl(this, label);
  }
  
  void Draw(IGraphics& g) override
  {
    DrawBackground(g, mRECT);
    DrawWidget(g);
    DrawLabel(g);
    
    if(mStyle.drawFrame)
      g.DrawRect(GetColor(kFR), mWidgetBounds, &mBlend, mStyle.frameThickness);
  }

  void DrawWidget(IGraphics& g) override
  {
    // g.DrawHorizontalLine(GetColor(kSH), mWidgetBounds, 0.5, &mBlend, mStyle.frameThickness);
    
    IRECT r = mWidgetBounds.GetPadded(-mPadding);

    const float maxY = (r.H() / 2.f); // y +/- centre

    float xPerData = r.W() / (float) 255;

    int ndotsl = mBuf.vals[0][200 * 2 + 0];
    int ndotsr = mBuf.vals[0][200 * 2 + 0];
    uint8_t* pl = &mBuf.vals[0][0];
    uint8_t* pr = &mBuf.vals[0][ndotsl];

    auto color = IColor(255, 113, 249, 147);

    for (int c = 0; c < ndotsl; c++)
    {
        float xHi = pl[c] * xPerData;
        float yHi = ((c + 1.0f) / (ndotsl + 1) * 2.0 - 1.0) * maxY;

        g.DrawCircle(color, r.L + xHi, r.MH() - yHi, 2.0f, &mBlend);
    }
    for (int c = 0; c < ndotsr; c++)
    {
        float xHi = pr[c] * xPerData;
        float yHi = ((c + 1.0f) / (ndotsl + 1) * 2.0 - 1.0) * maxY;

        g.DrawCircle(color, r.L + xHi, r.MH() - yHi, 2.0f, &mBlend);
    }
  }
  
  void OnResize() override
  {
    SetTargetRECT(MakeRects(mRECT));
    SetDirty(false);
  }

  void OnMsgFromDelegate(int msgTag, int dataSize, const void* pData) override
  {
    if (!IsDisabled() && msgTag == ISender<>::kUpdateMessage)
    {
      IByteStream stream(pData, dataSize);


      int pos = 0;
      pos = stream.Get(&mBuf, pos);

      SetDirty(false);
    }
  }

private:
  ISenderData<1, std::array<uint8_t, 200 * 2 + 2> > mBuf;
  float mPadding = 2.f;
};

END_IGRAPHICS_NAMESPACE
END_IPLUG_NAMESPACE
