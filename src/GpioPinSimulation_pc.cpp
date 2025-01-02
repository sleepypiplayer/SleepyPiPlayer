// ----------------------------------------------------------------------------
//  SleepyPiPlayer:  RaspberryGpioPin   PC-variant
//    easier development and testing on PC instead of Raspberry
//    Service will not be detected by PC-version
// license: free software   (sleepypiplayer(at)saftfresse.de)  [A.D.2025]
// - Do whatever you want with the software.
// - Do not claim my work as your own.
// - THIS SOFTWARE COMES WITH NO WARRANTIES, USE AT YOUR OWN RISK
// ----------------------------------------------------------------------------
#include "GpioPinRaspberry.h"

#include <termios.h>
#include <X11/Xlib.h>
#include <iostream>
#include "X11/keysym.h"


// ============================================================================
class GpioPinRaspberry::PrivateData
{
public:
   PrivateData()
   {
      struct termios TermSettings;
      tcgetattr( fileno( stdin ), &TermSettings );
      TermSettings.c_lflag &= (~ECHO);
      tcsetattr( fileno( stdin ), TCSANOW, &TermSettings );

      m_pDisplay = XOpenDisplay(":0");
   }

   virtual ~PrivateData()
   {
      XCloseDisplay(m_pDisplay);
      m_pDisplay = nullptr;

      struct termios TermSettings;
      tcgetattr( fileno( stdin ), &TermSettings );
      TermSettings.c_lflag |= (ECHO);
      tcsetattr( fileno( stdin ), TCSANOW, &TermSettings );

      tcflush( fileno( stdin  ), TCIOFLUSH);
      tcflush( fileno( stdout ), TCIOFLUSH);
   }

   Display* m_pDisplay = nullptr;
};

// ============================================================================

GpioPinRaspberry::GpioPinRaspberry()
{
   m_pPriv = std::make_unique<GpioPinRaspberry::PrivateData>();
   printf("PC-Keys:\n");
   printf("'a': previous file       'a'-long: playback-fast-backward\n");
   printf("'s': next     file       's'-long: playback-fast-forward\n");
   printf("'d': previous directory  'd'-long: fast change files\n");
   printf("'f': next     directory  'f'-long: fast change files\n");
   printf("'q': decrease volume     'w': increase volume\n");
   printf("'i': request playback informations\n");
   printf("'x': quit    'k': simulate automatic shutdown\n\n");
}

// ----------------------------------------------------------------------------

GpioPinRaspberry::~GpioPinRaspberry()
{
}

// ----------------------------------------------------------------------------

bool GpioPinRaspberry::GpioProblemDetected()
{
   return false;
}

// ----------------------------------------------------------------------------

void GpioPinRaspberry::ConfigureInput(int /*nGpioNumber*/, bool /*bInitPullUp*/)
{
}

// ----------------------------------------------------------------------------

bool GpioPinRaspberry::IsKeyPressed(int nGpioNumber)
{
   bool bPressed = false;

   char keys_return[32] = {};
   XQueryKeymap(m_pPriv->m_pDisplay, keys_return);

   char key = 0;

   if ((keys_return[6] & 0x20) != 0) key = 'x';  // GPIO # 17 red     on off
   if ((keys_return[4] & 0x40) != 0) key = 'a';  // GPIO # 21 white   file-back
   if ((keys_return[4] & 0x80) != 0) key = 's';  // GPIO # 13 white   file-forw
   if ((keys_return[5] & 0x01) != 0) key = 'd';  // GPIO # 16 + 21    Directory-Back: info + back
   if ((keys_return[5] & 0x02) != 0) key = 'f';  // GPIO # 16 + 13    Directory-Forw: info + forw
   if ((keys_return[3] & 0x01) != 0) key = 'q';  // GPIO # 20 black   quiet
   if ((keys_return[3] & 0x02) != 0) key = 'w';  // GPIO # 19 black   loud
   if ((keys_return[3] & 0x80) != 0) key = 'i';  // GPIO # 16 yellow  info
   if ((keys_return[5] & 0x20) != 0) key = 'k';  // simulate auto-shutdown  PC-version only

   switch(nGpioNumber)
   {
      case 17: bPressed = (key == 'x');                             break; // on off
      case 21: bPressed = (key == 'a' || key == 'd');               break; // back
      case 13: bPressed = (key == 's' || key == 'f');               break; // forw
      case 16: bPressed = (key == 'd' || key == 'f' || key == 'i'); break; // info
      case 20: bPressed = (key == 'q');                             break; // quiet
      case 19: bPressed = (key == 'w');                             break; // loud
      case 99: bPressed = (key == 'k');                             break; // simulate auto-shutdown
      default: bPressed = false;
   }

   return bPressed;
}
