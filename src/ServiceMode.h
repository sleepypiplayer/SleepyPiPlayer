// ----------------------------------------------------------------------------
// SleepyPiPlayer:  ServiceMode
//    configured by sleepy.cfg
//    creates threads allowing parallel audio feedback without delay
//    - disable WLAN on startup
//    - remount partitions ReadWrite
//    - activate WLAN in order to update MP3-files
//      either as AccessPoint = create WLAN and allow connections from clients
//      or as Client          = connect to an WLAN
// ----------------------------------------------------------------------------
#pragma once
#include <string>
#include "SystemConfigFile.h"

class ServiceMode
{
public:
   ServiceMode(SystemConfigFile& config);
   virtual ~ServiceMode();

   void DisableWlan();   // disable  WLAN on startup
   void StartService();  // activate WLAN / remount RW

   std::string GetFallbackPassphrase(); // no fallback: empty string else e.g. "123123123"

private:
   class PrivateData;
   PrivateData* m_pPriv;
};

