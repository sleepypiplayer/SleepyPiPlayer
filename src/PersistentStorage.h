// ----------------------------------------------------------------------------
// SleepyPiPlayer:  PersistentStorage
//   "STORAGE_DIR" 100 x 4MB-files store volume-setting and playback-position
//   000PersistStorage.bin .. 099PersistStorage.bin
//  write-cycles are limited on SD-cards
//   ADVICE: use industrial micro-SD-cards with SLC-Flash
//    Even if a partition is mounted/unmounted read-only,
//     a changed mount/unmount-counter is written to the SD-card.
//    If a partition is mounted/unmounted read-write without writing a file,
//     additional changes are written to the SD-card.
//   try to leave FAT unchanged: the file-size will never change
//   cat /sys/block/mmcblk0/device/preferred_erase_size   4194304
//     so better write a saveset every 4MB: 100 storage-files each 4MB
//   first 4 characters of a saveset is a counter:
//     only one saveset with the hightes number is used for reading
//     if number   "0001" co-exists with "9999":
//       there is a number-overflow: 0001 will be considered as higher number
// ----------------------------------------------------------------------------
#pragma once
#include <string>
#include <memory>

class PersistentStorage
{
public:
   PersistentStorage(std::string txtStorageDirPath);  // constructor reads files
   virtual ~PersistentStorage();  // destructor does not write file

   bool WriteFile();               // true if successful
   bool ChecksumProblemDetected(); // true if a checksum failed when reading the storage

   int  GetVolume();
   void SetVolume(int nVolume);

   std::string GetPlaybackPath();
   double      GetPlaybackTime(); // in seconds
   void        SetPlayback(std::string txtPath, double dTime);
private:
   class PrivateData;
   std::unique_ptr<PrivateData> m_pPriv;
};
