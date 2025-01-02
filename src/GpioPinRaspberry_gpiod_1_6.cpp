// ----------------------------------------------------------------------------
//  SleepyPiPlayer:  RaspberryGpioPin   libgpiod  v1.6
//    detect pin-state by libgpiod  v1.6
// license: free software   (sleepypiplayer(at)saftfresse.de)  [A.D.2025]
// - Do whatever you want with the software.
// - Do not claim my work as your own.
// - THIS SOFTWARE COMES WITH NO WARRANTIES, USE AT YOUR OWN RISK
// ----------------------------------------------------------------------------
#include "GpioPinRaspberry.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <gpiod.h>

// ============================================================================
class GpioPinRaspberry::PrivateData
{
public:
   PrivateData();
   virtual ~PrivateData();

   void ConfigureInput(int nGpioNumber, bool bInitPullUp);
   bool IsKeyPressed(int nGpioNumber);

   void ReleaseSysClassGpio(int nGpioNumber);

   bool                              m_bGpioProblem = false;
   struct gpiod_chip*                m_pGpioChip    = nullptr;
   std::map<int,struct gpiod_line*>  m_mapGpioLine;
};

// ----------------------------------------------------------------------------

GpioPinRaspberry::PrivateData::PrivateData()
{
   m_pGpioChip = gpiod_chip_open_by_number(0);
   if (m_pGpioChip == nullptr)
   {
      m_bGpioProblem = true;
      printf("#= GPIO-Chip = nullptr\n");
   }
}

// ----------------------------------------------------------------------------

GpioPinRaspberry::PrivateData::~PrivateData()
{
   if (m_pGpioChip)
   {
      for (auto it = m_mapGpioLine.begin(); it != m_mapGpioLine.end(); ++it)
      {
         gpiod_line_release(it->second);
      }
      gpiod_chip_close(m_pGpioChip);
      m_pGpioChip = nullptr;
   }
}


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
   if (m_pGpioChip)
   {
      struct gpiod_line* pLine = gpiod_chip_get_line(m_pGpioChip, nGpioNumber);
      if (pLine == nullptr)
      {
         m_bGpioProblem = true;
         printf("#= GPIO Line %i = nullptr\n", nGpioNumber);
      }
      else
      {
         // avoid problems with /sys/class/gpio ownership
         ReleaseSysClassGpio(nGpioNumber);
         ReleaseSysClassGpio(nGpioNumber+512);  // https://github.com/raspberrypi/linux/issues/6037

         const char* txtConsumer = "sleepy";
         int retval = 0;
         if (bInitPullUp)
         {
            retval = gpiod_line_request_input_flags(pLine,
               txtConsumer, GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP);
         }
         else
         {
            retval = gpiod_line_request_input(pLine,txtConsumer);
         }
         if (retval != 0)
         {
            m_bGpioProblem = true;
            printf("#= GPIO Line %i config failed\n", nGpioNumber);
         }
         else
         {
            m_mapGpioLine[nGpioNumber] = pLine;
         }
      }
   }
}

// ----------------------------------------------------------------------------

bool GpioPinRaspberry::PrivateData::IsKeyPressed(int nGpioNumber)
{
   bool bPressed = false;
   if (m_mapGpioLine.find(nGpioNumber) != m_mapGpioLine.end())
   {
      bPressed = (gpiod_line_get_value(m_mapGpioLine[nGpioNumber]) == 0);
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
