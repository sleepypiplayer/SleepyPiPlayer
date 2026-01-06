// ----------------------------------------------------------------------------
//  SleepyPiPlayer:  KeyInput   detect Key-Press
//            poll GPIO pins
// GPIO # 21 white   file-back
// GPIO # 13 white   file-forw
// GPIO # 19 black   loud
// GPIO # 20 black   quiet
// GPIO # 16 yellow  info
// GPIO # 26 blue    service
// GPIO # 17 red     on off
// long-press file-forw:  PlayFastForward
// long-press file-back:  PlayFastBackward
// combination Directory-Forw: yellow-info + white-forw
// combination Directory-Back: yellow-info + white-back
// combination Service:  blue-service + white-forw + white-back
// license: free software   (sleepypiplayer(at)saftfresse.de)  [A.D.2025]
// - Do whatever you want with the software.
// - Do not claim my work as your own.
// - THIS SOFTWARE COMES WITH NO WARRANTIES, USE AT YOUR OWN RISK
// ----------------------------------------------------------------------------
#include "KeyInput.h"
#include "GpioPinRaspberry.h"
#include <thread>
#include <mutex>
#include <chrono>
#include <list>
#include <cstdio>
#include <condition_variable>
#include <atomic>

// ============================================================================

class  GpioKey
{
public:
   GpioKey(GpioPinRaspberry& GpioPin, int nGpioNumber, bool bInitPullUp)
   : m_GpioPin(GpioPin)
   {
      m_nGpioNumber =  nGpioNumber;
      if (nGpioNumber < 99)  // do not configure simulated-auto-shutdown: invalid raspberry-GPIO
      {
         m_GpioPin.ConfigureInput(nGpioNumber, bInitPullUp);
      }
      m_timeStart = std::chrono::steady_clock::now();
    }

   void CheckState()
   {
      bool bPressed = m_GpioPin.IsKeyPressed(m_nGpioNumber);

      if (m_bButtonBounce)
      {
         std::chrono::milliseconds  durationBounce{50};
         std::chrono::time_point<std::chrono::steady_clock> timeNow = std::chrono::steady_clock::now();
         if ((timeNow - m_timeStart) > durationBounce)
         {
            m_bButtonBounce = false;
         }
         else
         {
            bPressed = m_bPressedLastTime;
         }
      }

      if (m_bPressedLastTime != bPressed)
      {
         m_bButtonBounce = true;
         m_timeStart = std::chrono::steady_clock::now();
      }

      m_bWasPressed = false;
      if (m_bPressedLastTime && !bPressed && !m_bLongPress)
      {
         m_bWasPressed = true;
      }
      else if (bPressed)
      {
         std::chrono::milliseconds  durationLongPress{1800};
         std::chrono::time_point<std::chrono::steady_clock> timeNow = std::chrono::steady_clock::now();
         if ((timeNow - m_timeStart) > durationLongPress)
         {
            m_bLongPress = true;
         }
         else
         {
            m_bLongPress = false;
         }
      }
      else
      {
         m_bLongPress = false;
      }
      m_bPressedLastTime = bPressed;
   }

   bool WasPressed() // pressed and released
   {
      return m_bWasPressed;
   }

   bool IsPressed() // currently pressed or just released
   {
      return m_bPressedLastTime || m_bWasPressed;
   }

   bool LongPress()  // long-press detected
   {
      return m_bLongPress;
   }

private:

   GpioPinRaspberry& m_GpioPin;
   int   m_nGpioNumber      = -1;
   bool  m_bButtonBounce    = false;  // ignore unreliable button-state for some milliseconds
   bool  m_bWasPressed      = false;  // detected  pressed -> released
   bool  m_bLongPress       = false;  // pressed some time without releasing
   bool  m_bPressedLastTime = false;  // state of last check used to detect "WasPressed"
   std::chrono::time_point<std::chrono::steady_clock> m_timeStart;  // last change of state: pressed->released or released->pressed

};

// ============================================================================
class KeyInput::PrivateData
{
public:
   PrivateData(bool bUseServiceKeyAsNextDir)
   {
      m_bUseServiceKeyAsNextDir = bUseServiceKeyAsNextDir;
      std::unique_lock<std::mutex> guard(m_Mutex);
      m_Thread = std::thread(ThreadFunction, this);
      m_ConditionStarted.wait(guard); // wait for detection of m_bServiceKeyIsPressed
   }

   virtual ~PrivateData()
   {
      m_bStopThread = true;
      m_Thread.join();
   }

   volatile bool             m_bStopThread = false;
   volatile bool             m_bGpioProblem = false;
   bool                      m_bUseServiceKeyAsNextDir = false; // SystemConfigFile::AllowNextDirByServiceKey()
   std::list<KeyInput::KEY>  m_listDetectedKeys;
   std::atomic<bool>         m_bServiceKeyIsPressed = false;
   std::thread               m_Thread;
   std::mutex                m_Mutex;
   std::condition_variable   m_ConditionStarted;

   static void ThreadFunction(KeyInput::PrivateData* pThis);
};

// ----------------------------------------------------------------------------

void KeyInput::PrivateData::ThreadFunction(KeyInput::PrivateData* pThis)
{
   GpioPinRaspberry pin_access;

   GpioKey  keyPower( pin_access, 17, false);  // red
   GpioKey  keyInfo(  pin_access, 16, true);   // yellow
   GpioKey  keyForw(  pin_access, 13, true);   // white
   GpioKey  keyBack(  pin_access, 21, true);   // white
   GpioKey  keyLoud(  pin_access, 19, true);   // black
   GpioKey  keyQuiet( pin_access, 20, true);   // black
   GpioKey  keyServ(  pin_access, 26, true);   // blue
   GpioKey  keySimAutoSutdown(pin_access, 99, false);  // simulate auto-shutdown in PC-version

   std::list<GpioKey*> listKeys;
   listKeys.push_back(&keyPower);
   listKeys.push_back(&keyInfo);
   listKeys.push_back(&keyForw);
   listKeys.push_back(&keyBack);
   listKeys.push_back(&keyLoud);
   listKeys.push_back(&keyQuiet);
   listKeys.push_back(&keyServ);
   listKeys.push_back(&keySimAutoSutdown);

   pThis->m_bGpioProblem = pin_access.GpioProblemDetected();

   bool bShutdownAllowed = false;  // Power-Button was released after system-start
   bool bShutdown        = false;  // Power-Button detected: do need to record any further key-press
   bool bService         = false;  // Service-Combination detected: do need to record any further key-press
   bool bLongFfFb        = false;  // long-press Forward/Backward
   bool bInfoDir         = false;  // info + Forw/Back = Dir-Forw/Back
   bool bInvalid         = false;  // nonsense key-combination


   int nNofPressedKeysLastTime = 0;
   std::chrono::milliseconds durationSleep{20};
   while (!pThis->m_bStopThread)
   {
      std::this_thread::sleep_for(durationSleep);
      int nNofPressedKeys = 0;
      for (GpioKey* pKey : listKeys)
      {
         pKey->CheckState();
         if (pKey->IsPressed())
         {
            ++nNofPressedKeys;
         }
      }

      // --- shutdown
      if (!keyPower.IsPressed())
      {
         bShutdownAllowed = true;
      }
      if (bShutdownAllowed && keyPower.IsPressed())
      {
         bShutdown = true;
      }

      if (bInfoDir)
      {
         if (!keyInfo.IsPressed())
         {
            bInfoDir = true;
            bInvalid = true;
         }
      }

      // --- reset invalid
      if (nNofPressedKeys == 0)
      {
         bInvalid = false;
         bInfoDir = false;
      }

      KeyInput::KEY eNewKey = KeyInput::KEY_NONE;
      bool bServiceKeyCurrentlyPressed = keyServ.IsPressed();  // do not disable wifi on startup

      if (!bShutdown && !bInvalid)
      {
         if (nNofPressedKeys == 1  &&  keySimAutoSutdown.WasPressed())
         {
            eNewKey = KeyInput::KEY_SimulatedAutoShutdown;
         }
         if (nNofPressedKeys == 1  &&  keyLoud.WasPressed())
         {
            eNewKey = KeyInput::KEY_VolumeInc;
         }
         if (nNofPressedKeys == 1  &&  keyQuiet.WasPressed())
         {
            eNewKey = KeyInput::KEY_VolumeDec;
         }
         if (nNofPressedKeys == 1  &&  keyBack.WasPressed())
         {
            eNewKey = KeyInput::KEY_FilePrev;
         }
         else if (nNofPressedKeys == 1  && keyBack.LongPress())
         {
            eNewKey = KeyInput::KEY_PlayFastBack;
            bLongFfFb = true;
         }
         if (nNofPressedKeys == 1  && keyForw.WasPressed())
         {
            eNewKey = KeyInput::KEY_FileNext;
         }
         else if (nNofPressedKeys == 1  && keyForw.LongPress())
         {
            eNewKey = KeyInput::KEY_PlayFastForw;
            bLongFfFb = true;
         }
         if (nNofPressedKeys == 1 && keyInfo.WasPressed() && !bInfoDir)
         {
             eNewKey = KeyInput::KEY_Info;
         }
         if (pThis->m_bUseServiceKeyAsNextDir)
         {
            if (nNofPressedKeys == 1 && keyServ.WasPressed())
            {
               eNewKey = KeyInput::KEY_DirNext;
            }
         }

         // -- Key-Combinations
         if (nNofPressedKeys > 1)
         {
            if (keyInfo.IsPressed())
            {
               if (keyForw.WasPressed() && nNofPressedKeys == 2)
               {
                  eNewKey = KeyInput::KEY_DirNext;
                  bInfoDir = true;
               }
               else if (keyBack.WasPressed() && nNofPressedKeys == 2)
               {
                  eNewKey = KeyInput::KEY_DirPrev;
                  bInfoDir = true;
               }
               else if (keyForw.LongPress() && nNofPressedKeys == 2)
               {
                  eNewKey = KeyInput::KEY_FileFastForw;
                  bLongFfFb = true;
                  bInfoDir  = true;
               }
               else if (keyBack.LongPress() && nNofPressedKeys == 2)
               {
                  eNewKey = KeyInput::KEY_FileFastBack;
                  bLongFfFb = true;
                  bInfoDir  = true;
               }
               else if (nNofPressedKeys > 2)
               {
                  bInvalid = true;
               }
               else if (!keyForw.IsPressed() && !keyBack.IsPressed())
               {
                  bInvalid = true;
               }
            }
            else if (keyServ.IsPressed())
            {
                if (keyForw.IsPressed())
                {
                   if (keyBack.IsPressed())
                   {
                      if (nNofPressedKeys > 3)
                      {
                         bInvalid = true;
                      }
                      else if (keyServ.LongPress() && keyBack.LongPress() && keyForw.LongPress())
                      {
                         bService = true;
                      }
                   }
                   else if (nNofPressedKeys > 2)
                   {
                      bInvalid = true;
                   }
                }
                else if (keyBack.IsPressed())
                {
                   if (nNofPressedKeys > 2)
                   {
                      bInvalid = true;
                   }
                }
                else
                {
                   bInvalid = true;
                }
            }
            else
            {
               bInvalid = true;
            }
         }
      }

      {
         std::lock_guard<std::mutex> guard(pThis->m_Mutex);

         pThis->m_bServiceKeyIsPressed = bServiceKeyCurrentlyPressed;

         if (bShutdown)
         {
            bShutdown = false;
            pThis->m_listDetectedKeys.clear();
            pThis->m_listDetectedKeys.push_back(KeyInput::KEY_Shutdown);
         }
         else if (bService)
         {
            pThis->m_listDetectedKeys.push_back(KeyInput::KEY_Service);
         }
         else if ( bLongFfFb &&
                  eNewKey != KeyInput::KEY_PlayFastForw &&
                  eNewKey != KeyInput::KEY_PlayFastBack &&
                  eNewKey != KeyInput::KEY_FileFastForw &&
                  eNewKey != KeyInput::KEY_FileFastBack  )
         {
            pThis->m_listDetectedKeys.push_back(KeyInput::KEY_NONE);
            bLongFfFb = false;
         }
         else if (bLongFfFb && bInvalid)
         {
            pThis->m_listDetectedKeys.push_back(KeyInput::KEY_NONE);
            bLongFfFb = false;
         }
         else if (eNewKey != KeyInput::KEY_NONE && !bInvalid)
         {
            pThis->m_listDetectedKeys.push_back(eNewKey);
         }

         if (pThis->m_listDetectedKeys.size() == 0)
         {
            if ( nNofPressedKeys == 0 && nNofPressedKeysLastTime > 0)
            {
               pThis->m_listDetectedKeys.push_back(KEY_ANY);  // stop auto-shutdown with invalid key-combination
            }
            nNofPressedKeysLastTime = nNofPressedKeys;
         }
         else
         {
            nNofPressedKeysLastTime = 0;  // some key-press was already detected: no need to stop auto-shutdown by KEY_ANY
         }
      }
      pThis->m_ConditionStarted.notify_all();
   }  // while !StopThread

}

// ============================================================================

KeyInput::KeyInput(bool bUseServiceKeyAsNextDir)
{
   m_pPriv = std::make_unique<KeyInput::PrivateData>(bUseServiceKeyAsNextDir);
}

// ----------------------------------------------------------------------------

KeyInput::~KeyInput()
{
}

// ----------------------------------------------------------------------------

std::list<KeyInput::KEY> KeyInput::GetInputKeys()
{
   std::list<KeyInput::KEY> listKeys;
   std::lock_guard<std::mutex> guard(m_pPriv->m_Mutex);
   listKeys.swap(m_pPriv->m_listDetectedKeys);
   if (m_pPriv->m_bGpioProblem)
   {
      listKeys.push_back(KeyInput::KEY_Service);
   }
   return listKeys;
}

// ----------------------------------------------------------------------------

bool KeyInput::IsServiceKeyPressed()  // pressed on startup: do not disable wifi
{
   std::lock_guard<std::mutex> guard(m_pPriv->m_Mutex);
   return m_pPriv->m_bServiceKeyIsPressed;
}
