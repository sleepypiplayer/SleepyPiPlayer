// ----------------------------------------------------------------------------
// SleepyPiPlayer:  PlaybackInfo
//     Mp3-Filename + PlaybackPosition in seconds
// ----------------------------------------------------------------------------
#pragma once
#include <string>
#include <memory>

/** MP3-file-path + position in seconds */
class PlaybackInfo
{
public:
   PlaybackInfo();
   PlaybackInfo(const PlaybackInfo&);
   PlaybackInfo& operator=(const PlaybackInfo&);
   PlaybackInfo(std::string txtFilePath);
   PlaybackInfo(std::string txtFilePath, double dSeconds, int nIdxFile, int nIdxDir, int nNofFiles, int nNofDirs);
   virtual ~PlaybackInfo();

   bool IsValid() const;
   int  GetFileNumber() const;  // 1 .. NofFilesInDirectory
   int  GetDirNumber() const;   // 1 .. NumberOfDirectories
   int  GetNofFiles() const;    // number of files in a directory
   int  GetNofDirs() const;     // number of directories

   void        SetFilePath(std::string txtFilePath);
   std::string GetFilePath() const;
   std::string GetFileName() const;
   std::string GetDirName() const;

   void   SetPlaybackPos(double dSeconds);
   double GetPlaybackPos() const;


private:
   class PrivateData;
   std::unique_ptr<PrivateData> m_pPriv;
};

