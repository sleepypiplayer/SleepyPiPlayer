// ----------------------------------------------------------------------------
// SleepyPiPlayer:  SystemConfigFile
//  file in the executable directory with name "sleepy.cfg"  example:
//    # all characters after a hash will be ignored
//    # only lines starting with a key-word are processed
//    MP3_PATH                /SLEEPY_MP3/mp3              # files used for playback
//    STORAGE_DIR             /SLEEPY_SAVE/save            # 100 x 4MB-files containing playback-position and volume
//    SYSTEM_SOUND_PATH       /SLEEPY_FW/sleepy/SysMP3/german  # files used for audio-feedback
//    NUMBER_FORMAT           ENGLISH                      # "ENGLISH" or "GERMAN"   e.g. say "sieben-und-zwanzig" or "twenty-seven"
//    DISABLE_WIFI            ON                           # "ON" or "OFF"  default=ON  save battery by disabling wifi (Service-mode can still enable WIFI)
//    AUTO_SHUTDOWN           25                           # time in minutes (-1: no shutdown)
//    SRV_KEY_CHG_DIR         OFF                          # "ON" or "OFF"  default=OFF  single-press of blue service-key: change directory
//    WLAN_ACCESSPOINT_MODE   ON                           # ON: AccessPoint OFF: Wlan-Client
//    WLAN_SSID               SleepyPiPlayer               # WLAN-Name
//    WLAN_PASSPHRASE         Sleepy-MP3                   # WLAN-Password
//    WLAN_COUNTRY            DE                           # WLAN country-code https://en.wikipedia.org/wiki/ISO_3166-1_alpha-2
//    SERVICE_REMOUNT_WRITE   /SLEEPY_MP3;/SLEEPY_SAVE     # Service-Mode remount these partitions with write-acccess
// license: free software   (sleepypiplayer(at)saftfresse.de)  [A.D.2026]
// - Do whatever you want with the software.
// - Do not claim my work as your own.
// - THIS SOFTWARE COMES WITH NO WARRANTIES, USE AT YOUR OWN RISK
// ----------------------------------------------------------------------------
#include "SystemConfigFile.h"
#include <filesystem>
#include <fstream>
#include <cstring>
#include <string>
#include <mutex>

// ============================================================================
// provide path to SystemSoundCatalog without passing it as parameter
class StaticSystemConfigSysFilePath
{
public:
   StaticSystemConfigSysFilePath()
   {
      m_txtSystemSoundPath = std::string("/SLEEPY_FW/sleepy/SysMP3/english");
   }
   void SetPath(std::string txtSystemSoundPath)
   {
      std::lock_guard<std::mutex> guard(m_Mutex);
      m_txtSystemSoundPath = txtSystemSoundPath;
   }

   std::string GetPath()
   {
      std::lock_guard<std::mutex> guard(m_Mutex);
      return m_txtSystemSoundPath;
   }

private:
   std::mutex   m_Mutex;
   std::string  m_txtSystemSoundPath;
};

// ============================================================================
// provide NumberToSound with format without passing it as parameter
class StaticSystemConfigNumberFormat
{
public:
   void SetFormat(SystemConfigFile::NumberFormat eNumberFormat)
   {
      std::lock_guard<std::mutex> guard(m_Mutex);
      m_eNumberFormat = eNumberFormat;
   }

   SystemConfigFile::NumberFormat GetFormat()
   {
      std::lock_guard<std::mutex> guard(m_Mutex);
      return m_eNumberFormat;
   }

private:
   std::mutex   m_Mutex;
   SystemConfigFile::NumberFormat  m_eNumberFormat = SystemConfigFile::NumberFormat::ENGLISH;
};

// ============================================================================
class SystemConfigFile::PrivateData
{
public:
  PrivateData(std::string txtDirectoryPath)
  {
     std::string txtFileName = std::string("sleepy.cfg");

     std::filesystem::path pathConfigAbsolute = txtDirectoryPath;
     pathConfigAbsolute += txtFileName;

     if (! std::filesystem::exists(pathConfigAbsolute))
     {
         pathConfigAbsolute = std::filesystem::current_path();
         pathConfigAbsolute += txtFileName;
     }

     if (std::filesystem::exists(pathConfigAbsolute))
     {
        ReadConfigFile(pathConfigAbsolute);
     }
  }
  ~PrivateData() {}

  void ReadConfigFile(std::filesystem::path);
  void DecodeLine(const char* txtLine);
  void TrimString(std::string&  text);
  bool StartsWith(const std::string&  txtLong, const char* txtShort);
  
  int         m_nAutoShutdownMinutes = 25;
  std::string m_txtMp3Path;
  std::string m_txtStorageDirPath;
  bool        m_bWifiOff = false;  // disable wifi
  bool        m_bAllowNextDirByServiceKey = false;
  bool        m_bWlanAccessPointMode = true;
  std::string m_txtWlanSSID = "SleepyPiPlayer";
  std::string m_txtWlanPassphrase = "123123123";
  std::string m_txtWlanCountry = "DE";
  std::list<std::string> m_listRemountReadWrite;

  static StaticSystemConfigSysFilePath  m_SystemSoundPath;
  static StaticSystemConfigNumberFormat m_NumberFormat;
};
StaticSystemConfigSysFilePath  SystemConfigFile::PrivateData::m_SystemSoundPath;
StaticSystemConfigNumberFormat SystemConfigFile::PrivateData::m_NumberFormat;


// -----------------------------------------------------------------------------

void SystemConfigFile::PrivateData::ReadConfigFile(std::filesystem::path pathConfigAbsolute)
{
   std::ifstream fileStream(pathConfigAbsolute, std::ios_base::in );

   char txtLineData[300] = {};
   while (fileStream.getline(txtLineData, 299, '\n'))
   {
      DecodeLine(txtLineData);
      for (int i = 0; i < 300; i++) txtLineData[i] = 0;
   }
}

// -----------------------------------------------------------------------------
// remove  heading/trailing  Tab/Space/CarriageReturn/LineFeed
void SystemConfigFile::PrivateData::TrimString(std::string&  text)
{
   while (!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\n' || text.front() == '\r'))
   {
      text.erase(0, 1);
   }
   while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\n' || text.back() == '\r'))
   {
      text.pop_back();
   }
}

// -----------------------------------------------------------------------------
// true if txtLong starts with txtShort
bool SystemConfigFile::PrivateData::StartsWith(const std::string&  txtLong, const char* txtShort)
{
   bool bStartsWith = false;
   uint32_t nLengthShort = strlen(txtShort);
   if ( nLengthShort <= txtLong.length() )
   {
      bStartsWith = true;

      for(uint32_t i = 0; i < nLengthShort; i++)
      {
         if (txtLong.at(i) != txtShort[i])
         {
            bStartsWith = false;
            break;
         }
      }
   }

   return bStartsWith;
}

// -----------------------------------------------------------------------------
// search for keywords and store data
void SystemConfigFile::PrivateData::DecodeLine(const char* txtRawLine)
{
   const char* txtMp3Path   = "MP3_PATH";
   const char* txtSysPath   = "SYSTEM_SOUND_PATH";
   const char* txtNumFormat = "NUMBER_FORMAT";
   const char* txtShutdown  = "AUTO_SHUTDOWN";
   const char* txtWIFI_OFF  = "DISABLE_WIFI";
   const char* txtStorage   = "STORAGE_DIR";
   const char* txtServDir   = "SRV_KEY_CHG_DIR";
   const char* txtWlanAP    = "WLAN_ACCESSPOINT_MODE";
   const char* txtWlanSSID  = "WLAN_SSID";
   const char* txtWlanPass  = "WLAN_PASSPHRASE";
   const char* txtWlanCntry = "WLAN_COUNTRY";
   const char* txtRemountRW = "SERVICE_REMOUNT_WRITE";

   std::string txtLine(txtRawLine);
   if (txtLine.find('#') != std::string::npos)
   {
      txtLine.erase(txtLine.find('#'), std::string::npos);
   }
   TrimString(txtLine);

   if (StartsWith(txtLine, txtMp3Path))
   {
      txtLine.erase(0, strlen(txtMp3Path));
      TrimString(txtLine);
      m_txtMp3Path = txtLine;
   }
   if (StartsWith(txtLine, txtSysPath))
   {
      txtLine.erase(0, strlen(txtSysPath));
      TrimString(txtLine);
      m_SystemSoundPath.SetPath( txtLine );
   }
   if (StartsWith(txtLine, txtNumFormat))
   {
      txtLine.erase(0, strlen(txtNumFormat));
      TrimString(txtLine);
      if (txtLine.at(0) == 'g' || txtLine.at(0) == 'G')  // German
      {
         m_NumberFormat.SetFormat(SystemConfigFile::NumberFormat::GERMAN);
      }
      else if (txtLine.at(0) == 'd' || txtLine.at(0) == 'D') // Deutsch
      {
         m_NumberFormat.SetFormat(SystemConfigFile::NumberFormat::GERMAN);
      }
      else
      {
         m_NumberFormat.SetFormat(SystemConfigFile::NumberFormat::ENGLISH);
      }
   }
   if (StartsWith(txtLine, txtShutdown))
   {
      txtLine.erase(0, strlen(txtShutdown));
      TrimString(txtLine);
      try
      {
         m_nAutoShutdownMinutes = std::stoi(txtLine);
      }
      catch(...)
      {
         m_nAutoShutdownMinutes = 25;
      }
   }
   if (StartsWith(txtLine, txtWIFI_OFF))
   {
      m_bWifiOff = false;
      txtLine.erase(0, strlen(txtWIFI_OFF));
      TrimString(txtLine);
      if (txtLine.length() == 2)
      {
         bool bDisable = true;
         bDisable = (bDisable && (txtLine.at(0) == 'o' || txtLine.at(0) == 'O'));
         bDisable = (bDisable && (txtLine.at(1) == 'n' || txtLine.at(1) == 'N'));
         if (bDisable)
         {
            m_bWifiOff = true;
         }
      }
   }
  if (StartsWith(txtLine, txtServDir))
  {
      m_bAllowNextDirByServiceKey = false;
      txtLine.erase(0, strlen(txtServDir));
      TrimString(txtLine);
      if (txtLine.length() == 2)
      {
         bool bAllow = true;
         bAllow = (bAllow && (txtLine.at(0) == 'o' || txtLine.at(0) == 'O'));
         bAllow = (bAllow && (txtLine.at(1) == 'n' || txtLine.at(1) == 'N'));
         if (bAllow)
         {
            m_bAllowNextDirByServiceKey = true;
         }
      }
   }
   if (StartsWith(txtLine, txtStorage))
   {
      txtLine.erase(0, strlen(txtStorage));
      TrimString(txtLine);
      m_txtStorageDirPath = txtLine;
   }
   if (StartsWith(txtLine, txtWlanAP))
   {
      m_bWlanAccessPointMode = false;
      txtLine.erase(0, strlen(txtWlanAP));
      TrimString(txtLine);
      if (txtLine.length() == 2)
      {
         bool bEnable = true;
         bEnable = (bEnable && (txtLine.at(0) == 'o' || txtLine.at(0) == 'O'));
         bEnable = (bEnable && (txtLine.at(1) == 'n' || txtLine.at(1) == 'N'));
         if (bEnable)
         {
            m_bWlanAccessPointMode = true;
         }
      }
   }
   if (StartsWith(txtLine, txtWlanSSID))
   {
      txtLine.erase(0, strlen(txtWlanSSID));
      TrimString(txtLine);
      m_txtWlanSSID = txtLine;
   }
   if (StartsWith(txtLine, txtWlanPass))
   {
      txtLine.erase(0, strlen(txtWlanPass));
      TrimString(txtLine);
      m_txtWlanPassphrase = txtLine;
   }
   if (StartsWith(txtLine, txtWlanCntry))
   {
      txtLine.erase(0, strlen(txtWlanCntry));
      TrimString(txtLine);
      m_txtWlanCountry = txtLine;
   }

   if (StartsWith(txtLine, txtRemountRW))
   {   
      txtLine.erase(0, strlen(txtRemountRW));
      TrimString(txtLine);
      m_listRemountReadWrite.clear();
      int nLengthText = txtLine.length();
      std::string txtElement;
      for (int i = 0; i < nLengthText; i++)
      {
         if (txtLine.at(i) == ';')
         {
            if (txtElement.length() > 0)
            {
               m_listRemountReadWrite.push_back(txtElement);
            }
            txtElement.clear();
         }
         else
         {
            txtElement.push_back(txtLine[i]);
         }
      }
      if (txtElement.length() > 0)
      {
         m_listRemountReadWrite.push_back(txtElement);
      } 
   }
}

// ============================================================================

SystemConfigFile::SystemConfigFile(std::string txtDirectoryPath)
{
   m_pPriv = std::make_unique<SystemConfigFile::PrivateData>(txtDirectoryPath);
}

// -----------------------------------------------------------------------------

SystemConfigFile::~SystemConfigFile()
{
}

// -----------------------------------------------------------------------------

std::string SystemConfigFile::GetMp3Path()
{
   return m_pPriv->m_txtMp3Path;
}

// -----------------------------------------------------------------------------

std::string SystemConfigFile::GetSystemSoundPath()
{
   return SystemConfigFile::PrivateData::m_SystemSoundPath.GetPath();
}

// -----------------------------------------------------------------------------

SystemConfigFile::NumberFormat SystemConfigFile::GetNumberFormat()
{
   return SystemConfigFile::PrivateData::m_NumberFormat.GetFormat();
}

// -----------------------------------------------------------------------------

int SystemConfigFile::GetAutoShutdownInMinutes()
{
   return m_pPriv->m_nAutoShutdownMinutes;
}

// -----------------------------------------------------------------------------

bool SystemConfigFile::DisableWifi()
{
   return m_pPriv->m_bWifiOff;
}

// -----------------------------------------------------------------------------

std::string SystemConfigFile::GetPersistentStorageDirPath()
{
   return m_pPriv->m_txtStorageDirPath;
}

// -----------------------------------------------------------------------------
// single press on blue button -> next directory (small children could like that)
bool SystemConfigFile::AllowNextDirByServiceKey()
{
   return m_pPriv->m_bAllowNextDirByServiceKey;
}

// -----------------------------------------------------------------------------

bool  SystemConfigFile::WlanAccessPointMode()
{
   return m_pPriv->m_bWlanAccessPointMode;
}

// -----------------------------------------------------------------------------

std::string  SystemConfigFile::WlanSSID()
{
   return m_pPriv->m_txtWlanSSID;
}

// -----------------------------------------------------------------------------

std::string  SystemConfigFile::WLANPassphrase()
{
   return m_pPriv->m_txtWlanPassphrase;
}

// -----------------------------------------------------------------------------

std::string  SystemConfigFile::WLANCountry()
{
   return m_pPriv->m_txtWlanCountry;
}

// -----------------------------------------------------------------------------

std::list<std::string>  SystemConfigFile::ServiceRemountReadWriteList()
{
   return m_pPriv->m_listRemountReadWrite;
}


