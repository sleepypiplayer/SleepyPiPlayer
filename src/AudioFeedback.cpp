// ----------------------------------------------------------------------------
// SleepyPiPlayer:  AudioFeedback
//     play audio-feedback-messages (probably as feedback to pressed keys)
// license: free software   (sleepypiplayer(at)saftfresse.de)  [A.D.2025]
// - Do whatever you want with the software.
// - Do not claim my work as your own.
// - THIS SOFTWARE COMES WITH NO WARRANTIES, USE AT YOUR OWN RISK
// ----------------------------------------------------------------------------
#include "AudioFeedback.h"

#include "SystemSoundCatalog.h"
#include "NumberToSound.h"
#include "AudioVolumeConversion.h"
#include <thread>
#include <mutex>
#include <chrono>
#include <list>
#include <vector>
#define _FILE_OFFSET_BITS 64    // enable usage of mpg123_length_64()
#include <out123.h>
#include <mpg123.h>



// ============================================================================

/** one message like "Volume 78 */
class AudioMessage
{
public:
   enum class TYPE { UNKNOWN, VOLUME, FILE, DIR, INFO, SYSTEM, POSITION };

   AudioMessage( TYPE  eType, long nSampleRate, int nChannels, int nEncoding, const std::list<std::string>&  listMp3Main, std::string txtSilence )
   : m_listMp3Main(listMp3Main)
   , m_eType(eType)
   , m_txtSilence(txtSilence)
   , SAMPLE_RATE{nSampleRate}
   , CHANNELS(nChannels)
   , ENCODING(nEncoding)
   {
   }
   virtual ~AudioMessage()
   {
      if (m_pHandleMp3File)
      {
         mpg123_close(m_pHandleMp3File);
         mpg123_delete(m_pHandleMp3File);
         m_pHandleMp3File = nullptr;
      }
   }

   // intro information can be skipped if another message of the same type is currently played
   //   intro: e.g. "File"   or  "Directory"   or  "Volume"
   // example:       "File 12 "+"of 26"  followed by  "File 13"+"of 26"
   //     results in "File 12 "          followed by       "13 of 26 "
   void SetIntroduction(const std::list<std::string>&  listMp3FilePath)
   {
      m_listMp3Intro = listMp3FilePath;
   }
   // no intro because same message type is currently played
   void SkipIntroduction()
   {
      m_listMp3Intro.clear();
   }

   // extra information can be skipped if another message is added to the queue
   //   ExtendedOutput: e.g. "of 26"
   // example:       "File 12 "+"of 26"  followed by  "File 13"+"of 26"
   //     results in "File 12 "          followed by       "13 of 26 "
   //    (the second "File" is omitted because TYPE::FILE is already in the queue)
   void SetExtendedOutput(const std::list<std::string>&  listMp3FilePath)
   {
      m_listMp3Extended = listMp3FilePath;
   }

   // no main-info because another message of the same type waits to be played
   // Introduction: "File"  Main: "12"  Extended: "of 26"
   // example "File"+"12"+"of 26"  followed by  "File"+"13"+"of 26"
   // will result in  ("File") + ("13" + "of 26")
   void SkipMainOutput()
   {
      m_listMp3Main.clear();
      m_listMp3Extended.clear();
   }
   bool PlaysMainOutput() // if already played: do not skip
   {
      return m_bPlayingMainOutput;
   }

   // no extra-info because another message waits to be played
   void SkipExtendedOutput()
   {
      m_listMp3Extended.clear();
      if (PlaysExtendedOutput())
      {
         m_listPlayback.clear();
      }
   }
   bool PlaysExtendedOutput()
   {
      return m_bPlayingExtendedOutput;
   }
   bool PlaysExtendedNonSilence()
   {
      return m_bPlayingExtendedNonSilence;
   }

   // will be played even if the ExtendedOutput is skipped
   // probably some short silence is added
   void SetFinalOutput(const std::list<std::string>&  listMp3FilePath)
   {
      m_listMp3Final = listMp3FilePath;
   }

   TYPE GetType() { return m_eType; }

   // get next chunk of decoded audio-samples
   //  returns false if the message is already played
   bool GetAudioSamples(
      double dVolume,
      unsigned char* pSampleBuffer,
      size_t SAMPLE_BUFFER_SIZE,
      size_t* /*OUT:*/pSampleBufferBytes);


private:
   std::list<std::string> m_listMp3Intro;      // e.g. "File" introduction  played first - skipped if same type is currently playing
   std::list<std::string> m_listMp3Main;       // e.g. "23"   standard-text that will be played  list of MP3-File-paths
   std::list<std::string> m_listMp3Extended;   // e.g. "of 56"   extended-text that could be skipped
   std::list<std::string> m_listMp3Final;      // e.g. " "  silence will be played even if extended was skipped

   std::list<std::string> m_listPlayback;      // list of currently played files filled with Intro/FilePath/Extended/Final

   TYPE    m_eType = TYPE::UNKNOWN;            //  FILE/DIR/VOLUME/INFO/...
   bool m_bPlayingMainOutput     = false;      //  started playing main     info: do not skip main  of this message
   bool m_bPlayingExtendedOutput = false;      //  started playing extended info: do not skip intro of next message
   bool m_bPlayingExtendedNonSilence = false;  //  started playing extended info: and a file different from silence is used
   bool m_bNextFile = true;   //  true: open next file for decoding when asked for samples
   std::string m_txtSilence;  //  m_SoundCatalog.Silence()
   const long  SAMPLE_RATE;   //  32000;
   const int   CHANNELS;      //  MPG123_MONO;
   const int   ENCODING;      //  MPG123_ENC_SIGNED_16;
   mpg123_handle* m_pHandleMp3File  = nullptr;
};

// ----------------------------------------------------------------------------

bool AudioMessage::GetAudioSamples(
   double dVolume,
   unsigned char* pSampleBuffer,
   size_t SAMPLE_BUFFER_SIZE,
   size_t* /*OUT:*/pSampleBufferBytes)
{
   bool bPlayAudioFeedback = false;
   int err = MPG123_OK;
   if (m_pHandleMp3File == nullptr)
   {
      m_pHandleMp3File  = mpg123_new(nullptr, &err);
      mpg123_param(m_pHandleMp3File, MPG123_VERBOSE, 0, 0);
      long   nMpg123Flags = 0;
      double dMpg123Dummy = 0.0;
      mpg123_getparam(m_pHandleMp3File, MPG123_FLAGS, &nMpg123Flags, &dMpg123Dummy);
      mpg123_param(m_pHandleMp3File, MPG123_FLAGS, MPG123_QUIET | nMpg123Flags, 0);
   }
   err = MPG123_OK;
   *pSampleBufferBytes = 0;

   if (m_listPlayback.size() == 0)
   {
      m_listPlayback = m_listMp3Intro;
      m_listMp3Intro.clear();
      if (m_listPlayback.size() < 1)
      {
         m_bPlayingMainOutput = true;
         m_listPlayback = m_listMp3Main;
         m_listMp3Main.clear();
      }
   }
   while (!bPlayAudioFeedback && m_listPlayback.size() > 0)
   {
      if (m_bNextFile)
      {
         m_bNextFile = false;
         mpg123_close(m_pHandleMp3File);
         mpg123_open( m_pHandleMp3File, m_listPlayback.front().c_str());
         mpg123_format_none(m_pHandleMp3File);
         mpg123_format(m_pHandleMp3File, SAMPLE_RATE, CHANNELS, ENCODING);
      }
      mpg123_volume(m_pHandleMp3File, dVolume);
      err = mpg123_read( m_pHandleMp3File, pSampleBuffer, SAMPLE_BUFFER_SIZE, /*OUT:*/pSampleBufferBytes );
      if (err == MPG123_NEW_FORMAT && *pSampleBufferBytes == 0)
      {
         err = mpg123_read( m_pHandleMp3File, pSampleBuffer, SAMPLE_BUFFER_SIZE, /*OUT:*/pSampleBufferBytes );
      }
      if (err == MPG123_DONE && *pSampleBufferBytes > 0)
      {
         err = MPG123_OK;
      }
      if (err != MPG123_OK  || *pSampleBufferBytes == 0)
      {
         m_bNextFile = true;
         if (m_bPlayingExtendedOutput)
         {
            if (m_listPlayback.front() != m_txtSilence)
            {
               m_bPlayingExtendedNonSilence = true;
            }
         }
         m_listPlayback.pop_front();
         if (m_listPlayback.size() == 0)
         {
            if (m_listMp3Main.size() > 0)
            {
               m_bPlayingMainOutput = true;
               m_listPlayback = m_listMp3Main;
               m_listMp3Main.clear();
            }
            else if (m_listMp3Extended.size() > 0)
            {
               m_listPlayback = m_listMp3Extended;
               m_listMp3Extended.clear();
               m_bPlayingExtendedOutput = true;
               m_bPlayingMainOutput = true;
            }
            else if (m_listMp3Final.size() > 0)
            {
               m_listPlayback = m_listMp3Final;
               m_listMp3Final.clear();
            }
         }
      }
      else
      {
         bPlayAudioFeedback = true;
      }
   }
   return bPlayAudioFeedback;
}


// ============================================================================


class AudioFeedback::PrivateData
{
public:
   PrivateData(long nSampleRate, int nChannels, int nEncoding)
   : SAMPLE_RATE{nSampleRate}
   , CHANNELS(nChannels)
   , ENCODING(nEncoding)
   {
      m_NumberToSound.reset(new NumberToSound());
   }

   virtual ~PrivateData()
   {
   }

   bool   GetAudioSamples(unsigned char* pSampleBuffer, size_t SAMPLE_BUFFER_SIZE, size_t* /*OUT:*/pSampleBufferBytes);
   double GetVolumeAsDouble();

   bool IsInMessageQueue(AudioMessage::TYPE eType);
   void RemoveFromMessageQueue(AudioMessage::TYPE eType);
   void PutInMessageQueue(AudioMessage::TYPE eType, const std::list<std::string>&  listMp3FilePath);
   void PutInMessageQueue(AudioMessage::TYPE eType,
                          const std::list<std::string>&  listMp3Intro,
                          const std::list<std::string>&  listMp3FilePath);
   void PutInMessageQueue(AudioMessage::TYPE eType,
                          const std::list<std::string>&  listMp3Intro,
                          const std::list<std::string>&  listMp3FilePath,
                          const std::list<std::string>&  listMp3Extended);

   void ReportChecksumProblem();
   void ReportStartupPosition(PlaybackInfo info);
   void ReportMp3FilePosition(int nMinutesInFile, bool bTextIncudingIntro);
   void ReportMp3FileProblem(PlaybackInfo info);
   void ReportNoMp3Files();
   void VolumeChanged(int nVolumePct);
   void FileChanged(PlaybackInfo info);
   void FastPlaybackFileChanged(PlaybackInfo info);
   void DirChanged(PlaybackInfo info);
   void InfoRequest(PlaybackInfo info);

   void ReportShutdown(); // countdown 9, 8, 7, ... 0
   void CancelShutdown();
   bool IsReportingShutdown();
   bool IsShutdownReported();

   void ReportServiceMode();
   bool IsServiceModeReported();

   const long  SAMPLE_RATE;   //  32000;
   const int   CHANNELS;      //  MPG123_MONO;
   const int   ENCODING;      //  MPG123_ENC_SIGNED_16;

   std::unique_ptr<NumberToSound>  m_NumberToSound;
   SystemSoundCatalog   m_SoundCatalog;

   int                   m_nVolumePct      = 80;
   AudioVolumeConversion m_VolumeConversion;

   bool                  m_bReportingShutdown = false;
   bool                  m_bReportingService  = false;

   std::list<std::shared_ptr<AudioMessage>>  m_listAudioMessages;  // pop_front() / push_back()
};

// ----------------------------------------------------------------------------

double AudioFeedback::PrivateData::GetVolumeAsDouble()
{
   return m_VolumeConversion.ConvertPercentToDouble(m_nVolumePct);
}

// ----------------------------------------------------------------------------

bool AudioFeedback::PrivateData::IsInMessageQueue(AudioMessage::TYPE eType)
{
   bool bIsInQueue = false;
   for (auto pMessage : m_listAudioMessages)
   {
      if (pMessage->GetType() == eType)
      {
         bIsInQueue = true;
      }
   }
   return bIsInQueue;
}

// ----------------------------------------------------------------------------

void AudioFeedback::PrivateData::RemoveFromMessageQueue(AudioMessage::TYPE eType)
{
   std::list<std::shared_ptr<AudioMessage>>  newList;
   for (auto pMessage : m_listAudioMessages)
   {
      if (pMessage->GetType() != eType)
      {
         newList.push_back(pMessage);
      }
   }
   m_listAudioMessages = newList;
}

// ----------------------------------------------------------------------------

void AudioFeedback::PrivateData::PutInMessageQueue(
       AudioMessage::TYPE eType,
       const std::list<std::string>&  listMp3Intro,
       const std::list<std::string>&  listMp3FilePath,
       const std::list<std::string>&  listMp3Extended)
{
   std::shared_ptr<AudioMessage> pNewMessage =
      std::make_shared<AudioMessage>( eType, SAMPLE_RATE, CHANNELS, ENCODING, listMp3FilePath, m_SoundCatalog.Silence() );
   pNewMessage->SetIntroduction(listMp3Intro);
   pNewMessage->SetExtendedOutput(listMp3Extended);
   if (listMp3FilePath.size() > 0)
   {
      std::list<std::string>  listMp3Final;
      listMp3Final.push_back(m_SoundCatalog.Silence());
      listMp3Final.push_back(m_SoundCatalog.Silence());
      pNewMessage->SetFinalOutput(listMp3Final);
   }

   if (eType == AudioMessage::TYPE::FILE || eType == AudioMessage::TYPE::DIR || eType == AudioMessage::TYPE::VOLUME)
   {
      bool bFirst = true;
      std::list<std::shared_ptr<AudioMessage>>  newList;
      for (auto pMessage : m_listAudioMessages)
      {
         if (bFirst)
         {
            bFirst = false;
            pMessage->SkipExtendedOutput();
            newList.push_back(pMessage);
         }
         else if (pMessage->GetType() != eType)
         {
            pMessage->SkipExtendedOutput();
            newList.push_back(pMessage);
         }
      }
      m_listAudioMessages = newList;
   }
   if (m_listAudioMessages.size() > 0)
   {
      if (m_listAudioMessages.back()->GetType() == eType)
      {
         if (! m_listAudioMessages.back()->PlaysExtendedOutput())
         {
            pNewMessage->SkipIntroduction(); // e.g. "16" instead of "File 16"
         }
         else if (m_listAudioMessages.size() == 1 && m_listAudioMessages.back()->PlaysExtendedOutput())
         {
            if (eType == AudioMessage::TYPE::FILE && !m_listAudioMessages.back()->PlaysExtendedNonSilence())
            {
               pNewMessage->SkipIntroduction(); // still playing silence: Fast-File-Forward: "File 16  17"  instead of "File 16  File 17"
            }
         }
         if (eType == AudioMessage::TYPE::FILE || eType == AudioMessage::TYPE::DIR || eType == AudioMessage::TYPE::VOLUME)
         {
            if (! m_listAudioMessages.back()->PlaysMainOutput())
            {
               m_listAudioMessages.back()->SkipMainOutput(); // e.g. "File" instead of "File 16"  (next message: [skipped: "File"] "17 of 26")
            }
         }
      }
   }
   m_listAudioMessages.push_back( pNewMessage );
}

// ----------------------------------------------------------------------------

void AudioFeedback::PrivateData::PutInMessageQueue(
       AudioMessage::TYPE eType,
       const std::list<std::string>&  listMp3Intro,
       const std::list<std::string>&  listMp3FilePath)
{
   const std::list<std::string> listExtendedEmpty;
   PutInMessageQueue(eType, listMp3Intro, listMp3FilePath, listExtendedEmpty);
}

// ----------------------------------------------------------------------------

void AudioFeedback::PrivateData::PutInMessageQueue(
   AudioMessage::TYPE eType, const std::list<std::string>&  listMp3FilePath)
{
   const std::list<std::string> listIntroEmpty;
   PutInMessageQueue(eType, listIntroEmpty, listMp3FilePath);
}

// ----------------------------------------------------------------------------

void AudioFeedback::PrivateData::VolumeChanged(int nVolumePct)
{
   m_nVolumePct = nVolumePct;

   std::list<std::string> listAudioIntro;
   listAudioIntro.push_back(m_SoundCatalog.Text_Volume());
   listAudioIntro.push_back(m_SoundCatalog.Silence());

   std::list<std::string> listAudioMain;
   m_NumberToSound->AddNumber(listAudioMain, nVolumePct);
   listAudioMain.push_back(m_SoundCatalog.Silence());

   RemoveFromMessageQueue(AudioMessage::TYPE::FILE);
   RemoveFromMessageQueue(AudioMessage::TYPE::DIR);
   RemoveFromMessageQueue(AudioMessage::TYPE::INFO);
   PutInMessageQueue(AudioMessage::TYPE::VOLUME, listAudioIntro, listAudioMain);
}

// ----------------------------------------------------------------------------

void AudioFeedback::PrivateData::ReportChecksumProblem()
{
   std::list<std::string> listAudioMain;
   listAudioMain.push_back(m_SoundCatalog.Text_ChecksumProblem());
   listAudioMain.push_back(m_SoundCatalog.Silence());
   PutInMessageQueue(AudioMessage::TYPE::SYSTEM, listAudioMain);
}

// ----------------------------------------------------------------------------

void AudioFeedback::PrivateData::ReportStartupPosition(PlaybackInfo info)
{
   std::list<std::string> listAudioMain;
   listAudioMain.push_back(m_SoundCatalog.Silence());
   listAudioMain.push_back(m_SoundCatalog.Text_File());
   listAudioMain.push_back(m_SoundCatalog.Silence());
   m_NumberToSound->AddNumber(listAudioMain, info.GetFileNumber());
   listAudioMain.push_back(m_SoundCatalog.Silence());
   listAudioMain.push_back(m_SoundCatalog.Text_Of());
   m_NumberToSound->AddNumber(listAudioMain, info.GetNofFiles());
   listAudioMain.push_back(m_SoundCatalog.Silence());
   listAudioMain.push_back(m_SoundCatalog.Text_Minute());
   int nMinute = (int)((info.GetPlaybackPos()+30.0) / 60.0);
   m_NumberToSound->AddNumber(listAudioMain,nMinute);
   listAudioMain.push_back(m_SoundCatalog.Silence());
   PutInMessageQueue(AudioMessage::TYPE::FILE, listAudioMain);
}

// ----------------------------------------------------------------------------

void AudioFeedback::PrivateData::ReportMp3FilePosition(int nMinutesInFile, bool bTextIncudingIntro)
{
   std::list<std::string> listAudioIntro;
   if (bTextIncudingIntro)
   {
      listAudioIntro.push_back(m_SoundCatalog.Text_Minute());
      listAudioIntro.push_back(m_SoundCatalog.Silence());
   }
   std::list<std::string> listAudioMain;
   m_NumberToSound->AddNumber(listAudioMain, nMinutesInFile);
   listAudioMain.push_back(m_SoundCatalog.Silence());
   PutInMessageQueue(AudioMessage::TYPE::POSITION, listAudioIntro, listAudioMain);
}

// ----------------------------------------------------------------------------

void AudioFeedback::PrivateData::ReportMp3FileProblem(PlaybackInfo info)
{
   std::list<std::string> listAudioIntro;

   std::list<std::string> listAudioMain;
   listAudioMain.push_back(m_SoundCatalog.Text_Mp3FileProblem());
   listAudioMain.push_back(m_SoundCatalog.Silence());

   std::list<std::string> listAudioExtended;
   m_SoundCatalog.SpellString(listAudioExtended, info.GetFileName());
   listAudioExtended.push_back(m_SoundCatalog.Silence());
   PutInMessageQueue(AudioMessage::TYPE::SYSTEM, listAudioIntro, listAudioMain, listAudioExtended);
}

// ----------------------------------------------------------------------------

void AudioFeedback::PrivateData::ReportNoMp3Files()
{
   if (!IsInMessageQueue(AudioMessage::TYPE::SYSTEM))
   {
      std::list<std::string> listAudioMain;
      listAudioMain.push_back(m_SoundCatalog.Text_NoMP3Found());
      listAudioMain.push_back(m_SoundCatalog.Silence());

      PutInMessageQueue(AudioMessage::TYPE::SYSTEM, listAudioMain);
   }
}

// ----------------------------------------------------------------------------

void AudioFeedback::PrivateData::FileChanged(PlaybackInfo info)
{
   std::list<std::string> listAudioIntro;
   listAudioIntro.push_back(m_SoundCatalog.Text_File());
   listAudioIntro.push_back(m_SoundCatalog.Silence());

   std::list<std::string> listAudioMain;
   m_NumberToSound->AddNumber(listAudioMain, info.GetFileNumber());
   listAudioMain.push_back(m_SoundCatalog.Silence());

   std::list<std::string> listAudioExtended;
   listAudioExtended.push_back(m_SoundCatalog.Silence()); // skip extended in FastFileForward
   listAudioExtended.push_back(m_SoundCatalog.Text_Of());
   m_NumberToSound->AddNumber(listAudioExtended, info.GetNofFiles());
   listAudioExtended.push_back(m_SoundCatalog.Silence());

   RemoveFromMessageQueue(AudioMessage::TYPE::VOLUME);
   RemoveFromMessageQueue(AudioMessage::TYPE::INFO);
   PutInMessageQueue(AudioMessage::TYPE::FILE, listAudioIntro, listAudioMain, listAudioExtended);
}

// ----------------------------------------------------------------------------

void AudioFeedback::PrivateData::FastPlaybackFileChanged(PlaybackInfo info)
{
   std::list<std::string> listAudioMain;
   listAudioMain.push_back(m_SoundCatalog.Text_File());
   listAudioMain.push_back(m_SoundCatalog.Silence());
   m_NumberToSound->AddNumber(listAudioMain, info.GetFileNumber());
   listAudioMain.push_back(m_SoundCatalog.Silence());
   listAudioMain.push_back(m_SoundCatalog.Text_Minute());
   int nMinute = (int)((info.GetPlaybackPos()+30.0) / 60.0);
   m_NumberToSound->AddNumber(listAudioMain,nMinute);
   listAudioMain.push_back(m_SoundCatalog.Silence());

   RemoveFromMessageQueue(AudioMessage::TYPE::VOLUME);
   RemoveFromMessageQueue(AudioMessage::TYPE::INFO);
   PutInMessageQueue(AudioMessage::TYPE::FILE, listAudioMain);
}

// ----------------------------------------------------------------------------

void AudioFeedback::PrivateData::DirChanged(PlaybackInfo info)
{
   std::list<std::string> listAudioIntro;
   listAudioIntro.push_back(m_SoundCatalog.Text_Directory());
   listAudioIntro.push_back(m_SoundCatalog.Silence());

   std::list<std::string> listAudioMain;
   m_NumberToSound->AddNumber(listAudioMain, info.GetDirNumber());
   listAudioMain.push_back(m_SoundCatalog.Silence());

   std::list<std::string> listAudioExtended;
   listAudioExtended.push_back(m_SoundCatalog.Text_Of());
   m_NumberToSound->AddNumber(listAudioExtended, info.GetNofDirs());
   listAudioExtended.push_back(m_SoundCatalog.Silence());
   m_SoundCatalog.SpellString(listAudioExtended, info.GetDirName());
   listAudioExtended.push_back(m_SoundCatalog.Silence());
   listAudioExtended.push_back(m_SoundCatalog.Text_File());
   listAudioExtended.push_back(m_SoundCatalog.Silence());
   m_NumberToSound->AddNumber(listAudioExtended, info.GetFileNumber());
   listAudioExtended.push_back(m_SoundCatalog.Text_Of());
   m_NumberToSound->AddNumber(listAudioExtended, info.GetNofFiles());
   listAudioExtended.push_back(m_SoundCatalog.Silence());

   RemoveFromMessageQueue(AudioMessage::TYPE::VOLUME);
   RemoveFromMessageQueue(AudioMessage::TYPE::FILE);
   RemoveFromMessageQueue(AudioMessage::TYPE::INFO);
   PutInMessageQueue(AudioMessage::TYPE::DIR, listAudioIntro, listAudioMain, listAudioExtended);
}

// ----------------------------------------------------------------------------

void AudioFeedback::PrivateData::InfoRequest(PlaybackInfo info)
{
   if (IsInMessageQueue(AudioMessage::TYPE::INFO))
   {
      RemoveFromMessageQueue(AudioMessage::TYPE::INFO);
   }
   else
   {
      std::list<std::string> listAudioMain;
      listAudioMain.push_back(m_SoundCatalog.Text_File());
      listAudioMain.push_back(m_SoundCatalog.Silence());
      m_NumberToSound->AddNumber(listAudioMain, info.GetFileNumber());
      listAudioMain.push_back(m_SoundCatalog.Silence());
      listAudioMain.push_back(m_SoundCatalog.Text_Of());
      m_NumberToSound->AddNumber(listAudioMain, info.GetNofFiles());
      listAudioMain.push_back(m_SoundCatalog.Silence());
      listAudioMain.push_back(m_SoundCatalog.Text_Minute());
      int nMinute = (int)((info.GetPlaybackPos()+10.0) / 60.0);
      m_NumberToSound->AddNumber(listAudioMain,nMinute);
      listAudioMain.push_back(m_SoundCatalog.Silence());
      listAudioMain.push_back(m_SoundCatalog.Text_Directory());
      listAudioMain.push_back(m_SoundCatalog.Silence());
      m_NumberToSound->AddNumber(listAudioMain, info.GetDirNumber());
      listAudioMain.push_back(m_SoundCatalog.Silence());
      listAudioMain.push_back(m_SoundCatalog.Text_Of());
      m_NumberToSound->AddNumber(listAudioMain, info.GetNofDirs());
      listAudioMain.push_back(m_SoundCatalog.Silence());
      listAudioMain.push_back(m_SoundCatalog.Silence());
      m_SoundCatalog.SpellString(listAudioMain, info.GetDirName());
      listAudioMain.push_back(m_SoundCatalog.Silence());
      listAudioMain.push_back(m_SoundCatalog.Silence());
      m_SoundCatalog.SpellString(listAudioMain, info.GetFileName());
      listAudioMain.push_back(m_SoundCatalog.Silence());

      RemoveFromMessageQueue(AudioMessage::TYPE::VOLUME);
      RemoveFromMessageQueue(AudioMessage::TYPE::FILE);
      RemoveFromMessageQueue(AudioMessage::TYPE::DIR);
      RemoveFromMessageQueue(AudioMessage::TYPE::INFO);
      PutInMessageQueue(AudioMessage::TYPE::INFO, listAudioMain);
   }
}

// ----------------------------------------------------------------------------

void AudioFeedback::PrivateData::ReportShutdown() // countdown 9, 8, 7, ... 0
{
   if (!m_bReportingShutdown)
   {
      m_bReportingShutdown = true;
      std::list<std::string> listAudioMain;

      listAudioMain.push_back(m_SoundCatalog.Text_Shutdown());
      listAudioMain.push_back(m_SoundCatalog.Silence());
      for (char digit = '9'; digit >= '0'; digit--)
      {
         listAudioMain.push_back(m_SoundCatalog.GetChar(digit));
         for (int nIdxSilence = 0; nIdxSilence < 3; nIdxSilence++)
         {
            listAudioMain.push_back(m_SoundCatalog.Silence());
         }
      }
      RemoveFromMessageQueue(AudioMessage::TYPE::VOLUME);
      RemoveFromMessageQueue(AudioMessage::TYPE::FILE);
      RemoveFromMessageQueue(AudioMessage::TYPE::DIR);
      RemoveFromMessageQueue(AudioMessage::TYPE::INFO);
      PutInMessageQueue(AudioMessage::TYPE::SYSTEM, listAudioMain);
      // add some silence: avoid playback of user-mp3 before shutdown
      std::list<std::string> listSilence;
      listSilence.push_back(m_SoundCatalog.Silence());
      listSilence.push_back(m_SoundCatalog.Silence());
      listSilence.push_back(m_SoundCatalog.Silence());
      listSilence.push_back(m_SoundCatalog.Silence());
      PutInMessageQueue(AudioMessage::TYPE::INFO, listSilence);
   }
}

// ----------------------------------------------------------------------------

void AudioFeedback::PrivateData::CancelShutdown()
{
   if (m_bReportingShutdown)
   {
      m_bReportingShutdown = false;
      RemoveFromMessageQueue(AudioMessage::TYPE::SYSTEM);
   }
}

// ----------------------------------------------------------------------------

bool AudioFeedback::PrivateData::IsReportingShutdown() // still output 4, 3, 2
{
   bool bReporting = false;
   if (m_bReportingShutdown)
   {
      if (IsInMessageQueue(AudioMessage::TYPE::SYSTEM))
      {
         bReporting = true;
      }
   }
   return bReporting;
}

// ----------------------------------------------------------------------------

bool AudioFeedback::PrivateData::IsShutdownReported()
{
   bool bShutdown = false;
   if (m_bReportingShutdown)
   {
      if (!IsInMessageQueue(AudioMessage::TYPE::SYSTEM))
      {
         bShutdown = true;
      }
   }
   return bShutdown;
}

// ----------------------------------------------------------------------------

void AudioFeedback::PrivateData::ReportServiceMode()
{
   if (!m_bReportingService)
   {
      m_bReportingService = true;
      std::list<std::string> listAudioMain;
      listAudioMain.push_back(m_SoundCatalog.Text_Service());
      listAudioMain.push_back(m_SoundCatalog.Silence());
      RemoveFromMessageQueue(AudioMessage::TYPE::VOLUME);
      RemoveFromMessageQueue(AudioMessage::TYPE::FILE);
      RemoveFromMessageQueue(AudioMessage::TYPE::DIR);
      RemoveFromMessageQueue(AudioMessage::TYPE::INFO);
      PutInMessageQueue(AudioMessage::TYPE::SYSTEM, listAudioMain);
      // add some silence: avoid playback of user-mp3 before service-mode
      std::list<std::string> listSilence;
      listSilence.push_back(m_SoundCatalog.Silence());
      listSilence.push_back(m_SoundCatalog.Silence());
      listSilence.push_back(m_SoundCatalog.Silence());
      listSilence.push_back(m_SoundCatalog.Silence());
      PutInMessageQueue(AudioMessage::TYPE::INFO, listSilence);
   }
}

// ----------------------------------------------------------------------------

bool AudioFeedback::PrivateData::IsServiceModeReported()
{
   bool bServiceMode = false;
   if (m_bReportingService)
   {
      if (!IsInMessageQueue(AudioMessage::TYPE::SYSTEM))
      {
         bServiceMode = true;
      }
   }
   return bServiceMode;
}

// ----------------------------------------------------------------------------

bool AudioFeedback::PrivateData::GetAudioSamples(
   unsigned char* pSampleBuffer,
   size_t SAMPLE_BUFFER_SIZE,
   size_t* /*OUT:*/pSampleBufferBytes )
{
   bool bPlayAudioFeedback = false;
   if (pSampleBufferBytes)
   {
      *pSampleBufferBytes = 0;
   }
   while (!bPlayAudioFeedback && m_listAudioMessages.size() > 0)
   {
      bPlayAudioFeedback = m_listAudioMessages.front()->GetAudioSamples(
         GetVolumeAsDouble(), pSampleBuffer, SAMPLE_BUFFER_SIZE, pSampleBufferBytes );
      if (!bPlayAudioFeedback)
      {
         m_listAudioMessages.pop_front();
      }
   }

   return bPlayAudioFeedback;
}

// ============================================================================

AudioFeedback::AudioFeedback(long nSampleRate, int nChannels, int nEncoding)
{
   m_pPriv = std::make_unique<AudioFeedback::PrivateData>(nSampleRate, nChannels, nEncoding);
}

// ----------------------------------------------------------------------------

AudioFeedback::~AudioFeedback()
{
}

// ----------------------------------------------------------------------------

void AudioFeedback::SetVolume(int nVolumePct)
{
   m_pPriv->m_nVolumePct = nVolumePct;
}

// ----------------------------------------------------------------------------

int AudioFeedback::GetNofMessagesInQueue()
{
   int nNofMessages = m_pPriv->m_listAudioMessages.size();
   if (nNofMessages == 1)
   {
      if (m_pPriv->m_listAudioMessages.front()->PlaysExtendedOutput())
      {
         nNofMessages = 0; // skip extended in FastFileForward
      }
   }
   return nNofMessages;
}

// ----------------------------------------------------------------------------

bool AudioFeedback::GetAudioSamples(unsigned char* pSampleBuffer, size_t SAMPLE_BUFFER_SIZE, size_t* /*OUT:*/pSampleBufferBytes)
{
   return m_pPriv->GetAudioSamples(pSampleBuffer, SAMPLE_BUFFER_SIZE, pSampleBufferBytes);
}

// ----------------------------------------------------------------------------

void AudioFeedback::VolumeChanged(int nVolumePct)
{
   m_pPriv->VolumeChanged(nVolumePct);
}

// ----------------------------------------------------------------------------

void AudioFeedback::FileChanged(PlaybackInfo info)
{
   m_pPriv->FileChanged(info);
}

// ----------------------------------------------------------------------------

void AudioFeedback::FastPlaybackFileChanged(PlaybackInfo info)
{
   m_pPriv->FastPlaybackFileChanged(info);
}

// ----------------------------------------------------------------------------

void AudioFeedback::DirChanged(PlaybackInfo info)
{
   m_pPriv->DirChanged(info);
}

// ----------------------------------------------------------------------------

void AudioFeedback::InfoRequest(PlaybackInfo info)
{
   m_pPriv->InfoRequest(info);
}

// ----------------------------------------------------------------------------

void AudioFeedback::ReportChecksumProblem()
{
   m_pPriv->ReportChecksumProblem();
}

// ----------------------------------------------------------------------------

void AudioFeedback::ReportStartupPosition(PlaybackInfo info)
{
   m_pPriv->ReportStartupPosition(info);
}

// ----------------------------------------------------------------------------

void AudioFeedback::ReportMp3FilePosition(int nMinutesInFile, bool bTextIncudingIntro)
{
   m_pPriv->ReportMp3FilePosition(nMinutesInFile, bTextIncudingIntro);
}

// ----------------------------------------------------------------------------

void AudioFeedback::ReportMp3FileProblem(PlaybackInfo info)
{
   m_pPriv->ReportMp3FileProblem(info);
}

// ----------------------------------------------------------------------------

void AudioFeedback::ReportNoMp3Files()
{
   m_pPriv->ReportNoMp3Files();
}

// ----------------------------------------------------------------------------

void AudioFeedback::ReportShutdown() // countdown 9, 8, 7, ... 0
{
   m_pPriv->ReportShutdown();
}

// ----------------------------------------------------------------------------

void AudioFeedback::CancelShutdown()
{
   m_pPriv->CancelShutdown();
}

// ----------------------------------------------------------------------------

bool AudioFeedback::IsReportingShutdown()
{
   return m_pPriv->IsReportingShutdown();
}

// ----------------------------------------------------------------------------
bool AudioFeedback::IsShutdownReported()
{
   return m_pPriv->IsShutdownReported();
}

// ----------------------------------------------------------------------------

void AudioFeedback::ReportServiceMode()
{
   m_pPriv->ReportServiceMode();
}

// ----------------------------------------------------------------------------

bool AudioFeedback::IsServiceModeReported()
{
   return m_pPriv->IsServiceModeReported();
}
