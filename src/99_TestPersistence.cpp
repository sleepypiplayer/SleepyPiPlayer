

// test class PersistentStorage for bugs

#include "PersistentStorage.h"
#include <list>
#include <vector>
#include <ctime>

class SavedContent
{
public:
   SavedContent()
   {
      std::srand(std::time({}));
      CreateRandomContent();
   }
   
   void CreateRandomContent();
   
   void UpdatePersistence(PersistentStorage& storage);
   bool IsContentEqual(PersistentStorage& storage);
   
   
   std::string GetPath() { return m_txtPathCurrent; }
   
   static  int GetRandomNum(int nMax);
   
private:

   std::string GetRandomStr(int nMaxLen);
   std::string   m_txtPathCurrent;
   double        m_dTimeCurrent = 0.0;
   int           m_nVolume      = 50;
   
   std::vector<std::string> m_listTxtPathAdd;
   std::vector<int>         m_listNumTimeAdd;
};


// -------------------------------------------------------

int SavedContent::GetRandomNum(int nMax)  // result: 0...nMax
{     
   int nNumRandom = 0;
   
   if (nMax > 0)
   {
      nNumRandom = std::rand();
      if (nNumRandom > nMax)
      {
         while(nNumRandom > nMax)
         {
            nNumRandom /= 2;
         }
      }
      else
      {  
         int nNumRandom2 = std::rand(); 
         if (nNumRandom2 %4 == 0)
         {
            while((double)nNumRandom+nNumRandom2 < nMax)
            {
               nNumRandom += nNumRandom2;
            }
         }
      }
   }
   if (nNumRandom < 0)
   {
      printf("negative random\n");
      nNumRandom = 0;
   }
    
   return nNumRandom;
}

// -------------------------------------------------------

std::string SavedContent::GetRandomStr(int nMaxLen)
{
   std::string txtResult;
   int nLength = GetRandomNum(nMaxLen);
   for (int i = 0; i < nLength; i++)
   {
      int nChar = 'A' + GetRandomNum(28);
      if (nChar > 'Z' && (i+1)<nLength && i > 0)
      {
         if (nChar == ('Z'+1))
         {
            nChar = ' ';
         }
         else
         {
            nChar = '/';
         }
      }
      else if (nChar > 'Z')
      {
         nChar = 'A' + GetRandomNum(20);
      }
      txtResult.push_back((char)nChar);
   }
   return txtResult;
}

// -------------------------------------------------------

void SavedContent::CreateRandomContent()
{

   m_nVolume        = GetRandomNum(50) + 30;
   m_dTimeCurrent   = GetRandomNum(99999999);
   m_txtPathCurrent = GetRandomStr(490);
   int nNofAdd = GetRandomNum(99);

   m_listTxtPathAdd.clear();
   m_listNumTimeAdd.clear();
   for (int i = 0; i < nNofAdd; i++)
   {
      m_listTxtPathAdd.push_back(GetRandomStr(490));
      m_listNumTimeAdd.push_back(GetRandomNum(99999999));
   }
}

// -------------------------------------------------------

void SavedContent::UpdatePersistence(PersistentStorage& storage)
{
   storage.SetVolume(m_nVolume);
   storage.SetPlayback(m_txtPathCurrent, m_dTimeCurrent);
   storage.ClearAdditionalPlaybackPos();
   int nNofAdd = m_listTxtPathAdd.size();
   for (int i = 0; i < nNofAdd; i++)
   {
      storage.AddAdditionalPlaybackPos(m_listTxtPathAdd[i], m_listNumTimeAdd[i]);
   }
}

// -------------------------------------------------------

bool SavedContent::IsContentEqual(PersistentStorage& storage)
{
   bool bEqual = true;

   if (!(m_txtPathCurrent == storage.GetPlaybackPath()))
      bEqual = false;
   if (std::abs(m_dTimeCurrent - storage.GetPlaybackTime()) > 0.01)
      bEqual = false;
   if (std::abs(m_nVolume - storage.GetVolume()) > 0.01)
      bEqual = false;
   
   int nNofAdd = m_listTxtPathAdd.size();
   if (nNofAdd != storage.GetNofAdditionalPlaybackPos())
      bEqual = false;
      
   if (bEqual)
   {
      for (int i = 0; i < nNofAdd; i++)
      {
         if (!(m_listTxtPathAdd[i] == storage.GetAdditionalPlaybackPath(i)))
            bEqual = false;
         if (std::abs(m_listNumTimeAdd[i] - storage.GetAdditionalPlaybackTime(i)) > 0.01)
            bEqual = false;
      }
   }
   
   return bEqual;
}


// -------------------------------------------------------
// -------------------------------------------------------

void RandomSwapFiles(std::string txtPersistDir)
{
   // e.g.  062PersistStorage.bin   <->   026PersistStorage.bin
   // should also work if the file-sequence is slightly distrurbed
   
   char txtBuffer[1000] = {};
   
   sprintf(txtBuffer, "%s/tempName.bin", txtPersistDir.c_str());
   std::string txtNameTemp(txtBuffer);
   
   int nFileNr1 = SavedContent::GetRandomNum(99);
   sprintf(txtBuffer, "%s/%03iPersistStorage.bin", txtPersistDir.c_str(),nFileNr1);
   std::string txtName1(txtBuffer);
   
   int nFileNr2 = SavedContent::GetRandomNum(99);
   sprintf(txtBuffer, "%s/%03iPersistStorage.bin", txtPersistDir.c_str(),nFileNr2);
   std::string txtName2(txtBuffer);
   
   if (nFileNr1 != nFileNr2)
   {
      //printf("%s %s %s\n",txtName1.c_str(),txtName2.c_str(),txtNameTemp.c_str());
      std::rename( txtName1.c_str(),    txtNameTemp.c_str());
      std::rename( txtName2.c_str(),    txtName1.c_str());
      std::rename( txtNameTemp.c_str(), txtName2.c_str());      
   }
}

// -------------------------------------------------------
// -------------------------------------------------------


int main()
{
   SavedContent content;

   std::string txtPersistDir("/delete_me_sleepy_persist");   

   // -- check if equality-check works --
   {
      PersistentStorage storage(txtPersistDir);
      if (content.IsContentEqual(storage))
      {
         printf("##== unlikely equal to random\n");
      }
      content.UpdatePersistence(storage);
      storage.WriteFile();
   }
   
   // -- the real test --
   for (int i = 0; i < 100000; i++)
   {
      PersistentStorage storage(txtPersistDir);
      bool bContentEqual = content.IsContentEqual(storage);
      if (!bContentEqual)
      {
         printf("##== problem Detected:%i\n",i);
      }
      content.CreateRandomContent();
      content.UpdatePersistence(storage);
      storage.WriteFile();
      if (i %10 == 0)
      {
         RandomSwapFiles(txtPersistDir);
      }
   }  
}
