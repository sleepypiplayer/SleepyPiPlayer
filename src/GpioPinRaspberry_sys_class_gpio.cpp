// ----------------------------------------------------------------------------
//  SleepyPiPlayer:  RaspberryGpioPin   /sys/class/gpio
//    detect pin-state by /sys/class/gpio
// https://github.com/raspberrypi/linux/issues/6037
// 2024-08-04: gpio-numbers += 512   for /sys/class/gpio
// license: free software   (sleepypiplayer(at)saftfresse.de)  [A.D.2025]
// - Do whatever you want with the software.
// - Do not claim my work as your own.
// - THIS SOFTWARE COMES WITH NO WARRANTIES, USE AT YOUR OWN RISK
// ----------------------------------------------------------------------------
#include "GpioPinRaspberry.h"
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <cstring>


// ============================================================================
class GpioPinRaspberry::PrivateData
{
public:
   PrivateData()
   {}

   virtual ~PrivateData()
   {
      for (auto it = m_mapGpioValueFile.begin(); it != m_mapGpioValueFile.end(); ++it)
      {
         ReleaseSysClassGpio(it->first);
         ReleaseSysClassGpio(it->first+512);
      }
   }

   void ConfigureInput(int nGpioNumber, bool bInitPullUp);
   bool IsKeyPressed(int nGpioNumber);


   void ReleaseSysClassGpio(int nGpioNumber);

   // https://github.com/raspberrypi/linux/issues/6037
   int SYS_CLASS_GPIO_OFFSET = 0;   // 0 or 512

   bool m_bGpioProblem = false;
   std::map<int, std::string>   m_mapGpioValueFile;  // e.g. "/sys/class/gpio/gpio17/value"
};

// ----------------------------------------------------------------------------

// avoid problems with /sys/class/gpio ownership
// unexport e.g. /sys/class/gpio/gpio20/value
void GpioPinRaspberry::PrivateData::ReleaseSysClassGpio(int nGpioNumber)
{
   std::FILE* pFile = std::fopen("/sys/class/gpio/unexport", "w");
   if (pFile)
   {
      char txtBufferExportNumber[30] = {};
      sprintf(txtBufferExportNumber, "%i", nGpioNumber);
      fwrite(txtBufferExportNumber, 1, strlen(txtBufferExportNumber), pFile);
      std::fclose(pFile);
   }
}

// ----------------------------------------------------------------------------

void GpioPinRaspberry::PrivateData::ConfigureInput(int nGpioNumber, bool bInitPullUp)
{
   std::FILE* pFile = fopen("/sys/class/gpio/export", "w");
   if (pFile == nullptr)
   {
      m_bGpioProblem = true;
   }
   else
   {
      char txtBufferExportNumber[30] = {};
      sprintf(txtBufferExportNumber, "%i", nGpioNumber+SYS_CLASS_GPIO_OFFSET);
      fwrite(txtBufferExportNumber, 1, strlen(txtBufferExportNumber), pFile);
      fclose(pFile);

      char txtBufferDirectionFile[200] = {};
      sprintf(txtBufferDirectionFile, "/sys/class/gpio/gpio%i/direction", nGpioNumber+SYS_CLASS_GPIO_OFFSET);
      pFile = fopen(txtBufferDirectionFile, "w");
      if (pFile == nullptr)
      {
         m_bGpioProblem = true;
      }
      else
      {
         fwrite("in", 1, 2, pFile);
         fclose(pFile);
         if (bInitPullUp)
         {
            char txtBufferPullUp[200] = {};
            sprintf(txtBufferPullUp, "pinctrl set %i ip pu\n", nGpioNumber);
            std::system(txtBufferPullUp);
         }

         char txtBufferValueFile[200] = {};
         sprintf(txtBufferValueFile,"/sys/class/gpio/gpio%i/value", nGpioNumber+SYS_CLASS_GPIO_OFFSET);
         pFile = fopen(txtBufferValueFile, "r");
         if (pFile == nullptr)
         {
            m_bGpioProblem = true;
         }
         else
         {
            fclose(pFile);
            m_mapGpioValueFile[nGpioNumber] = std::string(txtBufferValueFile);
         }
      }
   }
   if (m_bGpioProblem && SYS_CLASS_GPIO_OFFSET == 0)
   {
      // https://github.com/raspberrypi/linux/issues/6037
      // try it again with offset of 512
      SYS_CLASS_GPIO_OFFSET = 512;
      m_bGpioProblem = false;
      ConfigureInput(nGpioNumber, bInitPullUp);
   }
}

// ----------------------------------------------------------------------------

bool GpioPinRaspberry::PrivateData::IsKeyPressed(int nGpioNumber)
{
   bool bPressed = false;

   if (m_mapGpioValueFile.find(nGpioNumber) != m_mapGpioValueFile.end())
   {
      std::FILE* pFile = fopen(m_mapGpioValueFile[nGpioNumber].c_str(), "r");
      if (pFile)
      {
         char txtBuffer[10] = {};
         fread(txtBuffer, 1, 1, pFile);
         fclose(pFile);
         bPressed = txtBuffer[0] == '0';
      }
   }
   return bPressed;
}


// ============================================================================

GpioPinRaspberry::GpioPinRaspberry()
{
   m_pPriv = std::make_unique<GpioPinRaspberry::PrivateData>();

}

// ----------------------------------------------------------------------------

GpioPinRaspberry::~GpioPinRaspberry()
{
}

// ----------------------------------------------------------------------------

bool GpioPinRaspberry::GpioProblemDetected()
{
   return m_pPriv->m_bGpioProblem;
}

// ----------------------------------------------------------------------------

void GpioPinRaspberry::ConfigureInput(int nGpioNumber, bool bInitPullUp)
{
   m_pPriv->ConfigureInput(nGpioNumber, bInitPullUp);
}

// ----------------------------------------------------------------------------

bool GpioPinRaspberry::IsKeyPressed(int nGpioNumber)
{
   return m_pPriv->IsKeyPressed(nGpioNumber);
}
