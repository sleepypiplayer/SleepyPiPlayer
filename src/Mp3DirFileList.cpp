// ----------------------------------------------------------------------------
// SleepyPiPlayer:  Mp3DirFileList
//     create list of directories containing MP3-Files used for Playback
// license: free software   (sleepypiplayer(at)saftfresse.de)  [A.D.2025]
// - Do whatever you want with the software.
// - Do not claim my work as your own.
// - THIS SOFTWARE COMES WITH NO WARRANTIES, USE AT YOUR OWN RISK
// ----------------------------------------------------------------------------

#include "Mp3DirFileList.h"
#include <string>
#include <vector>
#include <list>
#include <filesystem>
#include <cctype>


// ============================================================================
class Mp3Directory
{
public:
   Mp3Directory() {}
   virtual ~Mp3Directory() {}
   void SetPlaybackPosFromStorage(std::string txtpathMp3File, double dPlaybackPos);

   void FindFiles();
   PlaybackInfo GetCurrentFile();
   PlaybackInfo GetNextFile();
   PlaybackInfo GetPrevFile();
   void UpdatePlaybackPos(PlaybackInfo info);
   void InvalidateFileIdx();
   void SetLastFile();
   void SetNofDirs(int nNofDirs)   { m_nNofDirs = nNofDirs; }

   int m_nIdxDir         = -1;   // index in Mp3DirFileList::PrivateData::m_vecDirectory
   int m_nIdxFile        = -1;   // index of current file in m_vecTxtFileName
   int m_nNofDirs        = -1;
   double m_dPosSeconds  = 0.0;  // current file playback position
   std::string m_txtDirPath;
   std::vector<std::string> m_vecTxtFileName;
};

// -----------------------------------------------------------------------------

void Mp3Directory::SetPlaybackPosFromStorage(std::string txtpathMp3File, double dPlaybackPos)
{
   const std::filesystem::path pathFile{txtpathMp3File};
   std::string txtFileName = pathFile.filename().string();
   for (uint64_t nFileIdx = 0; nFileIdx < m_vecTxtFileName.size(); nFileIdx++)
   {
      if (txtFileName == m_vecTxtFileName[nFileIdx])
      {
         m_nIdxFile    = nFileIdx;
         m_dPosSeconds = dPlaybackPos;
         break;
      }
   }
}

// -----------------------------------------------------------------------------

void Mp3Directory::FindFiles()
{
   std::string txtMp3Extension(".MP3");

   std::list<std::string>  listFiles;
   const std::filesystem::path pathDir{m_txtDirPath};
   for (auto const& dir_entry : std::filesystem::directory_iterator{pathDir})
   {
      if (dir_entry.is_regular_file())
      {
         std::string txtFileExtension = dir_entry.path().extension().string();
         for (uint32_t nCharIdx = 0; nCharIdx < txtFileExtension.length(); nCharIdx++)
         {
            txtFileExtension[nCharIdx] = std::toupper(txtFileExtension[nCharIdx]);
         }
         if (txtFileExtension == txtMp3Extension)
         {
            listFiles.push_back(dir_entry.path().filename().string());
         }
      }
   }

   listFiles.sort();
   m_vecTxtFileName.reserve(listFiles.size());
   for (std::string txtFileName : listFiles)
   {
      m_vecTxtFileName.push_back(txtFileName);
   }
}

// -----------------------------------------------------------------------------

void Mp3Directory::InvalidateFileIdx()
{
   m_nIdxFile    = -1;
   m_dPosSeconds = 0.0;
}

// -----------------------------------------------------------------------------

void Mp3Directory::UpdatePlaybackPos(PlaybackInfo info)
{
   if (m_nIdxDir == info.GetDirNumber()-1)
   {
      if (m_nIdxFile == info.GetFileNumber()-1)
      {
         m_dPosSeconds = info.GetPlaybackPos();
      }
   }
}

// -----------------------------------------------------------------------------

void Mp3Directory::SetLastFile()
{
   m_dPosSeconds = 0.0;
   m_nIdxFile    = -1;
   int nNofFiles = m_vecTxtFileName.size();
   if (nNofFiles > 0)
   {
      m_nIdxFile = nNofFiles -1;
   }
}

// -----------------------------------------------------------------------------

PlaybackInfo Mp3Directory::GetCurrentFile()
{
   int nNofFiles = m_vecTxtFileName.size();
   if (nNofFiles == 0)
   {
      return PlaybackInfo();
   }
   if (m_nIdxFile < 0)
   {
      m_nIdxFile    = 0;
      m_dPosSeconds = 0.0;
   }
   m_nIdxFile = (m_nIdxFile + nNofFiles) % nNofFiles;
   std::filesystem::path pathFile;
   if (m_txtDirPath.length() > 0)
   {
      pathFile = std::filesystem::path{m_txtDirPath};
      pathFile.append(m_vecTxtFileName[m_nIdxFile]);
   }
   else
   {
      pathFile = std::filesystem::path{m_vecTxtFileName[m_nIdxFile]};
   }
   return PlaybackInfo(pathFile.string(), m_dPosSeconds, m_nIdxFile, m_nIdxDir, nNofFiles, m_nNofDirs);
}

// -----------------------------------------------------------------------------

PlaybackInfo Mp3Directory::GetNextFile()
{
   int nNofFiles = m_vecTxtFileName.size();
   if (nNofFiles == 0)
   {
      return PlaybackInfo();
   }
   if (m_nIdxFile < 0)
   {
      m_dPosSeconds = 0.0;
      return GetCurrentFile();
   }
   m_nIdxFile += 1;
   m_dPosSeconds = 0.0;
   if (m_nIdxFile >= nNofFiles)
   {
      m_nIdxFile = 0;
      return PlaybackInfo();
   }
   return GetCurrentFile();
}

// -----------------------------------------------------------------------------

PlaybackInfo Mp3Directory::GetPrevFile()
{
   int nNofFiles = m_vecTxtFileName.size();
   if (nNofFiles == 0)
   {
      return PlaybackInfo();
   }
   if (m_nIdxFile < 0)
   {
      m_dPosSeconds = 0.0;
      return GetCurrentFile();
   }
   if (m_dPosSeconds > 59.0)
   {
      m_dPosSeconds = 0.0;
   }
   else
   {
      m_nIdxFile -= 1;
      m_dPosSeconds = 0.0;
      if (m_nIdxFile < 0)
      {
         m_nIdxFile = 0;
         return PlaybackInfo();
      }
   }
   return GetCurrentFile();
}


// ============================================================================
class Mp3DirFileList::PrivateData
{
public:
  PrivateData(const std::string& txtDirectoryPath)
  {
     m_txtRootPath = txtDirectoryPath;
     FillDirectories(txtDirectoryPath);
     for (Mp3Directory& directory : m_vecDirectory)
     {
        directory.SetNofDirs(m_vecDirectory.size());
     }
  }
  PrivateData(const std::list<std::string> listMp3Path)
  {
     Mp3Directory directory;
     directory.m_nIdxDir  = 0;
     directory.m_nNofDirs = 0;
     for (std::string txtMp3FilePath : listMp3Path)
     {
        directory.m_vecTxtFileName.push_back(txtMp3FilePath);
     }
     m_vecDirectory.push_back(directory);
  }
  ~PrivateData() {}

   int m_nIdxCurrentDir       = -1;  // index of current directory in m_vecDirectory
   std::string                m_txtRootPath;
   std::vector<Mp3Directory>  m_vecDirectory;

private:
  void FillDirectories(std::string txtDirPath);
};

// -----------------------------------------------------------------------------

void Mp3DirFileList::PrivateData::FillDirectories(std::string txtDirPath)
{
   std::string txtMp3Extension(".MP3");

   const std::filesystem::path pathRoot{txtDirPath};
   std::list<std::string> listSubDirs;

   bool bMp3Found = false;
   try
   {
      for (auto const& dir_entry : std::filesystem::directory_iterator{pathRoot})
      {
         if (dir_entry.is_directory())
         {
            listSubDirs.push_back(dir_entry.path().filename().string());
         }
         else if (dir_entry.is_regular_file())
         {
            std::string txtFileExtension = dir_entry.path().extension().string();
            for (uint32_t nCharIdx = 0; nCharIdx < txtFileExtension.length(); nCharIdx++)
            {
               txtFileExtension[nCharIdx] = std::toupper(txtFileExtension[nCharIdx]);
            }
            if (txtFileExtension == txtMp3Extension)
            {
               bMp3Found = true;
            }
         }
      }
   }
   catch(...)
   {
      bMp3Found = false;
      listSubDirs.clear();
   }

   if (bMp3Found)
   {
      m_vecDirectory.push_back(Mp3Directory());
      m_vecDirectory.back().m_txtDirPath = txtDirPath;
      m_vecDirectory.back().m_nIdxDir = m_vecDirectory.size() -1;
      m_vecDirectory.back().FindFiles();
   }

   listSubDirs.sort();
   for (std::string txtSubPath : listSubDirs)
   {
      std::filesystem::path pathSubDir = pathRoot;
      pathSubDir.append(txtSubPath);
      FillDirectories(pathSubDir.string());
   }
}

// ============================================================================

Mp3DirFileList::Mp3DirFileList(std::string txtPathMp3Directory)
{
   m_pPriv = std::make_unique<Mp3DirFileList::PrivateData>(txtPathMp3Directory);
}

// -----------------------------------------------------------------------------

Mp3DirFileList::Mp3DirFileList(std::list<std::string> listMp3Path)
{
   m_pPriv = std::make_unique<Mp3DirFileList::PrivateData>(listMp3Path);
}


// -----------------------------------------------------------------------------

Mp3DirFileList::~Mp3DirFileList()
{
}

// -----------------------------------------------------------------------------

void Mp3DirFileList::SetPlaybackPosFromStorage(
   std::string txtpathMp3File,
   double dPlaybackPos)
{
   std::filesystem::path pathFile{txtpathMp3File};
   std::filesystem::path pathDir = pathFile.parent_path();
   std::string txtPathDir = pathDir.string();
   for (uint64_t nDirIdx = 0; nDirIdx < m_pPriv->m_vecDirectory.size(); nDirIdx++)
   {
      if (txtPathDir == m_pPriv->m_vecDirectory[nDirIdx].m_txtDirPath)
      {
         m_pPriv->m_nIdxCurrentDir = nDirIdx;
         m_pPriv->m_vecDirectory[nDirIdx].SetPlaybackPosFromStorage(txtpathMp3File, dPlaybackPos);
         break;
      }
   }
}

// -----------------------------------------------------------------------------

void Mp3DirFileList::UpdatePlaybackPosByPlayer(const PlaybackInfo& info)
{
   int nIdxDir  = info.GetDirNumber()  -1;
   int nNofDirectories = m_pPriv->m_vecDirectory.size();

   if (nIdxDir >= 0 && nIdxDir < nNofDirectories)
   {
      m_pPriv->m_vecDirectory[nIdxDir].UpdatePlaybackPos(info);
   }
}

// -----------------------------------------------------------------------------

PlaybackInfo Mp3DirFileList::GetCurrentFile()
{
   int nVecDirSize = m_pPriv->m_vecDirectory.size();
   if (nVecDirSize == 0)
   {
      return PlaybackInfo();
   }

   if (m_pPriv->m_nIdxCurrentDir < 0)
   {
      m_pPriv->m_nIdxCurrentDir = 0;
   }
   m_pPriv->m_nIdxCurrentDir =
     (m_pPriv->m_nIdxCurrentDir + nVecDirSize) % nVecDirSize;

   return m_pPriv->m_vecDirectory[m_pPriv->m_nIdxCurrentDir].GetCurrentFile();
}

// -----------------------------------------------------------------------------

void Mp3DirFileList::SetNextFile()
{
   PlaybackInfo infoCurrent = GetCurrentFile();
   if ( (! infoCurrent.IsValid()) || (infoCurrent.GetFileNumber() <= 0) || (infoCurrent.GetDirNumber() <= 0))
   {
      return;
   }

   int nIdxDir = infoCurrent.GetDirNumber()-1;
   infoCurrent = m_pPriv->m_vecDirectory[nIdxDir].GetNextFile();
   if (! infoCurrent.IsValid())
   {
      nIdxDir = (nIdxDir + 1) % m_pPriv->m_vecDirectory.size();
      m_pPriv->m_vecDirectory[nIdxDir].InvalidateFileIdx();
      infoCurrent = m_pPriv->m_vecDirectory[nIdxDir].GetCurrentFile();
   }
   if (infoCurrent.IsValid())
   {
      m_pPriv->m_nIdxCurrentDir = nIdxDir;
   }
}

// -----------------------------------------------------------------------------

void Mp3DirFileList::SetPrevFile()
{
   PlaybackInfo infoCurrent = GetCurrentFile();
   if ( (! infoCurrent.IsValid()) || (infoCurrent.GetFileNumber() <= 0) || (infoCurrent.GetDirNumber() <= 0))
   {
      return;
   }

   int nIdxDir = infoCurrent.GetDirNumber()-1;
   infoCurrent = m_pPriv->m_vecDirectory[nIdxDir].GetPrevFile();
   if (! infoCurrent.IsValid())
   {
      nIdxDir = (nIdxDir + m_pPriv->m_vecDirectory.size() - 1) % m_pPriv->m_vecDirectory.size();
      m_pPriv->m_vecDirectory[nIdxDir].SetLastFile();
      infoCurrent = m_pPriv->m_vecDirectory[nIdxDir].GetCurrentFile();
   }
   if (infoCurrent.IsValid())
   {
      m_pPriv->m_nIdxCurrentDir = nIdxDir;
   }
}

// -----------------------------------------------------------------------------

void Mp3DirFileList::SetNextDir()
{
   int nVecDirSize = m_pPriv->m_vecDirectory.size();

   if (nVecDirSize == 0)
   {
      return;
   }

   if (m_pPriv->m_nIdxCurrentDir < 0)
   {
      m_pPriv->m_nIdxCurrentDir = 0;
   }
   else
   {
      m_pPriv->m_nIdxCurrentDir = (m_pPriv->m_nIdxCurrentDir + 1) % nVecDirSize;
   }
}

// -----------------------------------------------------------------------------

void Mp3DirFileList::SetPrevDir()
{
   int nVecDirSize = m_pPriv->m_vecDirectory.size();

   if (nVecDirSize == 0)
   {
      return;
   }

   if (m_pPriv->m_nIdxCurrentDir < 0)
   {
      m_pPriv->m_nIdxCurrentDir = 0;
   }
   else
   {
      m_pPriv->m_nIdxCurrentDir = (m_pPriv->m_nIdxCurrentDir + nVecDirSize - 1) % nVecDirSize;
   }
}
