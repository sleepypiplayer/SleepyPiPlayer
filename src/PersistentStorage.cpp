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
#include <vector>


// the standard-data-record is stored three times in order to detect SD-card problems
//  0x000.0x3FF   first  copy of standard-data-record 
//  0x400.0x7FF   second copy of standard-data-record 
//  0x800.0xBFF   third  copy of standard-data-record 

// --- standard-data-record ---
// 0x000..0x003  0000..0003   04_char Storage-Number
// 0x00A..0x00D  0010..0012   03_char Volume
// 0x014..0x02D  0020..0045   25_char time-pos in seconds float "12345678.123456"
// 0x032..0x2BB  0050..0699  650_char file-path
// 0x2C0..0x2C3  0704..0707    4_char number of additional-records (playback-pos of other directories)
// 0x3E8..0x3EF  1000..1007    8_char CheckSum [0x000..0x3E7]


// 0xC00...  after the three copies of standard-data-records
// some extra playback positions of other directories can be stored
// --- additional directory playback data-record ---
// 0x000..0x1EF              496_char file-path followed by spaces
// 0x1F0                       1_char space
// 0x1F1                       1_char '\n'
// 0x1F2..0x1F9                8_char time-pos in seconds (integer "12345678")
// 0x1FA                       1_char '\n'
// 0x1FB..0x1FF                5_char spaces

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

   int         m_nFileRecordIndex = -1; // identifies the source-file of read data
   std::string m_txtPlaybackPath;       // path to last played mp3
   double      m_dPlaybackPos = 0.0;    // seconds
   int         m_nVolume = 70;          // 0 .. 100%

   bool        m_bChecksumProblem = false;

   // first: loop over all files and read only first 3kB of each file - later read the additional pos of only one persistance-file
   int                       m_nNofAdditionalPosInFile = 0;  // read from the first 3kB
   std::vector<std::string>  m_listAdditionalPath;  // file-path: playback-pos of other directories
   std::vector<int>          m_listAdditionalTime;  // seconds:   playback-pos of other directories

   static const int  RECORD_SIZE   = 4096*1024;    // 4MB per file
   static const int  DATA_SIZE     = 1024;         // size of one standard-data-record (3 copies in one file)
   static const int  NOF_RECORDS   = 100;          // 100 files
   static const int  ADD_DATA_SIZE = 512;          // size of one additional playback-position data-record
   static const int  MAX_NOF_ADDITIONAL = 100;     // maximal number of stored playback-positions in other directories

   // e.g. m_arrStorageNumber[55] contains storage number of 055PersistStorage.bin   (1..9999)
   int m_arrStorageNumber[NOF_RECORDS];     // storage-number found in storage-files   -1: not allocated

   std::string GetStorageFilePath(int nRecordIdx);
   void InvalidateData();
   bool WriteFile();
   void ReadFiles();
   void ReadAdditionalPlaybackPos();
   void EnsureFilesExist();

   int  CalcFreeRecordIdx();
   void PrepareData(unsigned char* buffer);
   void DecodeData(int nRecordIdx, unsigned char* buffer);
   int  CalcChecksum(const unsigned char* buffer);
   void PrepareAdditionalPos(unsigned char* buffer, int iAddIdx);
   void DecodeAdditionalPos(unsigned char* buffer);
   int  GetNofAdditionalPlaybackPos();
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
      char bufferText[652] = {};
      bool bStarted = false;
      for (int i =649; i >= 0; i--)
      {
         bStarted = bStarted || buffer[i+50] != ' ';
         if (bStarted)
         {
            bufferText[i] = buffer[i+50];
         }
      }
      txtPath = bufferText;
   }

   // --- number of additional positions in other directories ---
   int nNofAdditionalPos = 0;
   try
   {
      char bufferNumber[5] = {};
      for (int i = 0; i < 4; i++)
         bufferNumber[i] = buffer[i+704];
      int nMaxNofAdditional = MAX_NOF_ADDITIONAL;
      nNofAdditionalPos = std::stoi(bufferNumber);
      nNofAdditionalPos = std::max(nNofAdditionalPos, 0);
      nNofAdditionalPos = std::min(nNofAdditionalPos, nMaxNofAdditional);
   }
   catch(...)
   {
      nNofAdditionalPos = 0;
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
         m_nFileRecordIndex        = nRecordIdx;
         m_txtPlaybackPath         = txtPath;
         m_dPlaybackPos            = dPosition;
         m_nVolume                 = nVolume;
         m_nNofAdditionalPosInFile = nNofAdditionalPos;
      }
   }
}

// ----------------------------------------------------------------------------
void PersistentStorage::PrivateData::DecodeAdditionalPos(unsigned char* buffer)
{
   // --- path ---
   char bufferText[500] = {};
   bool bStarted = false;
   for (int i = 495; i >= 0; i--)
   {
      bStarted = bStarted || buffer[i] != ' ';
      if (bStarted)
      {
         bufferText[i] = buffer[i];
      }
   }
   if (bufferText[0])
   {
      m_listAdditionalPath.push_back( bufferText );

      // --- time ---
      int nAdditionalTime = 0;
      try
      {
         char bufferNumber[10] = {};
         for (int i = 0; i < 8; i++)
            bufferNumber[i] = buffer[i+0x1F2];
         nAdditionalTime = std::stoi(bufferNumber);
      }
      catch(...)
      {
         nAdditionalTime = 0;
      }
      m_listAdditionalTime.push_back( nAdditionalTime );
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
   if (nLengthPath > 0 && nLengthPath < 650)
   {
      for (int i = 0 ; i < nLengthPath; i++)
      {
         buffer[i+50] = m_txtPlaybackPath.at(i);
      }
   }
   // -- number of additional playback positions --
   sprintf(&txtTemp[50], "%04i", GetNofAdditionalPlaybackPos());
   for (int i = 50; i < 54; i++)
   {
      buffer[i+654] = txtTemp[i]; // 704..707
   } 
   // --- replace null-bytes by spaces ---
   for (int i = 0; i < 1000; i++)
   {
      buffer[i] = (buffer[i] != 0)? buffer[i] : ' ';
   }
   // --- checksum ---
   int ChkSum = CalcChecksum(buffer);
   sprintf(&txtTemp[60], "%08i", ChkSum);
   for (int i = 60; i < 68; i++)
   {
      buffer[i+940] = txtTemp[i]; // 1000..1007
   }
}

// ----------------------------------------------------------------------------
// prepare additional directory playback data-record
void PersistentStorage::PrivateData::PrepareAdditionalPos(unsigned char* buffer, int iAddIdx)
{
   for (int i = 0; i < ADD_DATA_SIZE; i++) buffer[i] = ' ';
   if (iAddIdx >= 0 && iAddIdx < GetNofAdditionalPlaybackPos())
   {
      int nLengthPath = m_listAdditionalPath[iAddIdx].length();
      if (nLengthPath > 0 && nLengthPath <= 496)
      {
         for (int i = 0 ; i < nLengthPath; i++)
         {
            buffer[i] = m_listAdditionalPath[iAddIdx].at(i);
         }
         int nAddTime = m_listAdditionalTime[iAddIdx];
         nAddTime = std::max(nAddTime, 0);
         nAddTime = std::min(nAddTime, 99999999);
         
         sprintf((char*)(&buffer[0x1F2]), "%08i", nAddTime);
      }
   }
   // --- replace null-bytes by spaces ---
   for (int i = 0; i < ADD_DATA_SIZE; i++)
   {
      buffer[i] = (buffer[i] != 0)? buffer[i] : ' ';
   }
   buffer[0x1F1] = '\n';
   buffer[0x1FA] = '\n';
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

void PersistentStorage::PrivateData::ReadAdditionalPlaybackPos()
{
   if (m_nFileRecordIndex >= 0 && m_nNofAdditionalPosInFile > 0)
   {
      std::string txtFilePath = GetStorageFilePath(m_nFileRecordIndex);
      std::FILE* pFile = std::fopen(txtFilePath.c_str(), "rb");
      if (pFile)
      {
         int nSeekResult = std::fseek(pFile, 3*DATA_SIZE, SEEK_SET);

         if (nSeekResult == 0)
         {
            for (int nAddIdx = 0; nAddIdx < m_nNofAdditionalPosInFile; nAddIdx++)
            {
               unsigned char buffer[ADD_DATA_SIZE] = {};
               if (std::fread(buffer, ADD_DATA_SIZE, 1 , pFile) > 0)
               {
                  DecodeAdditionalPos(buffer);
               }
            }
         }
      }
      fclose (pFile);
   }
   m_nNofAdditionalPosInFile = m_listAdditionalPath.size();
}

// ----------------------------------------------------------------------------
// read all 100 files and search for the one with the maximum storage-number
void PersistentStorage::PrivateData::ReadFiles()
{
   m_bDataModified = false;
   InvalidateData();

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
   ReadAdditionalPlaybackPos();
}

// ----------------------------------------------------------------------------

bool PersistentStorage::PrivateData::WriteFile()
{
   EnsureFilesExist();
   bool bSuccess = true;

   int nFreeRecordIdx = CalcFreeRecordIdx();

   // printf("Persist: record:%i playback %s\n", nFreeRecordIdx, m_txtPlaybackPath.c_str());

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
         // --- three copies of standard-data-record ---
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
         }

         // --- additional playback positions of other directories ---
         {
            int nNofAdditionalPos = GetNofAdditionalPlaybackPos();
            for (int iAddIdx = 0; iAddIdx < nNofAdditionalPos; iAddIdx++)
            {
               unsigned char buffer[ADD_DATA_SIZE] = {};
               PrepareAdditionalPos(buffer, iAddIdx);
               std::fwrite(buffer, ADD_DATA_SIZE, 1, pFile);
            }
            std::fflush(pFile);
         }

         // --- clean up ---
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

// ----------------------------------------------------------------------------

void PersistentStorage::PrivateData::InvalidateData()
{
   m_bDataModified     = false;
   m_bChecksumProblem  = false;
   m_nMaxStorageNumber = -1;
   m_nFileRecordIndex  = -1;
   m_dPlaybackPos      = 0.0;
   m_nVolume           = 70;
   m_txtPlaybackPath.clear();
   m_nNofAdditionalPosInFile = 0;
   m_listAdditionalPath.clear();
   m_listAdditionalTime.clear();
} 

// ----------------------------------------------------------------------------

int PersistentStorage::PrivateData::GetNofAdditionalPlaybackPos()
{
   int nListSizePath = m_listAdditionalPath.size();
   int nListSizeTime = m_listAdditionalTime.size();
   int nNofAddPos = MAX_NOF_ADDITIONAL;
   nNofAddPos = std::min(nNofAddPos, nListSizePath);
   nNofAddPos = std::min(nNofAddPos, nListSizeTime);
   return nNofAddPos;
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

void  PersistentStorage::SetPlayback(const std::string& txtPath, double dTime)
{
   m_pPriv->m_txtPlaybackPath = txtPath;
   m_pPriv->m_dPlaybackPos    = dTime;   // seconds
   m_pPriv->m_bDataModified   = true;
}

// ----------------------------------------------------------------------------

void PersistentStorage::ClearAdditionalPlaybackPos()
{
   m_pPriv->m_listAdditionalPath.clear();
   m_pPriv->m_listAdditionalTime.clear();
   m_pPriv->m_nNofAdditionalPosInFile = 0;
}

// ----------------------------------------------------------------------------

void PersistentStorage::AddAdditionalPlaybackPos(const std::string& txtPath, double dTime)
{
   if (m_pPriv->GetNofAdditionalPlaybackPos() < PrivateData::MAX_NOF_ADDITIONAL)
   {
      int nLengthPath = txtPath.length();
      if (nLengthPath > 0 && nLengthPath < 496)
      {
         m_pPriv->m_listAdditionalPath.push_back(txtPath);
         m_pPriv->m_listAdditionalTime.push_back((int)(dTime));
         m_pPriv->m_bDataModified   = true;
      }
   }
}

// ----------------------------------------------------------------------------

int PersistentStorage::GetNofAdditionalPlaybackPos()
{
   return m_pPriv->GetNofAdditionalPlaybackPos();
}

// ----------------------------------------------------------------------------

std::string PersistentStorage::GetAdditionalPlaybackPath(int nIdx)
{
   std::string txtAddPath;
   if (nIdx >= 0 && nIdx < GetNofAdditionalPlaybackPos())
   {
      txtAddPath = m_pPriv->m_listAdditionalPath[nIdx];
   }
   return txtAddPath;
}

// ----------------------------------------------------------------------------

double PersistentStorage::GetAdditionalPlaybackTime(int nIdx)
{
   double dAddTime;
   if (nIdx >= 0 && nIdx < GetNofAdditionalPlaybackPos())
   {
      dAddTime = m_pPriv->m_listAdditionalTime[nIdx];
   }
   return dAddTime;
}

