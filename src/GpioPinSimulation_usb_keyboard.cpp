// ----------------------------------------------------------------------------
//  SleepyPiPlayer:  RaspberryGpioPin   raspberry-variant
//    easier development and testing on raspberry on USB-keyboard instead of GPIO
//    detection uses /dev/input/eventXXX
//    requirement either root or group=input
// license: free software   (sleepypiplayer(at)saftfresse.de)  [A.D.2025]
// - Do whatever you want with the software.
// - Do not claim my work as your own.
// - THIS SOFTWARE COMES WITH NO WARRANTIES, USE AT YOUR OWN RISK
// ----------------------------------------------------------------------------

// /usr/include/linux/input.h
// /usr/include/linux/input-event-codes.h

#include "GpioPinRaspberry.h"

#include <stdio.h>
#include <cstdio>
#include <string>
#include <list>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <linux/input.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

// ============================================================================
// decode content of e.g. /sys/class/input/input6/capabilities/key
class SysInputCapabilityInfo
{
public:
   // path  e.g.  /sys/class/input/input6/capabilities/ev
   SysInputCapabilityInfo(std::string txtFilePath, int nDeviceIdx);

   // ID  e.g. EV_KEY
   bool IsAvailable(int ID) const;

   int GetNofAvailable() const;
   int GetDeviceIndex()  const { return m_nDeviceIdx; }

private:
   void Process(char data);
   std::string m_txtFilePath;
   int         m_NofData = 0;
   bool        m_arrAvail[512];
   int         m_nDeviceIdx = -1;
};

// ----------------------------------------------------------------------------

SysInputCapabilityInfo::SysInputCapabilityInfo(std::string txtFilePath, int nDeviceIdx)
{
   m_nDeviceIdx = nDeviceIdx;
   for (int i = 0; i < 512; i++)   m_arrAvail[i]=false;
   FILE* pFile = fopen(txtFilePath.c_str(), "rb");
   if (pFile)
   {
      char buffer[1024] = {};
      int64_t nNofRead = fread(buffer, 1, 512, pFile);
      if (nNofRead > 0 && nNofRead < 1024)
      {
         for (int i = nNofRead -1; i >= 0; i--)
         {
            Process(buffer[i]);
         }
      }
      fclose(pFile);
   }
}

// ----------------------------------------------------------------------------

void SysInputCapabilityInfo::Process(char data)
{
   if (m_NofData < (512-4))
   {
      int nBits = -1;
      if (data >= 'a' && data <= 'f')
      {
         nBits = 10 + data - 'a';
      }
      else if (data >= 'A' && data <= 'F')
      {
         nBits = 10 + data - 'A';
      }
      else if (data >= '0' && data <= '9')
      {
         nBits = data - '0';
      }
      if (nBits >= 0)
      {
         m_arrAvail[m_NofData + 0] = ((nBits & 0x01) != 0);
         m_arrAvail[m_NofData + 1] = ((nBits & 0x02) != 0);
         m_arrAvail[m_NofData + 2] = ((nBits & 0x04) != 0);
         m_arrAvail[m_NofData + 3] = ((nBits & 0x08) != 0);
         m_NofData += 4;
      }
   }
}

// ----------------------------------------------------------------------------
// ID  e.g. EV_KEY
bool SysInputCapabilityInfo::IsAvailable(int ID) const
{
   bool bAvail = false;
   if (ID >= 0 && ID < m_NofData)
   {
      bAvail = m_arrAvail[ID];
   }
   return bAvail;
}

// ----------------------------------------------------------------------------
// e.g. number of available keys of a keyboard
// if many keyboards are detected: use that with less keys
int SysInputCapabilityInfo::GetNofAvailable() const
{
   int nNofAvailable = 0;
   for (int i = 0; i < m_NofData; i++)
   {
      nNofAvailable = (m_arrAvail[i])? nNofAvailable+1 : nNofAvailable;
   }
   return nNofAvailable;
}


// ============================================================================
class GpioPinRaspberry::PrivateData
{
public:
   PrivateData()
   {
      m_stateKeyA = 0;
      m_stateKeyS = 0;
      m_stateKeyF = 0;
      m_stateKeyI = 0;
      m_stateKeyQ = 0;
      m_stateKeyW = 0;
      m_stateKeyK = 0;
      m_stateKeyX = 0;

      int nInputNumber = FindInputNumberOfKeyboard();
      if (nInputNumber >= 0)
      {
         std::string txtEventFile("/dev/input/event");
         txtEventFile.append(std::to_string(nInputNumber));
         m_nFileDescriptor = open(txtEventFile.c_str(), O_RDONLY | O_NONBLOCK);
         if (m_nFileDescriptor != -1)
         {
            std::unique_lock<std::mutex> guard(m_Mutex);
            m_Thread = std::thread(ThreadFunction, this);
            m_ConditionStarted.wait(guard); // wait for detection of m_bServiceKeyIsPressed
         }
      }
   }

   virtual ~PrivateData()
   {
      if (m_nFileDescriptor != 1)
      {
         m_bStopThread = true;
         m_Thread.join();

         close(m_nFileDescriptor);
         m_nFileDescriptor = -1;
      }
   }

   bool IsKeyPressed(int nKeyNumber);
   int  m_nFileDescriptor = -1;

protected:
   int FindInputNumberOfKeyboard();
   static void ThreadFunction(GpioPinRaspberry::PrivateData* pThis);

   volatile bool             m_bStopThread = false;
   std::thread               m_Thread;
   std::mutex                m_Mutex;
   std::condition_variable   m_ConditionStarted;

   std::atomic<int>          m_stateKeyA;  // back
   std::atomic<int>          m_stateKeyS;  // forw
   std::atomic<int>          m_stateKeyF;  // service
   std::atomic<int>          m_stateKeyI;  // info
   std::atomic<int>          m_stateKeyQ;  // volume-
   std::atomic<int>          m_stateKeyW;  // volume+
   std::atomic<int>          m_stateKeyX;  // OFF
   std::atomic<int>          m_stateKeyK;  // simulate auto-shutdown
};

// ----------------------------------------------------------------------------

bool  GpioPinRaspberry::PrivateData::IsKeyPressed(int nKeyNumber)
{
   bool bIsPressed = false;
   switch (nKeyNumber)
   {
      case KEY_A: bIsPressed = (m_stateKeyA !=0); break;
      case KEY_S: bIsPressed = (m_stateKeyS !=0); break;
      case KEY_F: bIsPressed = (m_stateKeyF !=0); break;
      case KEY_I: bIsPressed = (m_stateKeyI !=0); break;
      case KEY_Q: bIsPressed = (m_stateKeyQ !=0); break;
      case KEY_W: bIsPressed = (m_stateKeyW !=0); break;
      case KEY_X: bIsPressed = (m_stateKeyX !=0); break;
      case KEY_K: bIsPressed = (m_stateKeyK !=0); break;
   }
   return bIsPressed;
}

// ----------------------------------------------------------------------------

void GpioPinRaspberry::PrivateData::ThreadFunction(GpioPinRaspberry::PrivateData* pThis)
{
   pThis->m_ConditionStarted.notify_all();
   int nForceSleep = 1;
   while (!pThis->m_bStopThread)
   {
      if (pThis->m_nFileDescriptor == -1)
      {
         std::chrono::milliseconds durationSleep{20};
         std::this_thread::sleep_for(durationSleep);
      }
      else
      {
         input_event event_struct;
         int nNofRead = read(pThis->m_nFileDescriptor, &event_struct, sizeof(event_struct));
         if (nNofRead <= 0)
         {
            std::chrono::milliseconds durationSleep{20};
            std::this_thread::sleep_for(durationSleep);
            nForceSleep = 20;
         }
         else
         {
            if (event_struct.type == EV_KEY)
            {
               switch(event_struct.code)
               {
               case KEY_A: pThis->m_stateKeyA = event_struct.value; break;
               case KEY_S: pThis->m_stateKeyS = event_struct.value; break;
               case KEY_F: pThis->m_stateKeyF = event_struct.value; break;
               case KEY_I: pThis->m_stateKeyI = event_struct.value; break;
               case KEY_Q: pThis->m_stateKeyQ = event_struct.value; break;
               case KEY_W: pThis->m_stateKeyW = event_struct.value; break;
               case KEY_X: pThis->m_stateKeyX = event_struct.value; break;
               case KEY_K: pThis->m_stateKeyK = event_struct.value; break;
               default: break;
               }
            }
            --nForceSleep;
            if (nForceSleep <= 0)
            {
               std::chrono::milliseconds durationSleep{5};
               std::this_thread::sleep_for(durationSleep);
               nForceSleep = 10;
            }
         }
      }
   }
}

// ----------------------------------------------------------------------------
// valid return values: 0 .. 40   / -1: no keyboard found
int GpioPinRaspberry::PrivateData::FindInputNumberOfKeyboard()
{
   int nFoundIdx = -1;
   std::list<SysInputCapabilityInfo>  listKeyboards;

   for (int nTryInputIdx = 0; nTryInputIdx < 40; nTryInputIdx++)
   {
      std::string  txtCapabilityPath("/sys/class/input/input");
      txtCapabilityPath.append(std::to_string(nTryInputIdx));
      txtCapabilityPath.append("/capabilities");
      SysInputCapabilityInfo  capEvent(txtCapabilityPath+"/ev", nTryInputIdx);
      if (capEvent.IsAvailable(EV_KEY))
      {
         SysInputCapabilityInfo  capKey(txtCapabilityPath+"/key", nTryInputIdx);
         if (capKey.IsAvailable(KEY_S) && capKey.IsAvailable(KEY_A))
         {
            listKeyboards.push_back(capKey);
         }
      }
   }
   if (listKeyboards.size() == 1)
   {
      nFoundIdx = listKeyboards.front().GetDeviceIndex();
   }
   else if (listKeyboards.size() > 1)
   {
      // in my case the keyboard with less keys is the right one
      int nMinNofAvail = listKeyboards.front().GetNofAvailable();
      nFoundIdx        = listKeyboards.front().GetDeviceIndex();
      listKeyboards.pop_front();
      for (const SysInputCapabilityInfo& cap : listKeyboards)
      {
         int nNofAvail = cap.GetNofAvailable();
         if (nNofAvail < nMinNofAvail)
         {
            nMinNofAvail = nNofAvail;
            nFoundIdx = cap.GetDeviceIndex();
         }
      }
   }

   return nFoundIdx;
}


// ============================================================================

GpioPinRaspberry::GpioPinRaspberry()
{
   m_pPriv = std::make_unique<GpioPinRaspberry::PrivateData>();
   printf("PC-Keys:\n");
   printf("'a'    : previous file       'a'  -long: playback-fast-backward\n");
   printf("'s'    : next     file       's'  -long: playback-fast-forward\n");
   printf("'i+a'  : previous directory  'i+a'-long: fast change files\n");
   printf("'i+s'  : next     directory  'i+s'-long: fast change files\n");
   printf("'q'    : decrease volume     'w'       : increase volume\n");
   printf("'i'    : request playback informations\n");
   printf("'a+s+f': Service\n");
   printf("'x'    : quit    'k': simulate automatic shutdown\n\n");
}

// ----------------------------------------------------------------------------

GpioPinRaspberry::~GpioPinRaspberry()
{
}

// ----------------------------------------------------------------------------

bool GpioPinRaspberry::GpioProblemDetected()
{
   return m_pPriv->m_nFileDescriptor == -1;
}

// ----------------------------------------------------------------------------

void GpioPinRaspberry::ConfigureInput(int /*nGpioNumber*/, bool /*bInitPullUp*/)
{
}

// ----------------------------------------------------------------------------

bool GpioPinRaspberry::IsKeyPressed(int nGpioNumber)
{
   bool bPressed = false;

   switch(nGpioNumber)
   {
      case 17: bPressed = m_pPriv->IsKeyPressed(KEY_X);     break; // on off
      case 21: bPressed = m_pPriv->IsKeyPressed(KEY_A);     break; // back
      case 13: bPressed = m_pPriv->IsKeyPressed(KEY_S);     break; // forw
      case 16: bPressed = m_pPriv->IsKeyPressed(KEY_I);     break; // info
      case 20: bPressed = m_pPriv->IsKeyPressed(KEY_Q);     break; // quiet
      case 19: bPressed = m_pPriv->IsKeyPressed(KEY_W);     break; // loud
      case 26: bPressed = m_pPriv->IsKeyPressed(KEY_F);     break; // service
      case 99: bPressed = m_pPriv->IsKeyPressed(KEY_K);     break; // simulate auto-shutdown
      default: bPressed = false;
   }

   return bPressed;
}
