// ----------------------------------------------------------------------------
// SleepyPiPlayer:  main() is the designated start of the program
//     - create all objects
//     - set playback-position and volume recalled by class-PersistentStorage
//     - forward Key-Events from class-KeyInput to class-AudioPlayer
//     - take care of auto-shutdown-timer
//     - start shutdown if red-button was pressed
//     - store audio-playback-position by using class-PersistentStorage
// license: free software   (sleepypiplayer(at)saftfresse.de)  [A.D.2025]
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
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <unistd.h>
#include <thread>
#include <csignal>
#include <atomic>

void print_to_console(KeyInput::KEY key); // forward-declaration

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
         KeyInput key_input;

         // --- config
         std::filesystem::path pathApplicationAbsolute = std::filesystem::absolute(argv[0]);
         std::filesystem::path pathBaseDir = pathApplicationAbsolute;
         pathBaseDir.remove_filename();

         SystemConfigFile  config(pathBaseDir.c_str());
         printf("SysConfig sysAudio: %s\n\r", config.GetSystemSoundPath().c_str());
         printf("SysConfig Mp3Audio: %s\n\r", config.GetMp3Path().c_str());
         printf("SysConfig Storage:  %s\n\r", config.GetPersistentStorageDirPath().c_str());
         printf("SysConfig Numbers:  %s\n\r",(config.GetNumberFormat()==SystemConfigFile::NumberFormat::GERMAN)?"GERMAN":"ENGLISH");
         printf("SysConfig Dis.Wifi: %i\n\r", config.DisableWifi());
         printf("SysConfig shutdown: %i\n\r", config.GetAutoShutdownInMinutes());

         if (config.DisableWifi() && !key_input.IsServiceKeyPressed())
         {
            printf("#= rfkill block wifi\n");
            std::system("sudo rfkill block wifi");
            std::system("sudo rfkill block bluetooth");
         }

         PersistentStorage storage(config.GetPersistentStorageDirPath());
         printf("Persistence volume: %i   pos:%0.1fsec  file:%s\n\r", storage.GetVolume(),
               storage.GetPlaybackTime(), storage.GetPlaybackPath().c_str());
         Mp3DirFileList  Mp3List(config.GetMp3Path());
         Mp3List.SetPlaybackPosFromStorage(storage.GetPlaybackPath(), storage.GetPlaybackTime());

         std::chrono::milliseconds durationSleep{15};
         bool bService  = false;
         bool bShutdown = false;
         {
            AudioPlayer  player(
                  storage.GetVolume(),
                  &Mp3List,
                  config.GetAutoShutdownInMinutes(),
                  storage.ChecksumProblemDetected()
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
               storage.WriteFile();
            }
         }

         if (bShutdown && config.GetAutoShutdownInMinutes() >= 0)
         {
            printf("#= Shutdown\n");
            std::system("sudo shutdown --poweroff +0");
         }
         if (bService)
         {
            printf("#= Service Mode\n");
            std::system("sudo rfkill unblock wifi");
            std::system("sudo systemctl restart wpa_supplicant");
            std::system("sudo systemctl restart ifup@wlan0");
            std::system("sudo systemctl restart networking");
            std::system("sudo systemctl restart NetworkManager");
            std::system("sudo systemctl restart systemd-timesyncd");

            std::chrono::time_point<std::chrono::steady_clock> timeStart = std::chrono::steady_clock::now();
            std::chrono::time_point<std::chrono::steady_clock> timeNow   = std::chrono::steady_clock::now();
            std::chrono::minutes  durationShutdown{20};
            while (timeNow < (timeStart + durationShutdown) && !bShutdown && !bReceivedSigTerm)
            {
               timeNow = std::chrono::steady_clock::now();
               std::this_thread::sleep_for(durationSleep); // save cpu-power
               std::list<KeyInput::KEY> keys = key_input.GetInputKeys();
               for (KeyInput::KEY key : keys)
               {
                  bShutdown = bShutdown || (key == KeyInput::KEY_Shutdown);
               }
            }
            if (bShutdown)
            {
               printf("#= Shutdown\n");
               std::system("sudo shutdown --poweroff +1"); // just safety
               SystemSoundCatalog catalog;
               std::list<std::string> listMp3Path;
               listMp3Path.push_back(catalog.Text_ShuttingDown());
               for (int i = 0; i < 10; i++)
                  listMp3Path.push_back(catalog.Silence());
               Mp3DirFileList  Mp3ListShutdown(listMp3Path);
               AudioPlayer  player(
                  storage.GetVolume(),
                  &Mp3ListShutdown,
                  /* shudown  minutes:*/ 10,
                  /* checksum problem:*/ false);
               while (player.GetCurrentPlaybackInfo().GetFileNumber() == 1)
               {
                  std::this_thread::sleep_for(durationSleep); // save cpu-power
               }
               std::system("sudo shutdown --poweroff +0");
            }
            else
            {
               std::system("sudo shutdown --poweroff +15");
            }
         }
      }
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
