// ----------------------------------------------------------------------------
// SleepyPiPlayer:  AudioVolumeConversion
//     human audio-perception is logarithmic
//     convert value in percent to value suitable for
//     mpg123_volume(handle, ConvertPctToDouble(nValueInPercent))
// ----------------------------------------------------------------------------
#pragma once
#include <memory>

class AudioVolumeConversion
{
public:
   AudioVolumeConversion();
   virtual ~AudioVolumeConversion();

   // convert linear volume in percent to mpg123_volume(...)
   double ConvertPercentToDouble(int nVolumePct);

private:
   class PrivateData;
   std::unique_ptr<PrivateData> m_pPriv;
};
