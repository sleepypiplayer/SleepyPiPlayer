// ----------------------------------------------------------------------------
// SleepyPiPlayer:  AudioVolumeConversion
//     human audio-perception is logarithmic
//     convert value in percent to value suitable for
//     mpg123_volume(handle, ConvertPctToDouble(nValueInPercent))
// license: free software   (sleepypiplayer(at)saftfresse.de)  [A.D.2025]
// - Do whatever you want with the software.
// - Do not claim my work as your own.
// - THIS SOFTWARE COMES WITH NO WARRANTIES, USE AT YOUR OWN RISK
// ----------------------------------------------------------------------------
#include "AudioVolumeConversion.h"

// ============================================================================
class AudioVolumeConversion::PrivateData
{
public:
   PrivateData()
   {
      double dVolumeVal = 1.5; // arr[100] = 1.5  arr[20] = 0.1
      for (int i = 100; i >= 0; i--)
      {
         m_arrVolume[i] = dVolumeVal;
         dVolumeVal *= 0.966715;  // 10^(log10(1.5/0.1)/80)
      }
   }
   virtual ~PrivateData()
   {
   }

   double   m_arrVolume[101];
};


// ============================================================================

AudioVolumeConversion::AudioVolumeConversion()
{
   m_pPriv = std::make_unique<AudioVolumeConversion::PrivateData>();
}

// ----------------------------------------------------------------------------

AudioVolumeConversion::~AudioVolumeConversion()
{
}

// ----------------------------------------------------------------------------

/** convert linear volume in percent to mpg123_volume(...) */
double AudioVolumeConversion::ConvertPercentToDouble(int nVolumePct)
{
   nVolumePct = std::min(100, nVolumePct);
   nVolumePct = std::max(0,   nVolumePct);
   return m_pPriv->m_arrVolume[nVolumePct];
}
