// ----------------------------------------------------------------------------
// SleepyPiPlayer:  AudioPlayer
//     play mp3-files
//     handle key-press
//     check time for auto-shutdown
// ----------------------------------------------------------------------------
#pragma once
#include "Mp3DirFileList.h"
#include "KeyInput.h"


/** play mp3-files */
class AudioPlayer
{
public:
   AudioPlayer(int nVolumePct, Mp3DirFileList* pFiles, int nAutoShutdownMinutes, bool bChecksumProblem);
   virtual ~AudioPlayer();

   void AddUserRequest(KeyInput::KEY  key);
   int          GetVolumeInPct();          // for PermanentStorage
   PlaybackInfo GetCurrentPlaybackInfo();  // for PermanentStorage
   bool  IsShutdown();                     // OnOff-key pressed / auto-shutdown countdown was not canceled
   bool  IsServiceMode();                  // output of "service" finished aufter service-key-press

private:
   class PrivateData;
   std::unique_ptr<PrivateData> m_pPriv;
};
