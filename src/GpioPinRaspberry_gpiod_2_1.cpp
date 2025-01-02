// ----------------------------------------------------------------------------
//  SleepyPiPlayer:  RaspberryGpioPin   libgpiod  v2.1
//    detect pin-state by libgpiod  v2.1  (never tested)
//   g++ -I ../../libgpiod/libgpiod-master/include/  GpioPinRaspberry_gpiod_2_1.cpp
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

   bool                       m_bGpioProblem = false;
   struct gpiod_chip*         m_pGpioChip    = nullptr;
   std::map<int,struct gpiod_line_request*>  m_mapGpioLine;
};

// ----------------------------------------------------------------------------

GpioPinRaspberry::PrivateData::PrivateData()
{
   m_pGpioChip = gpiod_chip_open("gpiochip0");
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
         gpiod_line_request_release(it->second);
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
      // avoid problems with /sys/class/gpio ownership
      ReleaseSysClassGpio(nGpioNumber);
      ReleaseSysClassGpio(nGpioNumber+512);  // https://github.com/raspberrypi/linux/issues/6037

      struct gpiod_line_settings*  pGpioSettingDirection =  gpiod_line_settings_new();
      struct gpiod_line_settings*  pGpioSettingPullUp    =  gpiod_line_settings_new();
      struct gpiod_line_config *   pGpioLineCfg          =  gpiod_line_config_new();
      struct gpiod_request_config* pReqConfig            =  gpiod_request_config_new();

      if (pGpioSettingDirection == nullptr || pGpioSettingPullUp == nullptr ||
          pGpioLineCfg == nullptr          || pReqConfig         == nullptr  )
      {
         m_bGpioProblem = true;
      }
      else
      {
         gpiod_line_settings_set_direction(pGpioSettingDirection, GPIOD_LINE_DIRECTION_INPUT);
         int ret = gpiod_line_config_add_line_settings(pGpioLineCfg, (uint32_t*)(&nGpioNumber), 1, pGpioSettingDirection);
         if (ret != 0)
         {
            m_bGpioProblem = true;
         }

         gpiod_request_config_set_consumer(pReqConfig, "sleepy");

         if (bInitPullUp)
         {
            ret = gpiod_line_settings_set_bias(pGpioSettingPullUp, GPIOD_LINE_BIAS_PULL_UP);
            if (ret != 0)
            {
                m_bGpioProblem = true;
            }
            else
            {
               ret = gpiod_line_config_add_line_settings(pGpioLineCfg, (uint32_t*)(&nGpioNumber), 1, pGpioSettingPullUp);
            }
            if (ret != 0)
            {
               m_bGpioProblem = true;
            }
         }
         struct gpiod_line_request* pGpioRequest = nullptr;
         pGpioRequest = gpiod_chip_request_lines(m_pGpioChip, pReqConfig, pGpioLineCfg);
         if (pGpioRequest == nullptr)
         {
            m_bGpioProblem = true;
         }
         else
         {
            m_mapGpioLine[nGpioNumber] = pGpioRequest;
         }
      }

      // clean-up
      if (pGpioSettingDirection)
      {
         gpiod_line_settings_free(pGpioSettingDirection);
         pGpioSettingDirection = nullptr;
      }
      if (pGpioSettingDirection)
      {
         gpiod_line_settings_free(pGpioSettingPullUp);
         pGpioSettingPullUp = nullptr;
      }
      if (pGpioLineCfg)
      {
         gpiod_line_config_free(pGpioLineCfg);
         pGpioLineCfg = nullptr;
      }
      if (pReqConfig)
      {
         gpiod_request_config_free(pReqConfig);
         pReqConfig = nullptr;
      }
   }
}

// ----------------------------------------------------------------------------

bool GpioPinRaspberry::PrivateData::IsKeyPressed(int nGpioNumber)
{
   bool bPressed = false;
   if (m_mapGpioLine.find(nGpioNumber) != m_mapGpioLine.end())
   {
      enum gpiod_line_value eValue = gpiod_line_request_get_value(m_mapGpioLine[nGpioNumber], nGpioNumber);
      bPressed = (eValue == GPIOD_LINE_VALUE_INACTIVE);
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
