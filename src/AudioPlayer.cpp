// ----------------------------------------------------------------------------
// SleepyPiPlayer:  AudioPlayer
//     play mp3-files
//     handle key-press
//     check time for auto-shutdown
// license: free software   (sleepypiplayer(at)saftfresse.de)  [A.D.2025]
// - Do whatever you want with the software.
// - Do not claim my work as your own.
// - THIS SOFTWARE COMES WITH NO WARRANTIES, USE AT YOUR OWN RISK
// ----------------------------------------------------------------------------
// https://www.mpg123.de/api/modules.shtml

#include "AudioPlayer.h"
#include "AudioFeedback.h"
#include "AudioVolumeConversion.h"

#include <thread>
#include <mutex>
#include <chrono>
#include <list>
#define _FILE_OFFSET_BITS 64    // enable usage of mpg123_length_64()
#define MPG123_LARGESUFFIX MPG123_MACROCAT(_, _FILE_OFFSET_BITS)
#include <out123.h>
#include <mpg123.h>
#include <atomic>

// ============================================================================
class AudioPlayer::PrivateData
{
public:
   PrivateData(
      int nVolumePct,
      Mp3DirFileList* pFileList,
      int nAutoShutdownMinutes,
      bool bChecksumProblem)
   : m_AudioFeedback(SAMPLE_RATE, CHANNELS, ENCODING)
   {
      m_AudioFeedback.SetVolume(nVolumePct);
      m_nVolumePct            = nVolumePct;
      m_pFileList             = pFileList;
      m_nAutoShutdownMinutes  = nAutoShutdownMinutes;
      m_bShutdown             = false;
      m_bServiceMode          = false;
      m_timeAutoShutdownStart = std::chrono::steady_clock::now();
      m_Thread = std::thread(ThreadFunction, this);
      if (bChecksumProblem)
      {
         m_AudioFeedback.ReportChecksumProblem();
      }
      m_CurrentPlayback = pFileList->GetCurrentFile();
      if (m_CurrentPlayback.GetNofDirs() > 0) // no startup-report for "shutting_down"
      {
         m_AudioFeedback.ReportStartupPosition(m_pFileList->GetCurrentFile());
      }
   }

   virtual ~PrivateData()
   {
      m_bStopThread = true;
      m_Thread.join();
   }

   void AddUserRequest(KeyInput::KEY  key);

   const int   SAMPLE_BUFFER_SIZE = 16000;
   const long  SAMPLE_RATE        = 32000;
   const int   CHANNELS           = MPG123_MONO;
   const int   ENCODING           = MPG123_ENC_SIGNED_16;

   int64_t m_nPlayFastStartSampleIdx = -1; // increased difference to start-position: increased step-size
   bool    m_bPlayFastForw = false;
   bool    m_bPlayFastBack = false;

   int32_t m_nFileFastStartNumber = -1; // increased difference to start-position: increased step-size
   bool    m_bFileFastForw = false;  // long-press: info+nextFile
   bool    m_bFileFastBack = false;  // long-press: info+prevFile

   volatile bool    m_bStopThread = false;
   std::thread      m_Thread;
   std::mutex       m_MutexUserReq;
   std::mutex       m_MutexPlayback;
   Mp3DirFileList*  m_pFileList  = nullptr;
   std::list<KeyInput::KEY>  m_listUserRequests;  // protected by m_MutexUserReq
   PlaybackInfo     m_CurrentPlayback;            // protected by m_MutexPlayback

   AudioFeedback         m_AudioFeedback;    // audio-messages like "file 43 of 97"
   AudioVolumeConversion m_VolumeConversion; // convert from percent to logarithmic-scale
   std::atomic<int>      m_nVolumePct;       // currently used audio-volume

   std::chrono::time_point<std::chrono::steady_clock> m_timeAutoShutdownStart; // time of last key-press

   std::atomic<int>  m_nAutoShutdownMinutes;  // start auto-shutdown countdown some minutes after last key-press
   std::atomic<bool> m_bShutdown;      // tell MainApplication: shutdown required
   std::atomic<bool> m_bServiceMode;   // tell MainApplication: service-mode requested

   unsigned char* m_pSampleBuffer   = nullptr;
   out123_handle* m_pHandleSoundOut = nullptr;
   mpg123_handle* m_pHandleMp3File  = nullptr;
   int64_t        m_nCurrentFileSampleLength = 0;


   static void ThreadFunction(AudioPlayer::PrivateData* pThis);

   void ExecPlaybackThread();
   void SetupPlaybackFile();

   void    ChangeVolume(int nDeltaPct);
   void    FileNext(int nSteps = 1);
   void    FilePrev(int nSteps = 1);
   int     FileFastStepsize();
   void    FileFastForw();
   void    FileFastBack();
   void    DirNext();
   void    DirPrev();
   int64_t SetFastPlaybackPosInMp3File(); // returns: new position in minutes / -1: file-limits exceeded
   void    InfoRequest();
};

// ----------------------------------------------------------------------------

void AudioPlayer::PrivateData::ThreadFunction(AudioPlayer::PrivateData* pThis)
{
   pThis->ExecPlaybackThread();
}

// ----------------------------------------------------------------------------

void AudioPlayer::PrivateData::ExecPlaybackThread()
{

   int err = MPG123_OK;
   m_pSampleBuffer   = new unsigned char[SAMPLE_BUFFER_SIZE];
   m_pHandleSoundOut = out123_new();
   m_pHandleMp3File  = mpg123_new(nullptr, &err);
   {
      mpg123_param(m_pHandleMp3File, MPG123_VERBOSE, 0, 0);
      long   nMpg123Flags = 0;
      double dMpg123Dummy = 0.0;
      mpg123_getparam(m_pHandleMp3File, MPG123_FLAGS, &nMpg123Flags, &dMpg123Dummy);
      mpg123_param(m_pHandleMp3File, MPG123_FLAGS, MPG123_QUIET | nMpg123Flags, 0);
   }
   err = out123_open(m_pHandleSoundOut, "alsa", nullptr);
   if (err != 0)
   {
       err = out123_open(m_pHandleSoundOut, "alsa", "sysdefault:CARD=Device");  // raspberry-fallback
   }
   if (err != 0)
   {
       err = out123_open(m_pHandleSoundOut, "alsa", "plughw:CARD=Device,DEV=0");  // raspberry-fallback
   }
   if (err != 0)
   {
      char ** driver_names = nullptr;
      char ** driver_descr = nullptr;
      int nNofDrivers = out123_drivers(m_pHandleSoundOut, &driver_names, &driver_descr);
      for (int nDrvIdx = 0; nDrvIdx < nNofDrivers; nDrvIdx++)
      {
         printf("#= ---------------------------------------------\n");
         printf("#= sound_driver[%i]: \"%s\"\n",nDrvIdx, driver_names[nDrvIdx]);
         printf("#= \"%s\"\n", driver_descr[nDrvIdx]);
         char ** device_names = nullptr;
         char ** device_descr = nullptr;
         int nNofDevices = out123_devices(m_pHandleSoundOut, driver_names[nDrvIdx], &device_names, &device_descr, nullptr);
         for (int nDevIdx = 0; nDevIdx < nNofDevices; nDevIdx++)
         {
            printf("#=    device[%i]: \"%s\"\n",nDevIdx, device_names[nDevIdx]);
            printf("#=    \"%s\"\n", device_descr[nDevIdx]);
         }
         out123_stringlists_free(device_names, device_descr, nNofDevices);
      }
      out123_stringlists_free(driver_names, driver_descr, nNofDrivers);
   }
   err = out123_start(m_pHandleSoundOut, SAMPLE_RATE, CHANNELS, ENCODING);

   while (!m_bStopThread)
   {
      err = MPG123_OK;
      SetupPlaybackFile();
      bool bPlayingFile = true;
      while (!m_bStopThread && err == MPG123_OK &&  bPlayingFile)
      {
         {
            std::lock_guard<std::mutex> guard(m_MutexUserReq);
            for (KeyInput::KEY key : m_listUserRequests)
            {
               m_timeAutoShutdownStart = std::chrono::steady_clock::now();

               if (m_AudioFeedback.IsReportingShutdown())
               {
                  m_AudioFeedback.CancelShutdown();
                  key = KeyInput::KEY_NONE; // key-press canceled shutdown: do not change file/volume/...
               }
               else if (key == KeyInput::KEY_Shutdown)
               {
                  m_bShutdown = true;
               }

               switch(key)
               {
               case KeyInput::KEY_PlayFastForw:
                  m_bPlayFastForw = true;
                  m_bPlayFastBack = false;
                  break;
               case KeyInput::KEY_PlayFastBack:
                  m_bPlayFastBack = true;
                  m_bPlayFastForw = false;
                  break;
               default:
                  m_bPlayFastForw = false;
                  m_bPlayFastBack = false;
                  m_nPlayFastStartSampleIdx = -1;
                  break;
               }
               switch(key)
               {
               case KeyInput::KEY_FileFastForw:
                  m_bFileFastForw = true;
                  m_bFileFastBack = false;
                  break;
               case KeyInput::KEY_FileFastBack:
                  m_bFileFastBack = true;
                  m_bFileFastForw = false;
                  break;
               default:
                  m_bFileFastForw = false;
                  m_bFileFastBack = false;
                  m_nFileFastStartNumber = -1;
               }
               switch(key)
               {
                  case KeyInput::KEY_VolumeInc:  ChangeVolume(+1);  break;
                  case KeyInput::KEY_VolumeDec:  ChangeVolume(-1);  break;
                  case KeyInput::KEY_FileNext:   FileNext();        break;
                  case KeyInput::KEY_FilePrev:   FilePrev();        break;
                  case KeyInput::KEY_DirNext:    DirNext();         break;
                  case KeyInput::KEY_DirPrev:    DirPrev();         break;
                  case KeyInput::KEY_Info:       InfoRequest();     break;
                  case KeyInput::KEY_Service:               m_AudioFeedback.ReportServiceMode(); break;
                  case KeyInput::KEY_SimulatedAutoShutdown: m_AudioFeedback.ReportShutdown();    break;
                  default: break;
               }
            }
            m_listUserRequests.clear();
         }

         // FileFastXxx does not change the current file as long as
         // there is a message in the message-queue -- see FileFastStepsize()
         if (m_bFileFastForw) { FileFastForw(); }
         if (m_bFileFastBack) { FileFastBack(); }

         if (m_nAutoShutdownMinutes > 0)
         {
            std::chrono::minutes  durationShutdown{m_nAutoShutdownMinutes};
            std::chrono::time_point<std::chrono::steady_clock> timeNow = std::chrono::steady_clock::now();
            if (timeNow > (m_timeAutoShutdownStart + durationShutdown))
            {
               m_AudioFeedback.ReportShutdown();
            }
         }

         size_t nSampleBufferBytes = 0;

         m_bShutdown    = m_bShutdown    || m_AudioFeedback.IsShutdownReported();
         m_bServiceMode = m_bServiceMode || m_AudioFeedback.IsServiceModeReported();

         bool bPlayAudioFeedback = m_AudioFeedback.GetAudioSamples(m_pSampleBuffer, SAMPLE_BUFFER_SIZE, /*OUT:*/&nSampleBufferBytes);
         if (!bPlayAudioFeedback || nSampleBufferBytes == 0)
         {
            double dVolume = m_VolumeConversion.ConvertPercentToDouble(m_nVolumePct);
            mpg123_volume(m_pHandleMp3File, dVolume);
            if (m_bPlayFastForw || m_bPlayFastBack)
            {
               // create Audio-Feedback instead of MP3-Playback
               nSampleBufferBytes = 0; // no output from user-mp3-file
               bool bSayMinuteAsIntro = (m_nPlayFastStartSampleIdx < 0);
               int64_t nPositionMinutesInFile = SetFastPlaybackPosInMp3File();
               if (nPositionMinutesInFile >= 0)
               {
                  bPlayAudioFeedback = true;
                  m_AudioFeedback.ReportMp3FilePosition(nPositionMinutesInFile, bSayMinuteAsIntro);
               }
               else
               {
                  // new position is outside of the mp3-duration
                  bPlayingFile = false; // need next or previous file
               }
               err = MPG123_OK;
            }
            else
            {
               m_nPlayFastStartSampleIdx = -1;
               err = mpg123_read( m_pHandleMp3File, m_pSampleBuffer, SAMPLE_BUFFER_SIZE, /*OUT:*/&nSampleBufferBytes );
               if (err == MPG123_NEW_FORMAT && nSampleBufferBytes == 0)
               {
                  err = mpg123_read( m_pHandleMp3File, m_pSampleBuffer, SAMPLE_BUFFER_SIZE, /*OUT:*/&nSampleBufferBytes );
               }
               if (err == MPG123_DONE && nSampleBufferBytes > 0)
               {
                   err = MPG123_OK;
               }
            }
            if (!bPlayingFile)
            {
               // Fast Playback: need next/previous file
            }
            else if (err == MPG123_OK)
            {
               int64_t nSamples  = mpg123_tell_64(m_pHandleMp3File);
               double  dSeconds  = (double)nSamples / SAMPLE_RATE;
               PlaybackInfo info = m_pFileList->GetCurrentFile();
               info.SetPlaybackPos(dSeconds);
               m_pFileList->UpdatePlaybackPosByPlayer(info);

               std::lock_guard<std::mutex> guard(m_MutexPlayback);
               m_CurrentPlayback = info;
            }
            else if (err != MPG123_DONE)
            {
               if (m_pFileList->GetCurrentFile().GetNofDirs() < 1 && m_pFileList->GetCurrentFile().GetNofFiles() < 1)
               {
                  m_AudioFeedback.ReportNoMp3Files();  // "ATTENTION: no mp3-file found"
               }
               else
               {
                  m_AudioFeedback.ReportMp3FileProblem(m_pFileList->GetCurrentFile());
               }
            }
         }

         size_t nPlayed = 0;
         if (nSampleBufferBytes > 0)
         {
            nPlayed = out123_play(m_pHandleSoundOut, m_pSampleBuffer, nSampleBufferBytes);
         }
         if (nPlayed < 1  && !bPlayAudioFeedback)
         {
            bPlayingFile = false;
         }
      } // while (!m_bStopThread && err == MPG123_OK && bPlayingFile)
      if (m_bPlayFastBack)
      {
         m_pFileList->SetPrevFile();
      }
      else
      {
         m_pFileList->SetNextFile();
      }
      mpg123_close(m_pHandleMp3File);
   } // while !StopThread

   mpg123_close(m_pHandleMp3File);
   mpg123_delete(m_pHandleMp3File);
   out123_del(m_pHandleSoundOut);
   delete[] m_pSampleBuffer;
   m_pHandleMp3File  = nullptr;
   m_pHandleSoundOut = nullptr;
   m_pSampleBuffer   = nullptr;
}


// ----------------------------------------------------------------------------
 // returns: new position in minutes / -1: file-limits exceeded
int64_t AudioPlayer::PrivateData::SetFastPlaybackPosInMp3File()
{
   int nNewPositionInMinutes = -1;
   if (m_bPlayFastForw || m_bPlayFastBack)
   {
      int64_t nOldSampleIdx = mpg123_tell_64(m_pHandleMp3File);
      int64_t nNewSampleIdx = nOldSampleIdx;
      if (m_nPlayFastStartSampleIdx < 0)
      {
         m_nPlayFastStartSampleIdx = nOldSampleIdx;
      }
      int64_t nSamplesPerMinute = 60*SAMPLE_RATE;
      int64_t nOldDifferenceMinutes = 0;
      if (m_bPlayFastForw)
         nOldDifferenceMinutes = (nOldSampleIdx - m_nPlayFastStartSampleIdx)/nSamplesPerMinute;
      else
         nOldDifferenceMinutes = (m_nPlayFastStartSampleIdx - nOldSampleIdx)/nSamplesPerMinute;
      int64_t nStepSizeMinutes  = 1;
      if (nOldDifferenceMinutes > 31)
        nStepSizeMinutes = 10;
      else if (nOldDifferenceMinutes > 16)
        nStepSizeMinutes = 5;
      else if (nOldDifferenceMinutes > 7)
        nStepSizeMinutes = 2;
      int64_t nStepSizeSamples = nSamplesPerMinute * nStepSizeMinutes;
      if (m_bPlayFastForw)
      {
         nNewSampleIdx = nOldSampleIdx + nStepSizeSamples + nStepSizeSamples/8;
         nNewSampleIdx = nStepSizeSamples * (nNewSampleIdx / nStepSizeSamples);
         if (nNewSampleIdx < nOldSampleIdx + nSamplesPerMinute / 6)
            nNewSampleIdx += nStepSizeSamples;
      }
      else
      {
         nNewSampleIdx = nOldSampleIdx - nStepSizeSamples/2;
         nNewSampleIdx = nStepSizeSamples * (nNewSampleIdx / nStepSizeSamples);
         if (nNewSampleIdx > nOldSampleIdx - nSamplesPerMinute / 6)
            nNewSampleIdx -= nStepSizeSamples;
      }
      if (nNewSampleIdx >= 0 && nNewSampleIdx <= m_nCurrentFileSampleLength)
      {
         mpg123_seek_64(m_pHandleMp3File, nNewSampleIdx, SEEK_SET);
         int64_t nChangedSampleIdx = mpg123_tell_64(m_pHandleMp3File);
         if (m_bPlayFastForw && nChangedSampleIdx > nOldSampleIdx)
            nNewPositionInMinutes = nNewSampleIdx / nSamplesPerMinute;
         else if (m_bPlayFastBack && nChangedSampleIdx < nOldSampleIdx)
            nNewPositionInMinutes = nNewSampleIdx / nSamplesPerMinute;
      }
   }
   return nNewPositionInMinutes;
}

// ----------------------------------------------------------------------------

void AudioPlayer::PrivateData::SetupPlaybackFile()
{
   m_nPlayFastStartSampleIdx = -1;
   mpg123_close(m_pHandleMp3File);
   PlaybackInfo file = m_pFileList->GetCurrentFile();
   if (file.GetNofDirs() < 1 && file.GetNofFiles() < 1)
   {
      m_AudioFeedback.ReportNoMp3Files();
   }
   mpg123_open(m_pHandleMp3File, file.GetFilePath().c_str());
   mpg123_format_none(m_pHandleMp3File);
   mpg123_format(m_pHandleMp3File, SAMPLE_RATE, CHANNELS, ENCODING);
   mpg123_scan(m_pHandleMp3File);
   mpg123_volume(m_pHandleMp3File, m_VolumeConversion.ConvertPercentToDouble(m_nVolumePct));
   mpg123_seek_frame_64(m_pHandleMp3File, -1, SEEK_END);
   m_nCurrentFileSampleLength = mpg123_tell_64(m_pHandleMp3File);
   if (m_bPlayFastBack)
   {
      SetFastPlaybackPosInMp3File();
      m_nPlayFastStartSampleIdx = mpg123_tell_64(m_pHandleMp3File);
      double dSeconds  = (double)m_nPlayFastStartSampleIdx / SAMPLE_RATE;
      file.SetPlaybackPos(dSeconds);
      m_AudioFeedback.FastPlaybackFileChanged(file);
   }
   else
   {
      mpg123_seek_frame_64(m_pHandleMp3File, 0, SEEK_SET);
      if (m_bPlayFastForw)
      {
         m_nPlayFastStartSampleIdx = 0;
         double dSeconds  = (double)m_nPlayFastStartSampleIdx / SAMPLE_RATE;
         file.SetPlaybackPos(dSeconds);
         m_AudioFeedback.FastPlaybackFileChanged(file);
      }
   }
   if (file.GetPlaybackPos() > 2.0)
   {
      int64_t nSamples = static_cast<int64_t>(file.GetPlaybackPos()*SAMPLE_RATE);
      nSamples -= 2*SAMPLE_RATE; // some frames before current pos
      nSamples = std::max((int64_t)0, nSamples);
      mpg123_seek_64(m_pHandleMp3File, nSamples, SEEK_SET);
   }

   std::lock_guard<std::mutex> guard(m_MutexPlayback);
   m_CurrentPlayback = file;
}

// ----------------------------------------------------------------------------

void AudioPlayer::PrivateData::ChangeVolume(int nDeltaPct)
{
   int nNewVolumePct = m_nVolumePct + nDeltaPct;
   nNewVolumePct = std::max( 20, nNewVolumePct);
   nNewVolumePct = std::min(100, nNewVolumePct);
   if (nNewVolumePct != m_nVolumePct)
   {
      m_nVolumePct = nNewVolumePct;
      m_AudioFeedback.VolumeChanged(nNewVolumePct);
   }
}

// ----------------------------------------------------------------------------

void AudioPlayer::PrivateData::FileNext(int nSteps)
{
   bool bDirChanged = false;
   PlaybackInfo infoBefore = m_pFileList->GetCurrentFile();
   PlaybackInfo infoAfter  = m_pFileList->GetCurrentFile();

   for (int i = 0; (i < nSteps) && (!bDirChanged); i++)
   {
      m_pFileList->SetNextFile();
      infoAfter  = m_pFileList->GetCurrentFile();
      if (infoAfter.GetDirNumber() != infoBefore.GetDirNumber())
      {
         bDirChanged = true;
      }
   }

   if (bDirChanged)
      m_AudioFeedback.DirChanged(m_pFileList->GetCurrentFile());
   else
      m_AudioFeedback.FileChanged(m_pFileList->GetCurrentFile());
   SetupPlaybackFile();
}

// ----------------------------------------------------------------------------

void AudioPlayer::PrivateData::FilePrev(int nSteps)
{
   bool bDirChanged = false;
   PlaybackInfo infoBefore = m_pFileList->GetCurrentFile();
   PlaybackInfo infoAfter  = m_pFileList->GetCurrentFile();

   for (int i = 0; (i < nSteps) && (!bDirChanged); i++)
   {
      m_pFileList->SetPrevFile();
      infoAfter  = m_pFileList->GetCurrentFile();
      if (infoAfter.GetDirNumber() != infoBefore.GetDirNumber())
      {
         bDirChanged = true;
      }
   }

   if (bDirChanged)
      m_AudioFeedback.DirChanged(m_pFileList->GetCurrentFile());
   else
      m_AudioFeedback.FileChanged(m_pFileList->GetCurrentFile());
   SetupPlaybackFile();
}

// ----------------------------------------------------------------------------

// returns 0 if a message is in message-queue
int  AudioPlayer::PrivateData::FileFastStepsize()
{
   int nStepSize = 0;

   if (m_AudioFeedback.GetNofMessagesInQueue() == 0)
   {
      PlaybackInfo infoCurrent = m_pFileList->GetCurrentFile();
      if (m_nFileFastStartNumber < 0)
      {
         m_nFileFastStartNumber = infoCurrent.GetFileNumber();
      }
      int nDifferenceToStart = std::abs(m_nFileFastStartNumber - infoCurrent.GetFileNumber());

      if (nDifferenceToStart < 7)
         nStepSize = 1;
      else if (nDifferenceToStart < 19)
         nStepSize = 2;
      else if (nDifferenceToStart < 55)
         nStepSize = 5;
      else if (nDifferenceToStart < 115)
         nStepSize = 10;
      else if (nDifferenceToStart < 235)
         nStepSize = 20;
      else
         nStepSize = 50;

      int nMaxStepSize = 50;
      if (infoCurrent.GetNofFiles() < 400)
         nMaxStepSize = 20;
      if (infoCurrent.GetNofFiles() < 200)
         nMaxStepSize = 10;
      if (infoCurrent.GetNofFiles() < 100)
         nMaxStepSize = 5;
      if (infoCurrent.GetNofFiles() < 50)
         nMaxStepSize = 2;
      if (infoCurrent.GetNofFiles() < 20)
         nMaxStepSize = 1;

      nStepSize = std::min(nStepSize, nMaxStepSize);
   }
   return nStepSize;
}

// ----------------------------------------------------------------------------

void AudioPlayer::PrivateData::FileFastForw()
{
   int nStepSize = FileFastStepsize();
   if (nStepSize > 0)
   {
      PlaybackInfo infoCurrent = m_pFileList->GetCurrentFile();
      int nNextFile  = nStepSize * ((infoCurrent.GetFileNumber()+nStepSize)/nStepSize);
      int nNofSteps  = nNextFile - infoCurrent.GetFileNumber();
      FileNext(nNofSteps);
   }
}

// ----------------------------------------------------------------------------

void AudioPlayer::PrivateData::FileFastBack()
{
   int nStepSize = FileFastStepsize();
   if (nStepSize > 0)
   {
      PlaybackInfo infoCurrent = m_pFileList->GetCurrentFile();
      int nNextFile  = nStepSize * ((infoCurrent.GetFileNumber()-1)/nStepSize);
      int nNofSteps  = infoCurrent.GetFileNumber() - nNextFile;
      FilePrev(nNofSteps);
   }
}

// ----------------------------------------------------------------------------

void AudioPlayer::PrivateData::DirNext()
{
   m_pFileList->SetNextDir();
   m_AudioFeedback.DirChanged(m_pFileList->GetCurrentFile());
   SetupPlaybackFile();
}

// ----------------------------------------------------------------------------

void AudioPlayer::PrivateData::DirPrev()
{
   m_pFileList->SetPrevDir();
   m_AudioFeedback.DirChanged(m_pFileList->GetCurrentFile());
   SetupPlaybackFile();
}

// ----------------------------------------------------------------------------

void AudioPlayer::PrivateData::InfoRequest()
{
   m_AudioFeedback.InfoRequest(m_pFileList->GetCurrentFile());
}

// ----------------------------------------------------------------------------

void AudioPlayer::PrivateData::AddUserRequest(KeyInput::KEY  key)
{
   std::lock_guard<std::mutex> guard(m_MutexUserReq);
   m_listUserRequests.push_back(key);
}


// ============================================================================

AudioPlayer::AudioPlayer(
   int nVolumePct,
   Mp3DirFileList* pFiles,
   int nAutoShutdownMinutes,
   bool bChecksumProblem)
{
   m_pPriv = std::make_unique<AudioPlayer::PrivateData>(
      nVolumePct, pFiles, nAutoShutdownMinutes, bChecksumProblem);
}

// ----------------------------------------------------------------------------

AudioPlayer::~AudioPlayer()
{
}

// ----------------------------------------------------------------------------

void AudioPlayer::AddUserRequest(KeyInput::KEY  key)
{
   m_pPriv->AddUserRequest(key);
}

// ----------------------------------------------------------------------------

bool AudioPlayer::IsShutdown()
{
   return m_pPriv->m_bShutdown;
}

// ----------------------------------------------------------------------------

bool AudioPlayer::IsServiceMode()
{
   return m_pPriv->m_bServiceMode;
}

// ----------------------------------------------------------------------------

int  AudioPlayer::GetVolumeInPct()
{
   return m_pPriv->m_nVolumePct;
}

// ----------------------------------------------------------------------------

PlaybackInfo AudioPlayer::GetCurrentPlaybackInfo()
{
   std::lock_guard<std::mutex> guard(m_pPriv->m_MutexPlayback);
   return m_pPriv->m_CurrentPlayback;
}

