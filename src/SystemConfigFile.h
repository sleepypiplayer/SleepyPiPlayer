// ----------------------------------------------------------------------------
// SleepyPiPlayer:  SystemConfigFile
//  file in the executable directory with name "sleepy.cfg"  example:
//    # all characters after a hash will be ignored
//    # only lines starting with a key-word are processed
//    MP3_PATH             /SLEEPY_MP3/mp3              # files used for playback
//    STORAGE_DIR          /SLEEPY_SAVE/save            # 100 x 4MB-files containing playback-position and volume
//    SYSTEM_SOUND_PATH    /SLEEPY_FW/sleepy/SysMP3/german  # files used for audio-feedback
//    NUMBER_FORMAT        ENGLISH                      # "ENGLISH" or "GERMAN"   e.g. say "sieben-und-zwanzig" or "twenty-seven"
//    DISABLE_WIFI         ON                           # "ON" or "OFF"  default=ON  save battery by disabling wifi (Service-mode can still enable WIFI)
//    AUTO_SHUTDOWN        25                           # time in minutes (-1: no shutdown)
//    SRV_KEY_CHG_DIR      OFF                          # "ON" or "OFF"  default=OFF  single-press of blue service-key: change directory
// ----------------------------------------------------------------------------
#pragma once
#include <string>
#include <memory>


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

private:
   class PrivateData;
   std::unique_ptr<PrivateData> m_pPriv;
};

