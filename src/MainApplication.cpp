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


// ----- forward-declarations ----
void print_to_console(KeyInput::KEY key);
void execute_thread_rfkill();
void execute_thread_enable_wifi();
void play_audiofeedback_shutdown(int nVolume);

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


         KeyInput key_input(config.AllowNextDirByServiceKey());

         std::thread threadRfkill;

         if (config.DisableWifi())
         {
            printf("#= rfkill block wifi\n");
            std::thread threadTempRfkill( execute_thread_rfkill );
            threadRfkill.swap(threadTempRfkill);
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
               storage.ClearAdditionalPlaybackPos();
               for (int nDirIdx = 0; nDirIdx < Mp3List.GetNofDirectories(); nDirIdx++)
               {
                  PlaybackInfo info = Mp3List.GetPlaybackOfDirectory(nDirIdx);
                  storage.AddAdditionalPlaybackPos(info.GetFilePath(), info.GetPlaybackPos());
               }
               storage.WriteFile();
            }
         }

         if (threadRfkill.joinable())
             threadRfkill.join();

         if (bShutdown && config.GetAutoShutdownInMinutes() >= 0)
         {
            printf("#= Shutdown\n");
            play_audiofeedback_shutdown(storage.GetVolume());
            std::system("sudo umount /SLEEPY_SAVE");
            std::system("sudo shutdown --poweroff +0");
         }
         if (bService)
         {
            printf("#= Service Mode\n");

            std::thread threadService( execute_thread_enable_wifi );
            threadService.detach();

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
               play_audiofeedback_shutdown(storage.GetVolume());
               std::system("sudo shutdown --poweroff +0");
            }
            else
            {
               std::system("sudo shutdown --poweroff +15");
            }
         }
      }
   }
   return 0;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

// do not wait for rfkill: play audio 0.7 seconds earlier
void execute_thread_rfkill()
{
   std::system("sudo rfkill block wifi");
   std::system("sudo rfkill block bluetooth");
}

// ----------------------------------------------------------------------------

// do not wait for WIFI: immediate audio-feedback "shutting down"
void execute_thread_enable_wifi()
{
   // Restart of wlan0 requires write-access to /etc/resolv.conf
   // unused alternative to allow write-access to root-file-system: OverlayFileSystem
   // if (std::filesystem::is_directory("/SLEEPY_TMPFS_OVERLAY"))
   // {
   //    std::system("sudo mount -t tmpfs -o size=2M none /SLEEPY_TMPFS_OVERLAY");
   //    std::system("sudo mkdir /SLEEPY_TMPFS_OVERLAY/etc_tmpfs");
   //    std::system("sudo mkdir /SLEEPY_TMPFS_OVERLAY/etc_work");
   //    std::system("sudo mount -t overlay -o lowerdir=/etc,upperdir=/SLEEPY_TMPFS_OVERLAY/etc_tmpfs,workdir=/SLEEPY_TMPFS_OVERLAY/etc_work none /etc");
   // }

   std::system("sudo mount -o remount,rw /");
   std::system("sudo rfkill unblock wifi");
   std::system("sudo systemctl restart ifup@wlan0");
}

// ----------------------------------------------------------------------------

void play_audiofeedback_shutdown(int nVolume)
{
   SystemSoundCatalog catalog;
   std::list<std::string> listMp3Path;
   listMp3Path.push_back(catalog.Silence());
   listMp3Path.push_back(catalog.Text_ShuttingDown());
   for (int i = 0; i < 20; i++)
      listMp3Path.push_back(catalog.Silence());
   Mp3DirFileList  Mp3ListShutdown(listMp3Path);
   AudioPlayer  player(
      nVolume,
      &Mp3ListShutdown,
      /* shudown  minutes:*/ 10,
      /* checksum problem:*/ false);
   std::chrono::milliseconds durationSleep{10};
   int nTimeLimit = 500;
   while (player.GetCurrentPlaybackInfo().GetFileNumber() <= 2 && nTimeLimit > 0)
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

