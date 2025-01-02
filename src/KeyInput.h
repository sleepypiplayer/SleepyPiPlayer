// ----------------------------------------------------------------------------
// SleepyPiPlayer:  KeyInput
//     poll keys
// ----------------------------------------------------------------------------
#pragma once
#include <list>
#include <memory>

/** Poll for Keys  */
class KeyInput
{
public:
   KeyInput();
   virtual ~KeyInput();

   enum KEY
   {
      KEY_SimulatedAutoShutdown,  // PC-version only
      KEY_NONE,      // no key was pressed: stops FastForward
      KEY_ANY,       // e.g. Service-key: cancel auto-shutdown even if combination is invalid
      KEY_VolumeInc,
      KEY_VolumeDec,
      KEY_FileNext,
      KEY_FilePrev,
      KEY_FileFastForw,
      KEY_FileFastBack,
      KEY_PlayFastForw,
      KEY_PlayFastBack,
      KEY_DirNext,
      KEY_DirPrev,
      KEY_Info,
      KEY_Shutdown,
      KEY_Service
   };

   std::list<KEY> GetInputKeys();

   // if blue button is pressed on startup: do not turn off wifi
   bool IsServiceKeyPressed();

private:
   class PrivateData;
   std::unique_ptr<PrivateData> m_pPriv;
};

