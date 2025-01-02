// ----------------------------------------------------------------------------
// SleepyPiPlayer:  Mp3DirFileList
//     create list of directories containing MP3-Files used for Playback
// ----------------------------------------------------------------------------

#pragma once
#include "PlaybackInfo.h"
#include <memory>
#include <list>

/** list of directories containing MP3-Files */
class Mp3DirFileList
{
public:
   // search in a directory for mp3-files
   Mp3DirFileList(std::string txtPathMp3Directory);
   // use this list of mp3-files
   Mp3DirFileList(std::list<std::string> listMp3Path);
   virtual ~Mp3DirFileList();
   void SetPlaybackPosFromStorage(std::string txtpathMp3File, double dPlaybackPos);

   PlaybackInfo GetCurrentFile();
   void UpdatePlaybackPosByPlayer(const PlaybackInfo& info);

   void SetNextFile();
   void SetPrevFile();
   void SetNextDir();
   void SetPrevDir();


private:
   class PrivateData;
   std::unique_ptr<PrivateData> m_pPriv;
};
