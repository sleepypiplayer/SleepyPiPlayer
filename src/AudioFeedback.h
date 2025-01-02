// ----------------------------------------------------------------------------
// SleepyPiPlayer:  AudioFeedback
//     play audio-feedback-messages (probably as feedback to pressed keys)
// ----------------------------------------------------------------------------
#pragma once

#include "PlaybackInfo.h"

/** store message-requests and provide audio-samples for playback */
class AudioFeedback
{
public:
   // probably:  SampleRate=32000 Channels=MPG123_MONO Encoding=MPG123_ENC_SIGNED_16
   AudioFeedback(long nSampleRate, int nChannels, int nEncoding);
   virtual ~AudioFeedback();
   void SetVolume(int nVolumePct); // just set volume without audio-feedback

   int GetNofMessagesInQueue(); // used by FileFastForward to sync with messages

   // request some feedback because of events:
   void ReportChecksumProblem();                    // "Attention checksum error in saved-position"
   void ReportStartupPosition(PlaybackInfo info);   // "file 13 of 55 minute 2"
   void ReportMp3FilePosition(int nMinutesInFile, bool bTextIncudingIntro);  // ("minute") + "13"
   void ReportMp3FileProblem(PlaybackInfo info);    // "problem with mp3-file"
   void ReportNoMp3Files();                         // "no mp3-file found"
   void VolumeChanged(int nVolumePct);              // "volume 55"
   void FileChanged(PlaybackInfo info);             // "file 13 of 55"
   void FastPlaybackFileChanged(PlaybackInfo info); // "file 13 minute 4"
   void DirChanged(PlaybackInfo info);              // "directory 3 of 7  file 1 of 12"
   void InfoRequest(PlaybackInfo info);             //  direcotry ... file ... minute ...  spell-file-name

   void ReportShutdown();      // start countdown 9, 8, 7, ... 0
   void CancelShutdown();      // stop  countdown
   bool IsReportingShutdown(); // still output .. 4, 3, 2 ...
   bool IsShutdownReported();  // countdown has finished without being canceled

   void ReportServiceMode();      // "service  wifi active"
   bool IsServiceModeReported();  // finished output of "service"

   // used by AudioPlayer: play an audio-message instead of user-mp3-files
   // false: no audio-feedback available = continue playing user-mp3-files
   bool GetAudioSamples(unsigned char* pSampleBuffer, size_t SAMPLE_BUFFER_SIZE, size_t* /*OUT:*/pSampleBufferBytes);

private:
   class PrivateData;
   std::unique_ptr<PrivateData> m_pPriv;
};
