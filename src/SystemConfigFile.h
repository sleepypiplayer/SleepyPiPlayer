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
// ----------------------------------------------------------------------------
#pragma once
#include <string>
#include <memory>
#include <list>

class SystemConfigFile
{
public:
   SystemConfigFile(std::string txtDirectoryPath);
   virtual ~SystemConfigFile();

   enum class NumberFormat
   {
      ENGLISH,
      GERMAN
   };

   std::string         GetMp3Path();
   std::string         GetPersistentStorageDirPath();
   static std::string  GetSystemSoundPath();
   static NumberFormat GetNumberFormat();
   bool                DisableWifi();
   int                 GetAutoShutdownInMinutes();
   bool                AllowNextDirByServiceKey();  // single press on blue button -> next directory

   bool                WlanAccessPointMode();
   std::string         WlanSSID();
   std::string         WLANPassphrase();
   std::string         WLANCountry();      // DE GB ES FR US

   std::list<std::string> ServiceRemountReadWriteList();   // Service-Mode e.g.  { "/SLEEPY_MP3" , "/SLEEPY_SAVE" }

private:
   class PrivateData;
   std::unique_ptr<PrivateData> m_pPriv;
};

