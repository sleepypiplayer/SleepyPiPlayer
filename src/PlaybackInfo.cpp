// ----------------------------------------------------------------------------
// SleepyPiPlayer:  PlaybackInfo
//     Mp3-Filename + PlaybackPosition in seconds
// license: free software   (sleepypiplayer(at)saftfresse.de)  [A.D.2025]
// - Do whatever you want with the software.
// - Do not claim my work as your own.
// - THIS SOFTWARE COMES WITH NO WARRANTIES, USE AT YOUR OWN RISK
// ----------------------------------------------------------------------------
#include "PlaybackInfo.h"
#include <filesystem>


// ============================================================================
class PlaybackInfo::PrivateData
{
public:
  PrivateData()
  {
  }
  ~PrivateData() {}

  double        m_dPlaybackPos = 0.0; // in seconds
  std::string   m_txtMp3Path;
  int           m_nIdxDirectory = -1;
  int           m_nIdxFile      = -1;
  int           m_nNofDirs      = -1;
  int           m_nNofFiles     = -1;
};

// ============================================================================

PlaybackInfo::PlaybackInfo()
{
   m_pPriv = std::make_unique<PlaybackInfo::PrivateData>();
}

// -----------------------------------------------------------------------------

PlaybackInfo::PlaybackInfo(const PlaybackInfo& other)
{
   this->m_pPriv = std::make_unique<PlaybackInfo::PrivateData>();
   *this = other;
}

// -----------------------------------------------------------------------------

PlaybackInfo& PlaybackInfo::operator=(const PlaybackInfo& other)
{
   if (this != &other)
   {
      this->m_pPriv->m_dPlaybackPos     = other.m_pPriv->m_dPlaybackPos;
      this->m_pPriv->m_txtMp3Path       = other.m_pPriv->m_txtMp3Path;
      this->m_pPriv->m_nIdxDirectory    = other.m_pPriv->m_nIdxDirectory;
      this->m_pPriv->m_nIdxFile         = other.m_pPriv->m_nIdxFile;
      this->m_pPriv->m_nNofDirs         = other.m_pPriv->m_nNofDirs;
      this->m_pPriv->m_nNofFiles        = other.m_pPriv->m_nNofFiles;
   }
   return *this;
}

// -----------------------------------------------------------------------------

PlaybackInfo::PlaybackInfo(std::string txtFilePath)
{
   m_pPriv = std::make_unique<PlaybackInfo::PrivateData>();
   m_pPriv->m_txtMp3Path = txtFilePath;
}

// -----------------------------------------------------------------------------

PlaybackInfo::PlaybackInfo(std::string txtFilePath, double dSeconds, int nIdxFile, int nIdxDir, int nNofFiles, int nNofDirs)
{
   m_pPriv = std::make_unique<PlaybackInfo::PrivateData>();
   m_pPriv->m_txtMp3Path       = txtFilePath;
   m_pPriv->m_dPlaybackPos     = dSeconds;
   m_pPriv->m_nIdxFile         = nIdxFile;
   m_pPriv->m_nIdxDirectory    = nIdxDir;
   m_pPriv->m_nNofFiles        = nNofFiles;
   m_pPriv->m_nNofDirs         = nNofDirs;
}

// -----------------------------------------------------------------------------

PlaybackInfo::~PlaybackInfo()
{
}

// -----------------------------------------------------------------------------

void PlaybackInfo::SetFilePath(std::string txtFilePath)
{
   m_pPriv->m_txtMp3Path = txtFilePath;
}

// -----------------------------------------------------------------------------

std::string PlaybackInfo::GetFilePath() const
{
   return m_pPriv->m_txtMp3Path;
}

// -----------------------------------------------------------------------------

std::string PlaybackInfo::GetFileName() const
{
   std::filesystem::path pathFile{GetFilePath()};
   return pathFile.filename().string();
}

// -----------------------------------------------------------------------------

std::string PlaybackInfo::GetDirName() const
{
   std::filesystem::path pathFile{GetFilePath()};
   return pathFile.parent_path().filename().string();
}

// -----------------------------------------------------------------------------

void   PlaybackInfo::SetPlaybackPos(double dSeconds)
{
   m_pPriv->m_dPlaybackPos = dSeconds;
}

// -----------------------------------------------------------------------------

double PlaybackInfo::GetPlaybackPos() const  // in seconds
{
   double dPos = m_pPriv->m_dPlaybackPos;
   dPos = std::max(0.0, dPos);
   return dPos;

}

// -----------------------------------------------------------------------------

bool PlaybackInfo::IsValid() const
{
   return (GetFilePath().length() > 0);
}

// -----------------------------------------------------------------------------

int  PlaybackInfo::GetFileNumber() const // 1 .. NofFilesInDirectory
{
   return (m_pPriv->m_nIdxFile < 0)? -1 : m_pPriv->m_nIdxFile+1;
}

// -----------------------------------------------------------------------------

int  PlaybackInfo::GetDirNumber() const  // 1 .. NumberOfDirectories
{
   return (m_pPriv->m_nIdxDirectory < 0)? -1 : m_pPriv->m_nIdxDirectory+1;
}

// -----------------------------------------------------------------------------

int  PlaybackInfo::GetNofFiles() const   // number of files in a directory
{
   return m_pPriv->m_nNofFiles;
}

// -----------------------------------------------------------------------------

int  PlaybackInfo::GetNofDirs() const    // number of directories
{
   return m_pPriv->m_nNofDirs;
}
