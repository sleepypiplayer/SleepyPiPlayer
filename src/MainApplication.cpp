// ----------------------------------------------------------------------------
// SleepyPiPlayer:  main() is the designated start of the program
//     - create all objects
//     - set playback-position and volume recalled by class-PersistentStorage
//     - forward Key-Events from class-KeyInput to class-AudioPlayer
//     - take care of auto-shutdown-timer
//     - start shutdown if red-button was pressed
//     - store audio-playback-position by using class-PersistentStorage
// license: free software   (sleepypiplayer(at)saftfresse.de)  [A.D.2026]
// - Do whatever you want with the software.
// - Do not claim my work as your own.
// - THIS SOFTWARE COMES WITH NO WARRANTIES, USE AT YOUR OWN RISK
// ----------------------------------------------------------------------------

#include "KeyInput.h"
#include "SystemConfigFile.h"
#include "Mp3DirFileList.h"
#include "AudioPlayer.h"
#include "PersistentStorage.h"
#include "SystemSoundCatalog.h"
#include "NumberToSound.h"
#include "ServiceMode.h"
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <unistd.h>
#include <thread>
#include <csignal>
#include <atomic>


// ----- forward-declarations ----
void print_to_console(KeyInput::KEY key);
void play_audiofeedback_shutdown(int nVolume, int nInMinutes); // nInMinutes -1: immediate
void play_audiofeedback_wlan_fallback(int nVolume, std::string txtPassphrase);

// ----------------------------------------------------------------------------

namespace   // anonymous namespace
{
   volatile std::atomic<bool> bReceivedSigTerm = false;

   void sigterm_signal_handler(int /*signal*/)
   {
      // delete key_input: release gpio if the application is asked to quit
      bReceivedSigTerm = true;
   }
}

// ----------------------------------------------------------------------------

int main(int argc, char* argv[])
{
   std::this_thread::sleep_for( std::chrono::milliseconds(20) );
   std::signal(SIGTERM, sigterm_signal_handler);

   char txtWorkingDirectory[300] = {};
   getcwd(txtWorkingDirectory, 299);

   printf("\nTHIS SOFTWARE COMES WITH NO WARRANTIES, USE AT YOUR OWN RISK\n");

   if (argc > 0)
   {
      if (argv[0] != nullptr)
      {
         // --- config
         std::filesystem::path pathApplicationAbsolute = std::filesystem::absolute(argv[0]);
         std::filesystem::path pathBaseDir = pathApplicationAbsolute;
         pathBaseDir.remove_filename();

         SystemConfigFile  config(pathBaseDir.c_str());
         printf("SysConfig sysAudio:  %s\n\r", config.GetSystemSoundPath().c_str());
         printf("SysConfig Mp3Audio:  %s\n\r", config.GetMp3Path().c_str());
         printf("SysConfig Storage:   %s\n\r", config.GetPersistentStorageDirPath().c_str());
         printf("SysConfig Numbers:   %s\n\r",(config.GetNumberFormat()==SystemConfigFile::NumberFormat::GERMAN)?"GERMAN":"ENGLISH");
         printf("SysConfig Dis.Wifi:  %i\n\r", config.DisableWifi());
         printf("SysConfig shutdown:  %i\n\r", config.GetAutoShutdownInMinutes());
         printf("SysConfig SrvNxtDir: %i\n\r", config.AllowNextDirByServiceKey());
         printf("SysConfig WLAN:      %s\n\r", config.WlanAccessPointMode()? "AP" : "Client");
         printf("SysConfig W_SSID:    %s\n\r", config.WlanSSID().c_str());
         printf("SysConfig W_Passphr: %s\n\r", config.WLANPassphrase().c_str());
         printf("SysConfig W_Country: %s\n\r", config.WLANCountry().c_str());

         KeyInput key_input(config.AllowNextDirByServiceKey());
         ServiceMode service_mode(config);

         if (config.DisableWifi())
         {
            service_mode.DisableWlan();
         }

         PersistentStorage storage(config.GetPersistentStorageDirPath());
         printf("Persistence volume: %i   pos:%0.1fsec  file:%s\n\r", storage.GetVolume(),
               storage.GetPlaybackTime(), storage.GetPlaybackPath().c_str());
         Mp3DirFileList  Mp3List(config.GetMp3Path());
         for (int nDirIdx = 0; nDirIdx < storage.GetNofAdditionalPlaybackPos(); nDirIdx++)
         {
            std::string txtPath = storage.GetAdditionalPlaybackPath(nDirIdx);
            double      dTime   = storage.GetAdditionalPlaybackTime(nDirIdx);
            Mp3List.SetPlaybackPosFromStorage(txtPath, dTime);
         }
         Mp3List.SetPlaybackPosFromStorage(storage.GetPlaybackPath(), storage.GetPlaybackTime());

         std::string txtPassphrase;
         if (config.WlanAccessPointMode())
         {
            txtPassphrase = config.WLANPassphrase();
         }

         std::chrono::milliseconds durationSleep{15};
         bool bService  = false;
         bool bShutdown = false;
         {
            AudioPlayer  player(
                  storage.GetVolume(),
                  &Mp3List,
                  config.GetAutoShutdownInMinutes(),
                  storage.ChecksumProblemDetected(),
                  txtPassphrase
               );
            if (storage.ChecksumProblemDetected())
            {
               printf("#= storage checksum error\n");
            }

            while(!bShutdown && !bService && !bReceivedSigTerm)
            {
               std::list<KeyInput::KEY> keys = key_input.GetInputKeys();
               for (KeyInput::KEY key : keys)
               {
                  print_to_console(key);
                  player.AddUserRequest(key);
               }
               std::this_thread::sleep_for(durationSleep); // save cpu-power
               bShutdown = player.IsShutdown();
               bService  = player.IsServiceMode();
            }
            PlaybackInfo CurrentPlayback { player.GetCurrentPlaybackInfo() };
            if (CurrentPlayback.GetFilePath().length() > 0)
            {
               storage.SetPlayback(CurrentPlayback.GetFilePath(), CurrentPlayback.GetPlaybackPos() );
               storage.SetVolume(player.GetVolumeInPct());
               storage.ClearAdditionalPlaybackPos();
               for (int nDirIdx = 0; nDirIdx < Mp3List.GetNofDirectories(); nDirIdx++)
               {
                  PlaybackInfo info = Mp3List.GetPlaybackOfDirectory(nDirIdx);
                  storage.AddAdditionalPlaybackPos(info.GetFilePath(), info.GetPlaybackPos());
               }
               storage.WriteFile();
            }
         }

         if (bShutdown && config.GetAutoShutdownInMinutes() >= 0)
         {
            printf("#= Shutdown\n");
            play_audiofeedback_shutdown(storage.GetVolume(),/*InMinutes:*/-1);
            std::system("sudo umount /SLEEPY_SAVE");
            std::system("sudo shutdown --poweroff +0");
         }
         if (bService)
         {
            printf("#= Service Mode\n");

            service_mode.StartService();

            std::chrono::minutes  durationShutdown{30};  // auto-shutdown in minutes in service-mode
            std::chrono::minutes  durationAudioOut{4};   // repeated audio-output of "shutdown in XXX minutes"
            std::chrono::time_point<std::chrono::steady_clock> timeStart = std::chrono::steady_clock::now();
            std::chrono::time_point<std::chrono::steady_clock> timeNow   = std::chrono::steady_clock::now();
            std::chrono::time_point<std::chrono::steady_clock> timeAudioOutput = std::chrono::time_point<std::chrono::steady_clock>::min();

            std::string txtFallbackPassphrase;
            while (timeNow < (timeStart + durationShutdown) && !bShutdown && !bReceivedSigTerm)
            {
               timeNow = std::chrono::steady_clock::now();
               std::this_thread::sleep_for(durationSleep); // save cpu-power
               std::list<KeyInput::KEY> keys = key_input.GetInputKeys();
               for (KeyInput::KEY key : keys)
               {
                  bShutdown = bShutdown || (key == KeyInput::KEY_Shutdown);
                  if (key != KeyInput::KEY_NONE && key != KeyInput::KEY_Service)
                  {
                     timeStart = std::chrono::steady_clock::now();  // restart shutdown-timer on key-press
                     timeAudioOutput = std::chrono::time_point<std::chrono::steady_clock>::min(); // immediate audio-feedback
                  }
               }
               if (!bShutdown && (timeNow < timeAudioOutput || timeNow > (timeAudioOutput+durationAudioOut)))
               {
                  timeAudioOutput = std::chrono::steady_clock::now();
                  std::chrono::minutes durationToShutdown = std::chrono::duration_cast<std::chrono::minutes>((timeStart + durationShutdown) - timeNow);
                  play_audiofeedback_shutdown(storage.GetVolume(), durationToShutdown.count());
               }
               if (txtFallbackPassphrase.empty())
               {
                  txtFallbackPassphrase = service_mode.GetFallbackPassphrase();
                  if (!txtFallbackPassphrase.empty())
                  {
                     play_audiofeedback_wlan_fallback(storage.GetVolume(),txtFallbackPassphrase);
                  }
               }
            }
            if (!bReceivedSigTerm)
            {
               printf("#= Shutdown\n");
               std::system("sudo shutdown --poweroff +1"); // just safety
               play_audiofeedback_shutdown(storage.GetVolume(),/*InMinutes:*/-1);
               std::system("sudo shutdown --poweroff +0");
            }
         }
      }
   }
   return 0;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------


// play shutdown audio-message
// nInMinutes  X: "shutdwon in X minutes"  / -1 : "shutting down"
void play_audiofeedback_shutdown(int nVolume, int nInMinutes)
{
   SystemSoundCatalog catalog;
   std::list<std::string> listMp3Path;
   listMp3Path.push_back(catalog.Silence());
   if (nInMinutes > 0)
   {
      NumberToSound numbers;
      listMp3Path.push_back(catalog.Text_ShutdownIn());
      numbers.AddNumber(listMp3Path, nInMinutes);
      listMp3Path.push_back(catalog.Text_Minutes());
   }
   else
   {
      listMp3Path.push_back(catalog.Text_ShuttingDown());
   }
   listMp3Path.push_back(catalog.Silence());
   int nFileLimit = listMp3Path.size();
   for (int i = 0; i < 20; i++)
      listMp3Path.push_back(catalog.Silence());
   Mp3DirFileList  Mp3ListShutdown(listMp3Path);
   AudioPlayer  player(
      nVolume,
      &Mp3ListShutdown,
      /* shudown  minutes:*/ 10,
      /* checksum problem:*/ false,
      /* passphrase:*/ "");
   std::chrono::milliseconds durationSleep{10};
   int nTimeLimit = (nInMinutes > 0)? 800 : 500;
   while (player.GetCurrentPlaybackInfo().GetFileNumber() <= nFileLimit && nTimeLimit > 0)
   {
      std::this_thread::sleep_for(durationSleep); // save cpu-power
      --nTimeLimit;
   }
}

// ----------------------------------------------------------------------------

void play_audiofeedback_wlan_fallback(int nVolume, std::string txtPassphrase)
{
   SystemSoundCatalog catalog;
   std::list<std::string> listMp3Path;
   listMp3Path.push_back(catalog.Silence());
   listMp3Path.push_back(catalog.Text_WlanFallback());
   listMp3Path.push_back(catalog.Silence());
   catalog.SpellString(listMp3Path, txtPassphrase);
   listMp3Path.push_back(catalog.Silence());
   int nFileLimit = listMp3Path.size();
   for (int i = 0; i < 20; i++)
      listMp3Path.push_back(catalog.Silence());
   Mp3DirFileList  Mp3ListFallback(listMp3Path);
   AudioPlayer  player(
      nVolume,
      &Mp3ListFallback,
      /* shudown  minutes:*/ 10,
      /* checksum problem:*/ false,
      /* passphrase:*/ "");
   std::chrono::milliseconds durationSleep{10};
   int nTimeLimit = 1000;
   while (player.GetCurrentPlaybackInfo().GetFileNumber() <= nFileLimit && nTimeLimit > 0)
   {
      std::this_thread::sleep_for(durationSleep); // save cpu-power
      --nTimeLimit;
   }
}

// ----------------------------------------------------------------------------

void print_to_console(KeyInput::KEY key)
{
   static KeyInput::KEY static_old_key = KeyInput::KEY_NONE;

   switch (key)
   {
   case KeyInput::KEY_PlayFastForw:
   case KeyInput::KEY_PlayFastBack:
   case KeyInput::KEY_FileFastForw:
   case KeyInput::KEY_FileFastBack:
   case KeyInput::KEY_ANY:
   case KeyInput::KEY_Service:
      if (key == static_old_key)  return;
      break;
   default: break;
   }
   static_old_key = key;

   switch(key)
   {
   case KeyInput::KEY_SimulatedAutoShutdown: printf("#= Simulate auto-shutdown\n"); break;
   case KeyInput::KEY_NONE:                  printf("#= NONE\n");                   break;
   case KeyInput::KEY_ANY:                   printf("#= ANY\n");                    break;
   case KeyInput::KEY_VolumeInc:             printf("#= Volume +\n");               break;
   case KeyInput::KEY_VolumeDec:             printf("#= Volume -\n");               break;
   case KeyInput::KEY_FileNext:              printf("#= File +\n");                 break;
   case KeyInput::KEY_FilePrev:              printf("#= File -\n");                 break;
   case KeyInput::KEY_FileFastForw:          printf("#= File-Fast +\n");            break;
   case KeyInput::KEY_FileFastBack:          printf("#= File-Fast -\n");            break;
   case KeyInput::KEY_PlayFastForw:          printf("#= Audio-FastForw\n");         break;
   case KeyInput::KEY_PlayFastBack:          printf("#= Audio-FastBack\n");         break;
   case KeyInput::KEY_DirNext:               printf("#= Dir +\n");                  break;
   case KeyInput::KEY_DirPrev:               printf("#= Dir -\n");                  break;
   case KeyInput::KEY_Info:                  printf("#= Info\n");                   break;
   case KeyInput::KEY_Shutdown:              printf("#= Shutdown\n");               break;
   case KeyInput::KEY_Service:               printf("#= Service\n");                break;
   }
}

