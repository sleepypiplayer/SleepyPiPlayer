// ----------------------------------------------------------------------------
// SleepyPiPlayer:  GpioPinRaspberry
//     get state of GPIO-pins
//   possible implementations:
//   - /sys/class/gpio  -> deprecated
//   - wiringpi         -> paket-manager does not provide wiringpi
//   - libpigpio        -> does only work with root-privileges
//   - libgpiod         -> complicated API / v1.6 incompatible with v2.1
//  >>>> extract hardware-access from KeyInput
//  >>>> intention: easily replacing hardware-access method
// ----------------------------------------------------------------------------
// after apt-get upgrade the /sys/class/gpio did not work anymore
// https://github.com/raspberrypi/linux/issues/6037
// 2024-08-04: gpio-numbers += 512   for /sys/class/gpio
#pragma once

#include <memory>

class GpioPinRaspberry
{
public:
   GpioPinRaspberry();
   virtual ~GpioPinRaspberry();

   void ConfigureInput(int nGpioNumber, bool bInitPullUp);
   bool IsKeyPressed(int nGpioNumber);

   bool GpioProblemDetected();  // do not turn off wifi if the buttons do not work

private:
   class PrivateData;
   std::unique_ptr<PrivateData> m_pPriv;
};
