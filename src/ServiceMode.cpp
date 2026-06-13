// SleepyPiPlayer:  ServiceMode
//    configured by sleepy.cfg
//    creates threads allowing parallel audio feedback without delay
//    - disable WLAN on startup
//    - remount partitions ReadWrite
//    - activate WLAN in order to update MP3-files
//      either as AccessPoint = create WLAN and allow connections from clients
//      or as Client          = connect to an WLAN
// license: free software   (sleepypiplayer(at)saftfresse.de)  [A.D.2026]
// - Do whatever you want with the software.
// - Do not claim my work as your own.
// - THIS SOFTWARE COMES WITH NO WARRANTIES, USE AT YOUR OWN RISK
// ----------------------------------------------------------------------------
#include "ServiceMode.h"
#include <string>
#include <list>
#include <cstdio>
#include <thread>
#include <atomic>
#include <chrono>
#include <iostream>
#include <stdio.h>
#include <mutex>

/*

----- access-point ok

root@DietPi:~# root@DietPi:~# iw wlan0 info
Interface wlan0
        ifindex 3
        wdev 0x1
        addr dc:a6:32:d7:63:a4
        ssid SleepyPiPlayer
        type AP
        wiphy 0
        channel 3 (2422 MHz), width: 20 MHz, center1: 2422 MHz
        txpower 31.00 dBm

root@DietPi:~# ip a list wlan0
3: wlan0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc pfifo_fast state UP group default qlen 1000
    link/ether dc:a6:32:d7:63:a4 brd ff:ff:ff:ff:ff:ff
    inet 192.168.9.1/24 brd 192.168.9.255 scope global wlan0
       valid_lft forever preferred_lft forever
    inet6 fe80::dea6:32ff:fed7:74e4/64 scope link proto kernel_ll
       valid_lft forever preferred_lft forever


----- access-point passphrase too short only "123"


root@DietPi:/SLEEPY_FW/sleepy/src# ip a list wlan0
3: wlan0: <NO-CARRIER,BROADCAST,MULTICAST,UP> mtu 1500 qdisc pfifo_fast state DOWN group default qlen 1000
    link/ether dc:a6:32:d7:63:a4 brd ff:ff:ff:ff:ff:ff

root@DietPi:/SLEEPY_FW/sleepy/src# iw wlan0 info
Interface wlan0
        ifindex 3
        wdev 0x1
        addr dc:a6:32:d7:63:a4
        type managed
        wiphy 0
        channel 10 (2457 MHz), width: 20 MHz, center1: 2457 MHz
        txpower 31.00 dBm

----- rfkill block wlan

root@DietPi:/SLEEPY_FW/sleepy/src# iw wlan0 info
Interface wlan0
        ifindex 3
        wdev 0x1
        addr dc:a6:32:d7:63:a4
        type AP
        wiphy 0
        channel 3 (2422 MHz), width: 20 MHz, center1: 2422 MHz
        
root@DietPi:/SLEEPY_FW/sleepy/src# ip a list wlan0
3: wlan0: <BROADCAST,MULTICAST> mtu 1500 qdisc pfifo_fast state DOWN group default qlen 1000
    link/ether dc:a6:32:d7:63:a4 brd ff:ff:ff:ff:ff:ff
    inet 192.168.9.1/24 brd 192.168.9.255 scope global wlan0
       valid_lft forever preferred_lft forever

--------------------------------------------------------------------------
--------------------------------------------------------------------------

----- client ok

root@DietPi:/SLEEPY_FW/sleepy/src# iw wlan0 info
Interface wlan0
        ifindex 3
        wdev 0x1
        addr dc:a6:32:d7:63:a4
        ssid MyOwnWifi_2.4GHz
        type managed
        wiphy 0
        channel 10 (2457 MHz), width: 20 MHz, center1: 2457 MHz
        txpower 31.00 dBm
        
 root@DietPi:/SLEEPY_FW/sleepy/src# ip a list wlan0
3: wlan0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc pfifo_fast state UP group default qlen 1000
    link/ether dc:a6:32:d7:63:a4 brd ff:ff:ff:ff:ff:ff
    inet 192.168.8.42/24 brd 192.168.8.255 scope global dynamic wlan0
       valid_lft 86314sec preferred_lft 86314sec
    inet6 fe80::dea6:32ff:fed7:74e4/64 scope link proto kernel_ll
       valid_lft forever preferred_lft forever       
        

-----  client wrong passphrase

root@DietPi:/SLEEPY_FW/sleepy/src# iw wlan0 info
Interface wlan0
        ifindex 3
        wdev 0x1
        addr dc:a6:32:d7:63:a4
        ssid MyOwnWifi_2.4GHz
        type managed
        wiphy 0
        channel 10 (2457 MHz), width: 20 MHz, center1: 2457 MHz
        txpower 31.00 dBm

root@DietPi:/SLEEPY_FW/sleepy/src# ip a list wlan0
3: wlan0: <NO-CARRIER,BROADCAST,MULTICAST,UP> mtu 1500 qdisc pfifo_fast state DOWN group default qlen 1000
    link/ether dc:a6:32:d7:63:a4 brd ff:ff:ff:ff:ff:ff

*/


// ============================================================================

class ServiceMode::PrivateData
{
public:
   PrivateData()
   {
      m_timeConstructor = std::chrono::steady_clock::now();
   }

   virtual ~PrivateData()
   {
      PrivateData* pDeleteFallback = nullptr;
      {
         std::lock_guard<std::mutex> guard(m_Mutex);
         m_bAborting = true;
         pDeleteFallback     = m_pFallbackInstance;
         m_pFallbackInstance = nullptr;
      }

      delete pDeleteFallback;

      if (m_threadDisableWlan.joinable())
      {
         m_threadDisableWlan.join();
      }
      if (m_threadStartService.joinable())
      {
         m_threadStartService.join();
      }
   }

   bool ExecCommand(const char* txtCommand);

   std::string GetOutputOfCommand(const char* txtCommand);
   bool ContainsText(std::string& txtHaystack, const char* txtNeedle);

   bool CreateDnsmasqConf();
   bool CreateHostapdConf();
   bool CreateInterfacesWifiAp();
   bool CreateWpaSupplicantConf();
   bool WriteFile(std::list<std::string>& content, std::string txtFilePath);
   bool FileContentIsEqual(const char* content, int nFileSize, std::string txtFilePath);

   bool MountReadWrite();
   bool StartWlanAccessPoint();
   bool StartWlanClient();
   void DisableWlan();   

   bool IsWlanClientOK();
   bool IsWlanAccessPointOK();

   void CreateFallbackInstance();
   std::string GetFallbackPassphrase();
   std::string CreateFallbackPassphrase();

   bool         m_bAccessPointMode = true;
   std::string  m_txtWlanSSID;
   std::string  m_txtWlanPassw;
   std::string  m_txtWlanCountry;

   std::atomic<bool>  m_bAborting = false;
   std::atomic<bool>  m_bIsFallbackInstance = false;
   PrivateData*       m_pFallbackInstance = nullptr;
   std::string        m_txtFallbackPassphrase;

   // create some kind of random-number for fallback passphrase
   std::chrono::time_point<std::chrono::steady_clock>  m_timeConstructor;

   std::list<std::string> m_listPartitionsReadWrite;

   std::mutex  m_Mutex;
   std::thread m_threadDisableWlan;
   std::thread m_threadStartService;

   static void ThreadFunctionDisableWlan(ServiceMode::PrivateData* pThis);
   static void ThreadFunctionStartService(ServiceMode::PrivateData* pThis);
};

// ----------------------------------------------------------------------------

void ServiceMode::PrivateData::ThreadFunctionDisableWlan(ServiceMode::PrivateData* pThis)
{
   pThis->DisableWlan();
}

// ----------------------------------------------------------------------------

void ServiceMode::PrivateData::ThreadFunctionStartService(ServiceMode::PrivateData* pThis)
{
   if (pThis->m_bAccessPointMode)
   {
      pThis->StartWlanAccessPoint();
   }
   else
   {
      pThis->StartWlanClient();
   }   
}

// ----------------------------------------------------------------------------

bool ServiceMode::PrivateData::CreateDnsmasqConf() // AccessPoint-Mode
{
   std::list<std::string> content;

   content.push_back("interface=wlan0");
   content.push_back("dhcp-range=192.168.9.2,192.168.9.99,24h  # IP range for clients (PC)");
   content.push_back("dhcp-option=3,192.168.9.1                # Gateway IP address");
   content.push_back("dhcp-option=6,192.168.9.1                # DNS server IP address");
   content.push_back("server=8.8.8.8                           #Dummy DNS entry. Not needed but prevents errors");
   content.push_back("log-facility=/var/log/dnsmasq.log");

   return WriteFile(content, "/etc/dnsmasq.conf");
}

// ----------------------------------------------------------------------------

bool ServiceMode::PrivateData::CreateHostapdConf() // AccessPoint-Mode
{
   std::list<std::string> content;

   content.push_back("ssid="+m_txtWlanSSID);
   content.push_back("wpa_passphrase="+m_txtWlanPassw);
   content.push_back("ieee80211n=0");
   content.push_back("ieee80211ac=0");
   content.push_back("ieee80211ax=0");
   content.push_back("wmm_enabled=0");
   content.push_back("hw_mode=g");
   content.push_back("channel=3");
   content.push_back("interface=wlan0");
   content.push_back("wpa=2");
   content.push_back("wpa_key_mgmt=WPA-PSK");
   content.push_back("wpa_pairwise=TKIP");
   content.push_back("rsn_pairwise=CCMP");
   content.push_back("ignore_broadcast_ssid=0");
   content.push_back("country_code="+m_txtWlanCountry);

   return WriteFile(content, "/etc/hostapd/hostapd.conf");
}


// ----------------------------------------------------------------------------

bool ServiceMode::PrivateData::CreateInterfacesWifiAp() // AccessPoint-Mode
{
   std::list<std::string> content;

   content.push_back("# Location: /etc/network/interfaces_wifi_ap");
   content.push_back("# WLAN as access-point with static IP 192.168.9.1");
   content.push_back("");
   content.push_back("# WiFi");
   content.push_back("allow-hotplug wlan0");
   content.push_back("iface wlan0 inet static");
   content.push_back("address 192.168.9.1");
   content.push_back("netmask 255.255.255.0");
   content.push_back("wireless-power off");

   return WriteFile(content, "/etc/network/interfaces_wifi_ap");
}

// ----------------------------------------------------------------------------

bool ServiceMode::PrivateData::CreateWpaSupplicantConf()  // Client-Mode
{
   std::list<std::string> content;

   content.push_back("# WiFi country code, set here in case the access point does send one");
   content.push_back("country="+m_txtWlanCountry);
   content.push_back("# Grant all members of group \"netdev\" permissions to configure WiFi, e.g. via wpa_cli or wpa_gui");
   content.push_back("ctrl_interface=DIR=/run/wpa_supplicant GROUP=netdev");
   content.push_back("# Allow wpa_cli/wpa_gui to overwrite this config file");
   content.push_back("update_config=1");
   content.push_back("network={");
   content.push_back("        ssid=\""+m_txtWlanSSID+"\"");
   content.push_back("        scan_ssid=1");
   content.push_back("        key_mgmt=WPA-PSK");
   content.push_back("        psk=\""+m_txtWlanPassw+"\"");
   content.push_back("}");

   return WriteFile(content, "/etc/wpa_supplicant/wpa_supplicant.conf");
}

// ----------------------------------------------------------------------------

bool ServiceMode::PrivateData::WriteFile(std::list<std::string>& content, std::string txtFilePath)
{
   bool bOK = false;
   int nFileSize = 0;
   for (std::string line : content)
   {
      nFileSize += line.length() + 1;
   }
   char* pBuffer = new char[nFileSize];
   if (pBuffer)
   {
      int nIndex = 0;
      for (std::string line : content)
      {
         int nLineLength = line.length();
         const char* txtLine = line.c_str();
         for (int i = 0; i < nLineLength; i++)
         {
            pBuffer[nIndex] = txtLine[i];
            ++nIndex;
         }
         pBuffer[nIndex] = '\n';
         ++nIndex;
      }
      bOK = FileContentIsEqual(pBuffer, nFileSize, txtFilePath);
      if (!bOK)
      {
         std::FILE* pFile = std::fopen(txtFilePath.c_str(), "wb");
         if (pFile)
         {
            int nNofWritten = std::fwrite(pBuffer, 1, nFileSize, pFile);
            std::fclose(pFile);
            bOK = nNofWritten == nFileSize;
         }
      }
      delete [] pBuffer;
   }

   if (!bOK)
   {
      printf("##==  WriteFile failed: %s\n", txtFilePath.c_str());
   }

   return bOK;
}

// ----------------------------------------------------------------------------

bool ServiceMode::PrivateData::FileContentIsEqual(const char* content, int nFileSize, std::string txtFilePath)
{
   bool bEqual = false;
   char* pReadBuffer = new char[nFileSize+10];
   std::FILE* pFile = std::fopen(txtFilePath.c_str(), "rb");
   if (pFile)
   {
      int nNofRead = std::fread(pReadBuffer, 1, nFileSize+10, pFile);
      std::fclose(pFile);
      if (nNofRead == nFileSize)
      {
         bEqual = true;
         for (int i = 0; i < nFileSize; i++)
         {
            if (pReadBuffer[i] != content[i])
            {
               bEqual = false;
            }
         }
      }
   }
   return bEqual;
}

// ----------------------------------------------------------------------------

bool ServiceMode::PrivateData::MountReadWrite() // partitions on SD-card
{
   bool bOK = true;
   std::system("sudo mount -o remount,rw /");
   for (std::string txtPartition : m_listPartitionsReadWrite)
   {
      if (txtPartition.length() > 140 || txtPartition.length() < 1)
      {
         bOK = false;
      }
      else
      {
         char txtCommand[200] = {};
         std::snprintf(txtCommand, 199, "sudo mount -o remount,rw %s", txtPartition.c_str());
         std::system(txtCommand);
      }
   }
   return bOK;
}

// ----------------------------------------------------------------------------

bool ServiceMode::PrivateData::ExecCommand(const char* txtCommand)
{
   int nReturned = std::system(txtCommand);

   if (nReturned != 0)
   {
      printf("##== command %s returned %i\n", txtCommand, nReturned);
   }

   return (nReturned == 0);
}

// ----------------------------------------------------------------------------

std::string ServiceMode::PrivateData::GetOutputOfCommand(const char* txtCommand)
{
   std::string txtOutput;
   FILE* pPipe = popen(txtCommand, "r");
   if (pPipe)
   {
      char txtBuffer[4096] = {};
      const char* result = fgets(txtBuffer, 4095, pPipe);
      while (result)
      {
         txtOutput.append(txtBuffer);
         for (int i = 0; i < 4096; i++) txtBuffer[i] = 0;
         result = fgets(txtBuffer, 4095, pPipe);
      }
      pclose(pPipe);
   }

   return txtOutput;
}

// ----------------------------------------------------------------------------

bool ServiceMode::PrivateData::ContainsText(std::string& txtHaystack, const char* txtNeedle)
{
   return (txtHaystack.find(txtNeedle,0) != std::string::npos);
}

// ----------------------------------------------------------------------------

void ServiceMode::PrivateData::DisableWlan()
{
   ExecCommand("sudo rfkill block bluetooth");
   ExecCommand("sudo rfkill block wifi");
}

// ----------------------------------------------------------------------------

bool ServiceMode::PrivateData::StartWlanAccessPoint()
{
   bool bOK = true;
   
   bOK = (bOK && MountReadWrite());
   bOK = (bOK && CreateHostapdConf());
   bOK = (bOK && CreateDnsmasqConf());
   bOK = (bOK && CreateInterfacesWifiAp());

   bOK = (bOK && ExecCommand("sudo rfkill unblock wifi"));   
   bOK = (bOK && ExecCommand("sudo systemctl unmask hostapd"));
   bOK = (bOK && ExecCommand("sudo systemctl stop hostapd"));
   bOK = (bOK && ExecCommand("sudo systemctl stop dnsmasq"));
   bOK = (bOK && ExecCommand("sudo update-rc.d dnsmasq enable"));
   bOK = (bOK && ExecCommand("sudo update-rc.d hostapd enable"));
   bOK = (bOK && ExecCommand("sudo systemctl start hostapd"));
   bOK = (bOK && ExecCommand("sudo ifdown wlan0"));
                 ExecCommand("sudo ip address del 192.168.9.1/24 dev wlan0");
   bOK = (bOK && ExecCommand("sudo ifup -v --interfaces /etc/network/interfaces_wifi_ap wlan0"));
   bOK = (bOK && ExecCommand("sudo systemctl restart hostapd"));
   bOK = (bOK && ExecCommand("sudo systemctl start dnsmasq"));

   if (!bOK)  printf("##== StartWlanAccessPoint() failed\n");

   // --- wait some time ---
   std::chrono::duration<double> durationWait = std::chrono::seconds(25);
   std::chrono::time_point<std::chrono::steady_clock> timeStart;
   timeStart = std::chrono::steady_clock::now();
   while (!m_bAborting && ((std::chrono::steady_clock::now() - timeStart) < durationWait))
   {
      std::this_thread::sleep_for( std::chrono::milliseconds(50) );
   }

   bool bWlanOK = IsWlanAccessPointOK();

   if (!bWlanOK && !m_bAborting && !m_bIsFallbackInstance)
   {
      CreateFallbackInstance();
   }

   return bOK;
}

// ----------------------------------------------------------------------------

bool ServiceMode::PrivateData::StartWlanClient()
{
   bool bOK = true;
   
   bOK = (bOK && MountReadWrite());
   bOK = (bOK && CreateWpaSupplicantConf());

   bOK = (bOK && ExecCommand("sudo rfkill unblock wifi"));   
   bOK = (bOK && ExecCommand("sudo systemctl stop hostapd"));
   bOK = (bOK && ExecCommand("sudo systemctl stop dnsmasq"));
   bOK = (bOK && ExecCommand("sudo update-rc.d dnsmasq disable"));
   bOK = (bOK && ExecCommand("sudo update-rc.d hostapd disable"));
   bOK = (bOK && ExecCommand("sudo ifdown wlan0"));
                 ExecCommand("sudo ip address del 192.168.9.1/24 dev wlan0");
   bOK = (bOK && ExecCommand("sudo ifup -v wlan0"));

   if (!bOK)  printf("##== StartWlanClient() failed\n");

   // --- wait some time ---
   std::chrono::duration<double> durationWait = std::chrono::seconds(25);
   std::chrono::time_point<std::chrono::steady_clock> timeStart;
   timeStart = std::chrono::steady_clock::now();
   while (!m_bAborting && ((std::chrono::steady_clock::now() - timeStart) < durationWait))
   {
      std::this_thread::sleep_for( std::chrono::milliseconds(50) );
   }

   bool bWlanOK = IsWlanClientOK();
   
   if (!bWlanOK && !m_bAborting && !m_bIsFallbackInstance)
   {
      CreateFallbackInstance();
   }

   return bOK;
}

// ----------------------------------------------------------------------------

void ServiceMode::PrivateData::CreateFallbackInstance()
{
   std::lock_guard<std::mutex> guard(m_Mutex);
   if (m_pFallbackInstance == nullptr && !m_bIsFallbackInstance && !m_bAborting)
   {
      printf("\n\n##== CreateFallbackInstance()\n");
      m_txtFallbackPassphrase = CreateFallbackPassphrase();

      // this is the fallback - do not use any data from config because it could cause problems
      m_pFallbackInstance = new PrivateData();
      m_pFallbackInstance->m_bIsFallbackInstance = true;
      m_pFallbackInstance->m_bAccessPointMode    = true;
      m_pFallbackInstance->m_txtWlanSSID         = "SleepyPiFallback";
      m_pFallbackInstance->m_txtWlanPassw        = m_txtFallbackPassphrase;
      m_pFallbackInstance->m_txtWlanCountry      = "DE";  // should be OK for almaost all countries

      std::thread threadTempStart( ServiceMode::PrivateData::ThreadFunctionStartService, m_pFallbackInstance );
      m_pFallbackInstance->m_threadStartService.swap(threadTempStart);
   }
}

// ----------------------------------------------------------------------------

std::string ServiceMode::PrivateData::CreateFallbackPassphrase()
{
   std::chrono::time_point<std::chrono::steady_clock>  timeNow = std::chrono::steady_clock::now(); 
   std::chrono::duration<double, std::milli> durationSinceStart = timeNow - m_timeConstructor;
   uint64_t nNumber = durationSinceStart.count();
   uint64_t nNumber1 = (nNumber / 100) % 10;  // 0 .. 9
   uint64_t nNumber2 = (nNumber /  10) % 10;  // 0 .. 9
   uint64_t nNumber3 = (nNumber /   1) % 10;  // 0 .. 9
   if (nNumber1 == nNumber2)
   {
      nNumber2 = (nNumber2 + 1) % 10;
   }
   if (nNumber2 == nNumber3)
   {
      nNumber3 = (nNumber3 + 1) % 10;
   }
   char txtNumbers[10] = {};

   txtNumbers[0] = nNumber1 + '0';
   txtNumbers[1] = nNumber2 + '0';
   txtNumbers[2] = nNumber3 + '0';

   txtNumbers[3] = nNumber1 + '0';
   txtNumbers[4] = nNumber2 + '0';
   txtNumbers[5] = nNumber3 + '0';

   txtNumbers[6] = nNumber1 + '0';
   txtNumbers[7] = nNumber2 + '0';
   txtNumbers[8] = nNumber3 + '0';     

   txtNumbers[9] = 0;

   return std::string(txtNumbers);
}

// ----------------------------------------------------------------------------

std::string ServiceMode::PrivateData::GetFallbackPassphrase()
{
   std::string txtFallbackPassphrase;
   std::lock_guard<std::mutex> guard(m_Mutex);
   txtFallbackPassphrase = m_txtFallbackPassphrase;
   return txtFallbackPassphrase;
}

// ----------------------------------------------------------------------------

bool ServiceMode::PrivateData::IsWlanClientOK()
{
   bool bOK = true;

   if (bOK)
   {
      std::string txtOutput = GetOutputOfCommand("iw wlan0 info");
      if (!ContainsText(txtOutput, "type managed"))
      {
         bOK = false;
      }
   }

   if (bOK)
   {
      std::string txtOutput = GetOutputOfCommand("ip a list wlan0");
      if (!ContainsText(txtOutput, "state UP"))
      {
         bOK = false;
      }
      if (!ContainsText(txtOutput, "inet "))
      {
         bOK = false;
      }
   }

   return bOK;
}

// ----------------------------------------------------------------------------

bool ServiceMode::PrivateData::IsWlanAccessPointOK()
{
   bool bOK = true;

   if (bOK)
   {
      std::string txtOutput = GetOutputOfCommand("iw wlan0 info");
      if (!ContainsText(txtOutput, "type AP"))
      {
         bOK = false;
      }
   }

   if (bOK)
   {
      std::string txtOutput = GetOutputOfCommand("ip a list wlan0");
      if (!ContainsText(txtOutput, "state UP"))
      {
         bOK = false;
      }
      if (!ContainsText(txtOutput, "inet "))
      {
         bOK = false;
      }
   }

   return bOK;
}

// ============================================================================

ServiceMode::ServiceMode(SystemConfigFile& config)
{   
   m_pPriv = new ServiceMode::PrivateData();

   m_pPriv->m_bAccessPointMode = config.WlanAccessPointMode();
   m_pPriv->m_txtWlanSSID      = config.WlanSSID();
   m_pPriv->m_txtWlanPassw     = config.WLANPassphrase();
   m_pPriv->m_txtWlanCountry   = config.WLANCountry();
   m_pPriv->m_listPartitionsReadWrite = config.ServiceRemountReadWriteList();
}

// ----------------------------------------------------------------------------

ServiceMode::~ServiceMode()
{
   delete m_pPriv;
   m_pPriv = nullptr;
}

// ----------------------------------------------------------------------------

void ServiceMode::DisableWlan()
{
   if (m_pPriv->m_threadDisableWlan.joinable())
   {
      m_pPriv->m_threadDisableWlan.join();
   }
   if (m_pPriv->m_threadStartService.joinable())
   {
      m_pPriv->m_threadStartService.join();
   }      

#ifndef PC_TESTVERSION
   std::thread threadTempDisable = std::thread( PrivateData::ThreadFunctionDisableWlan, m_pPriv );
   m_pPriv->m_threadDisableWlan.swap(threadTempDisable);
#endif
}

// ----------------------------------------------------------------------------

void ServiceMode::StartService()
{
   if (m_pPriv->m_threadDisableWlan.joinable())
   {
      m_pPriv->m_threadDisableWlan.join();
   }
   if (m_pPriv->m_threadStartService.joinable())
   {
      m_pPriv->m_threadStartService.join();
   }      

#ifndef PC_TESTVERSION
   std::thread threadTempStart( ServiceMode::PrivateData::ThreadFunctionStartService, m_pPriv );
   m_pPriv->m_threadStartService.swap(threadTempStart);
#endif
}

// ----------------------------------------------------------------------------

std::string ServiceMode::GetFallbackPassphrase()
{
   return m_pPriv->GetFallbackPassphrase();
}

