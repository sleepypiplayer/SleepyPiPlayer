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
// license: free software   (sleepypiplayer(at)saftfresse.de)  [A.D.2025]
// - Do whatever you want with the software.
// - Do not claim my work as your own.
// - THIS SOFTWARE COMES WITH NO WARRANTIES, USE AT YOUR OWN RISK
// ----------------------------------------------------------------------------
#include "PersistentStorage.h"
#include <cstdio>
#include <cstring>


// 0x000..0x003  0000..0003   04_char Storage-Number
// 0x00A..0x00D  0010..0012   03_char Volume
// 0x014..0x02D  0020..0045   25_char time-pos in seconds
// 0x032..0x288  0050..0699  650_char file-path
// 0x3E8..0x3EF  1000..1007    8_char CheckSum

// ============================================================================
class PersistentStorage::PrivateData
{
public:
   PrivateData()
   {
      for (int i = 0; i < NOF_RECORDS; i++) m_arrStorageNumber[i] = -1;
   }
   virtual ~PrivateData() {}

   std::string m_txtStorageDirPath;

   bool        m_bDataModified = false;
   int         m_nMaxStorageNumber = -1;


   std::string m_txtPlaybackPath;
   double      m_dPlaybackPos = 0.0; // seconds
   int         m_nVolume = 70;       // 0 .. 100%

   bool        m_bChecksumProblem = false;

   static const int  RECORD_SIZE = 4096*1024;    // 4MB per file
   static const int  DATA_SIZE   = 1024;         // size of stored data (3 copies in one file)
   static const int  NOF_RECORDS = 100;          // 100 files

   int m_arrStorageNumber[NOF_RECORDS];

   std::string GetStorageFilePath(int nRecordIdx);
   void ReadFiles();
   bool WriteFile();
   void EnsureFilesExist();

   int  CalcFreeRecordIdx();
   void PrepareData(unsigned char* buffer);
   void DecodeData(int nRecordIdx, unsigned char* buffer);
   int  CalcChecksum(const unsigned char* buffer);
};

// ----------------------------------------------------------------------------

int PersistentStorage::PrivateData::CalcFreeRecordIdx()
{
   int nFreeRecordIdx = -1;
   int nMaxNumber1 = -1;
   int nMaxNumber2 = -1;
   for (int i = 0; i < NOF_RECORDS; i++)
   {
      if (m_arrStorageNumber[i] < 0)
      {
         if (nFreeRecordIdx < 0)
         {
            nFreeRecordIdx = i;
         }
      }
      if (m_arrStorageNumber[i] > 5000)
      {
         if (m_arrStorageNumber[i] > nMaxNumber2)
         {
            nMaxNumber2 = m_arrStorageNumber[i];
         }
      }
      else
      {
         if (m_arrStorageNumber[i] > nMaxNumber1)
         {
            nMaxNumber1 = m_arrStorageNumber[i];
         }
      }
   }
   if (nMaxNumber2 > 8000 && nMaxNumber1 >= 0)
   {
      m_nMaxStorageNumber = nMaxNumber1;
   }
   else
   {
      m_nMaxStorageNumber = std::max(nMaxNumber1, nMaxNumber2);
   }
   if (nFreeRecordIdx < 0)
   {
      int nMinNumber = (nMaxNumber2 > 8000 && nMaxNumber1 >= 0)? 5000 : 0;
      nFreeRecordIdx = 0;
      for (int i = 0; i < NOF_RECORDS; i++)
      {
         int nNumberFree  = m_arrStorageNumber[nFreeRecordIdx];
         if (nNumberFree < nMinNumber)
         {
            nNumberFree += 10000;
         }
         int nNumberCheck = m_arrStorageNumber[i];
         if (nNumberCheck < nNumberFree && nNumberCheck > nMinNumber)
         {
            nFreeRecordIdx = i;
         }
      }
   }
   return nFreeRecordIdx;
}

// ----------------------------------------------------------------------------

int  PersistentStorage::PrivateData::CalcChecksum(const unsigned char* buffer)
{
   static_assert(DATA_SIZE > 1020);
   static_assert(RECORD_SIZE > 3* DATA_SIZE);

   int nChkSum = 0;
   for (int i = 0; i < 1000; i++)
   {
      unsigned int nAdd = buffer[i];
      nAdd = nAdd << (i % 10);
      nChkSum = (nChkSum ^ nAdd);
   }
   return nChkSum;
}

// ----------------------------------------------------------------------------
// remember stored data if storage-number bigger than the numbers read so far
void PersistentStorage::PrivateData::DecodeData(int nRecordIdx, unsigned char* buffer)
{
   int nCalculatedCheckSum = CalcChecksum(buffer);

   // --- storage-number --
   int nStorageNumber = -1;
   try
   {
      char bufferNumber[10] = {};
      bool bEmpty = true;
      for (int i = 0; i < 4; i++)
      {
         bufferNumber[i] = buffer[i];
         bEmpty = bEmpty && (buffer[i] == ' ');
      }
      nStorageNumber = bEmpty? -1 : std::stoi(bufferNumber);
   }
   catch(...)
   {
      nStorageNumber = -1;
   }

   // --- checksum ---
   int nStoredCheckSum = -1;
   try
   {
      char bufferNumber[10] = {};
      for (int i = 0; i < 8; i++)
         bufferNumber[i] = buffer[i+1000];
      nStoredCheckSum = std::stoi(bufferNumber);
   }
   catch(...)
   {
      nStoredCheckSum = -1;
   }

   // --- volume ---
   int nVolume = 70;
   try
   {
      char bufferNumber[10] = {};
      for (int i = 0; i < 3; i++)
         bufferNumber[i] = buffer[i+10];
      nVolume = std::stoi(bufferNumber);
   }
   catch(...)
   {
      nVolume = 70;
   }

   // --- position in seconds ---
   double dPosition = 0.0;
   try
   {
      char bufferNumber[30] = {};
      for (int i = 0; i < 25; i++)
         bufferNumber[i] = buffer[i+20];
      dPosition = std::stod(bufferNumber);
   }
   catch(...)
   {
      dPosition = 0.0;
   }

   // --- path ---
   std::string txtPath;
   {
      char bufferText[702] = {};
      bool bStarted = false;
      for (int i =699; i >= 0; i--)
      {
         bStarted = bStarted || buffer[i+50] != ' ';
         if (bStarted)
         {
            bufferText[i] = buffer[i+50];
         }
      }
      txtPath = bufferText;
   }

   if (nCalculatedCheckSum != nStoredCheckSum && nStorageNumber >= 0)
   {
      m_bChecksumProblem = true;
      nStorageNumber = -1;
   }
   m_arrStorageNumber[nRecordIdx] = nStorageNumber;

   if (nStorageNumber >= 0)
   {
      if (m_nMaxStorageNumber < 0)
      {
         m_nMaxStorageNumber = nStorageNumber;
      }
      else if (m_nMaxStorageNumber > 8000 && nStorageNumber < 2000)
      {
         m_nMaxStorageNumber = nStorageNumber;
      }
      if (nStorageNumber > m_nMaxStorageNumber && nStorageNumber < m_nMaxStorageNumber + 2000)
      {
         m_nMaxStorageNumber = nStorageNumber;
      }
      if (m_nMaxStorageNumber == nStorageNumber)
      {
         m_txtPlaybackPath = txtPath;
         m_dPlaybackPos    = dPosition;
         m_nVolume         = nVolume;
      }
   }
}

// ----------------------------------------------------------------------------
// convert mp3-file-path and volume to the correct format and calculate checksum
void PersistentStorage::PrivateData::PrepareData(unsigned char* buffer)
{
   for (int i = 0; i < DATA_SIZE; i++) buffer[i] = ' ';
   buffer[DATA_SIZE-1] = '\n';
   if (m_nMaxStorageNumber < 1)
   {
      m_nMaxStorageNumber = 1;
   }
   else
   {
      m_nMaxStorageNumber = (m_nMaxStorageNumber + 1) % 10000;
      m_nMaxStorageNumber = std::max(1, m_nMaxStorageNumber);
   }
   char txtTemp[100] = {};
   // ---  storage-number ---
   sprintf(txtTemp, "%04i", m_nMaxStorageNumber);
   for (int i = 0; i < 4; i++)
   {
      buffer[i] = txtTemp[i];
   }
   // --- volume ---
   m_nVolume = std::min(100, m_nVolume);
   m_nVolume = std::max(  0, m_nVolume);
   sprintf(&txtTemp[10], "%03i", m_nVolume);
   for (int i = 10; i < 13; i++)
   {
      buffer[i] = txtTemp[i];
   }
   // --- playback-pos ---
   sprintf(&txtTemp[20], "%015.6f", m_dPlaybackPos);
   for (int i = 20; i < 45; i++)
   {
      buffer[i] = txtTemp[i];
   }
   // --- path ---
   int nLengthPath = m_txtPlaybackPath.length();
   if (nLengthPath > 0 && nLengthPath < 700)
   {
      for (int i = 0 ; i < nLengthPath; i++)
      {
         buffer[i+50] = m_txtPlaybackPath.at(i);
      }
   }
   // --- replace null-bytes by spaces ---
   for (int i = 0; i < 1000; i++)
   {
      buffer[i] = (buffer[i] != 0)? buffer[i] : ' ';
   }
   // --- checksum ---
   int ChkSum = CalcChecksum(buffer);
   sprintf(&txtTemp[50], "%08i", ChkSum);
   for (int i = 50; i < 58; i++)
   {
      buffer[i+950] = txtTemp[i];
   }
}

// ----------------------------------------------------------------------------
// if not already present: create 100 files with 4MB each containing whitespace
void PersistentStorage::PrivateData::EnsureFilesExist()
{
   for (int nRecordIdx = 0; nRecordIdx < NOF_RECORDS; nRecordIdx++)
   {
      std::string txtFilePath = GetStorageFilePath(nRecordIdx);
      std::FILE* pFile = std::fopen(txtFilePath.c_str(), "rb");
      if (pFile)
      {
         std::fclose(pFile);
      }
      else
      {
         pFile = std::fopen(txtFilePath.c_str(), "w+b");
         if (pFile)
         {
            int nBufferSize = DATA_SIZE;
            void* pBuffer = std::malloc(nBufferSize);
            if (pBuffer)
            {
               for (int i = 0; i < nBufferSize; i++)  ((char*)pBuffer)[i] = ' ';
               int64_t nNofWritten = 1;
               int64_t nFileSize   = 0;
               while (nNofWritten > 0 && nFileSize < RECORD_SIZE)
               {
                  nNofWritten = std::fwrite(pBuffer, 1, nBufferSize, pFile);
                  nFileSize += nNofWritten;
               }
            }
            std::fclose(pFile);
         }
      }
   }
}

// ----------------------------------------------------------------------------
// e.g. /SLEEPY_SAVE/save/024PersistStorage.bin
//      RecordIdx = 24  but StorageNumber is probably different
std::string  PersistentStorage::PrivateData::GetStorageFilePath(int nRecordIdx)
{
   char txtNumber[20] = {};
   sprintf(txtNumber, "%03i", nRecordIdx);
   std::string txtRecordIdx(txtNumber);
   std::string txtStorageFilePath = m_txtStorageDirPath+std::string("/")+txtRecordIdx+std::string("PersistStorage.bin");
   return txtStorageFilePath;
}

// ----------------------------------------------------------------------------
// read all 100 files and search for the one with the maximum storage-number
void PersistentStorage::PrivateData::ReadFiles()
{
   m_bDataModified = false;

   for (int nRecordIdx = 0; nRecordIdx < NOF_RECORDS; nRecordIdx++)
   {
      std::string txtFilePath = GetStorageFilePath(nRecordIdx);
      std::FILE* pFile = std::fopen(txtFilePath.c_str(), "rb");

      if (pFile)
      {

         unsigned char buffer1[DATA_SIZE] = {};
         unsigned char buffer2[DATA_SIZE] = {};
         unsigned char buffer3[DATA_SIZE] = {};
         std::fread(buffer1, DATA_SIZE, 1, pFile);
         std::fread(buffer2, DATA_SIZE, 1, pFile);
         std::fread(buffer3, DATA_SIZE, 1, pFile);
         int nErr1 = std::memcmp(buffer1, buffer2, DATA_SIZE);
         int nErr2 = std::memcmp(buffer2, buffer3, DATA_SIZE);
         if (nErr1 != 0 || nErr2 != 0)
         {
            m_bChecksumProblem = true;
         }
         if (nErr1 == 0)
         {
            DecodeData(nRecordIdx, buffer1);
         }
         else if (nErr2  == 0)
         {
            DecodeData(nRecordIdx, buffer2);
         }
         else
         {
            DecodeData(nRecordIdx, buffer1);
         }

         std::fclose(pFile);
      }
   }
}

// ----------------------------------------------------------------------------

bool PersistentStorage::PrivateData::WriteFile()
{
   EnsureFilesExist();
   bool bSuccess = true;

   int nFreeRecordIdx = CalcFreeRecordIdx();

   printf("Persist: record:%i playback %s\n", nFreeRecordIdx, m_txtPlaybackPath.c_str());

   if (m_bDataModified && nFreeRecordIdx >= 0)
   {
      bSuccess = false;
      std::string txtFilePath = GetStorageFilePath(nFreeRecordIdx);
      std::FILE* pFile = std::fopen(txtFilePath.c_str(), "r+b");
      if (pFile == nullptr)
      {
         pFile = std::fopen(txtFilePath.c_str(), "w+b");
         if (pFile)
         {
            int nBufferSize = DATA_SIZE;
            void* pBuffer = std::malloc(nBufferSize);
            if (pBuffer)
            {
               for (int i = 0; i < nBufferSize; i++)  ((char*)pBuffer)[i] = ' ';
               int64_t nNofWritten = 1;
               int64_t nFileSize   = 0;
               while (nNofWritten > 1 && nFileSize < RECORD_SIZE)
               {
                  nNofWritten = std::fwrite(pBuffer, 1, nBufferSize, pFile);
                  nFileSize += nNofWritten;
               }
            }
            std::fclose(pFile);
            pFile = std::fopen(txtFilePath.c_str(), "r+b");
         }
      }
      if (pFile)
      {
         unsigned char buffer[3*DATA_SIZE] = {};
         std::memset(buffer, ' ', 3*DATA_SIZE);
         PrepareData(buffer);
         for (int iCpyIdx = 0; iCpyIdx < DATA_SIZE; iCpyIdx++)
         {
            buffer[1*DATA_SIZE + iCpyIdx] = buffer[iCpyIdx];
            buffer[2*DATA_SIZE + iCpyIdx] = buffer[iCpyIdx];
         }
         std::fwrite(buffer, 3*DATA_SIZE, 1, pFile);
         std::fflush(pFile);
         std::fclose(pFile);
         bSuccess = true;
         m_arrStorageNumber[nFreeRecordIdx] = m_nMaxStorageNumber;
      }
      else
      {
         printf("Persist: failed to open file %s\n", txtFilePath.c_str());
      }

      m_bDataModified = (!bSuccess);
   }
   else
   {
      printf("Persist not saved: (save to index: %i) (modified: %i)\n", nFreeRecordIdx, m_bDataModified);
   }
   return bSuccess;
}


// ============================================================================

PersistentStorage::PersistentStorage(std::string txtStorageDirPath)
{
   m_pPriv = std::make_unique<PersistentStorage::PrivateData>();
   m_pPriv->m_txtStorageDirPath = txtStorageDirPath;
   m_pPriv->ReadFiles();
}

// ----------------------------------------------------------------------------

PersistentStorage::~PersistentStorage()
{
}

// ----------------------------------------------------------------------------

bool PersistentStorage::WriteFile()
{
   return m_pPriv->WriteFile();
}

// ----------------------------------------------------------------------------

bool PersistentStorage::ChecksumProblemDetected()
{
   return m_pPriv->m_bChecksumProblem;
}
// ----------------------------------------------------------------------------

int  PersistentStorage::GetVolume()
{
   return m_pPriv->m_nVolume;
}

// ----------------------------------------------------------------------------

void PersistentStorage::SetVolume(int nVolume)
{
   m_pPriv->m_nVolume       = nVolume;
   m_pPriv->m_bDataModified = true;
}

// ----------------------------------------------------------------------------

std::string PersistentStorage::GetPlaybackPath()
{
   return m_pPriv->m_txtPlaybackPath;
}

// ----------------------------------------------------------------------------

double      PersistentStorage::GetPlaybackTime()  // seconds
{
   return m_pPriv->m_dPlaybackPos;
}

// ----------------------------------------------------------------------------

void  PersistentStorage::SetPlayback(std::string txtPath, double dTime)
{
   m_pPriv->m_txtPlaybackPath = txtPath;
   m_pPriv->m_dPlaybackPos    = dTime;   // seconds
   m_pPriv->m_bDataModified   = true;
}
