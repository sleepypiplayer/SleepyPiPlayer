// ----------------------------------------------------------------------------
// SleepyPiPlayer:  NumberToSound
//     convert an integer-number to a sequence of MP3-sound-files
// license: free software   (sleepypiplayer(at)saftfresse.de)  [A.D.2025]
// - Do whatever you want with the software.
// - Do not claim my work as your own.
// - THIS SOFTWARE COMES WITH NO WARRANTIES, USE AT YOUR OWN RISK
// ----------------------------------------------------------------------------
#include "NumberToSound.h"
#include "SystemSoundCatalog.h"
#include "SystemConfigFile.h"


// ============================================================================

class NumberToSound_base
{
public:
   NumberToSound_base() {};
   virtual ~NumberToSound_base() {};

   virtual void  AddNumber_0_20( std::list<std::string>& listPath, int nNumber);
   virtual void  AddNumber_0_99( std::list<std::string>& listPath, int nNumber);
   virtual void  AddNumber_21_99( std::list<std::string>& listPath, int nNumber) = 0;
   virtual void  AddNumber_0_999(std::list<std::string>& listPath, int nNumber);
   virtual void  AddNumber_0_999999(std::list<std::string>& listPath, int nNumber);

   SystemSoundCatalog catalog;
};

// ============================================================================

/** convert a number to sound-files in german-style */
//   example:  27  "sieben und zwanzig"  instead of "twenty seven"
class NumberToSound_german : public NumberToSound_base
{
public:
   NumberToSound_german() {};
   virtual ~NumberToSound_german() {};

   void AddNumber_21_99( std::list<std::string>& listPath, int nNumber) override;
};

// ----------------------------------------------------------------------------

// numbers in range 21..99    e.g. 27: "sieben" + "und" + "zwanzig"
void  NumberToSound_german::AddNumber_21_99(
   std::list<std::string>& listPath, int nNumber)
{
   int nLowestDigit  = nNumber % 10;
   int nHighestDigit = nNumber / 10;
   if (nLowestDigit > 0)
   {
      if (nLowestDigit == 1)
      {
         listPath.push_back(catalog.Number_00x1());  // "ein" instead of "eins"
      }
      else
      {
         AddNumber_0_20(listPath, nLowestDigit);
      }
      listPath.push_back(catalog.Text_And());
   }
   switch (nHighestDigit)
   {
   case 2:  listPath.push_back(catalog.Number_0020()); break;
   case 3:  listPath.push_back(catalog.Number_0030()); break;
   case 4:  listPath.push_back(catalog.Number_0040()); break;
   case 5:  listPath.push_back(catalog.Number_0050()); break;
   case 6:  listPath.push_back(catalog.Number_0060()); break;
   case 7:  listPath.push_back(catalog.Number_0070()); break;
   case 8:  listPath.push_back(catalog.Number_0080()); break;
   case 9:  listPath.push_back(catalog.Number_0090()); break;
   default: break;
   }
}

// ============================================================================

/** convert a number to sound-files in english-style */
//   example:  27  "twenty seven"  instead of "sieben und zwanzig"
class NumberToSound_english : public NumberToSound_base
{
public:
   NumberToSound_english() {};
   virtual ~NumberToSound_english() {};

   void  AddNumber_21_99( std::list<std::string>& listPath, int nNumber) override;
};

// ----------------------------------------------------------------------------

// numbers in range 21..99    e.g. 27: "twenty" + "seven"
void  NumberToSound_english::AddNumber_21_99(
   std::list<std::string>& listPath, int nNumber)
{
   int nLowestDigit  = nNumber % 10;
   int nHighestDigit = nNumber / 10;

   switch (nHighestDigit)
   {
   case 2:  listPath.push_back(catalog.Number_0020()); break;
   case 3:  listPath.push_back(catalog.Number_0030()); break;
   case 4:  listPath.push_back(catalog.Number_0040()); break;
   case 5:  listPath.push_back(catalog.Number_0050()); break;
   case 6:  listPath.push_back(catalog.Number_0060()); break;
   case 7:  listPath.push_back(catalog.Number_0070()); break;
   case 8:  listPath.push_back(catalog.Number_0080()); break;
   case 9:  listPath.push_back(catalog.Number_0090()); break;
   default: break;
   }

   if (nLowestDigit > 0)
   {
      AddNumber_0_20(listPath, nLowestDigit);
   }
}

// ============================================================================

void  NumberToSound_base::AddNumber_0_20(
   std::list<std::string>& listPath, int nNumber)
{
   switch (nNumber)
   {
      case  0:  listPath.push_back(catalog.Number_0000());   break;
      case  1:  listPath.push_back(catalog.Number_0001());   break;
      case  2:  listPath.push_back(catalog.Number_0002());   break;
      case  3:  listPath.push_back(catalog.Number_0003());   break;
      case  4:  listPath.push_back(catalog.Number_0004());   break;
      case  5:  listPath.push_back(catalog.Number_0005());   break;
      case  6:  listPath.push_back(catalog.Number_0006());   break;
      case  7:  listPath.push_back(catalog.Number_0007());   break;
      case  8:  listPath.push_back(catalog.Number_0008());   break;
      case  9:  listPath.push_back(catalog.Number_0009());   break;
      case 10:  listPath.push_back(catalog.Number_0010());   break;
      case 11:  listPath.push_back(catalog.Number_0011());   break;
      case 12:  listPath.push_back(catalog.Number_0012());   break;
      case 13:  listPath.push_back(catalog.Number_0013());   break;
      case 14:  listPath.push_back(catalog.Number_0014());   break;
      case 15:  listPath.push_back(catalog.Number_0015());   break;
      case 16:  listPath.push_back(catalog.Number_0016());   break;
      case 17:  listPath.push_back(catalog.Number_0017());   break;
      case 18:  listPath.push_back(catalog.Number_0018());   break;
      case 19:  listPath.push_back(catalog.Number_0019());   break;
      case 20:  listPath.push_back(catalog.Number_0020());   break;
      default: break;
   }
}

// ----------------------------------------------------------------------------

void  NumberToSound_base::AddNumber_0_99(
   std::list<std::string>& listPath, int nNumber)
{
   if (nNumber < 0 || nNumber >= 100)
   {
      // do nothing
   }
   else if (nNumber >= 0 && nNumber <= 20)
   {
      AddNumber_0_20(listPath, nNumber);
   }
   else
   {
      AddNumber_21_99(listPath, nNumber);
   }
}
// ----------------------------------------------------------------------------

void  NumberToSound_base::AddNumber_0_999(
   std::list<std::string>& listPath, int nNumber)
{
   if (nNumber < 0  || nNumber >= 1000)
   {
      // do nothing
   }
   else if (nNumber <= 20)
   {
      AddNumber_0_20(listPath, nNumber);
   }
   else
   {
      int nHighestDigit = nNumber / 100;
      int nLowestDigits = nNumber % 100;
      switch (nHighestDigit)
      {
      case 1:  listPath.push_back(catalog.Number_0100()); break;
      case 2:  listPath.push_back(catalog.Number_0200()); break;
      case 3:  listPath.push_back(catalog.Number_0300()); break;
      case 4:  listPath.push_back(catalog.Number_0400()); break;
      case 5:  listPath.push_back(catalog.Number_0500()); break;
      case 6:  listPath.push_back(catalog.Number_0600()); break;
      case 7:  listPath.push_back(catalog.Number_0700()); break;
      case 8:  listPath.push_back(catalog.Number_0800()); break;
      case 9:  listPath.push_back(catalog.Number_0900()); break;
      default: break;
      }

      if (nLowestDigits > 0)
      {
         AddNumber_0_99(listPath, nLowestDigits);
      }
   }
}

// ----------------------------------------------------------------------------

void  NumberToSound_base::AddNumber_0_999999(
   std::list<std::string>& listPath, int nNumber)
{
   if (nNumber < 0 || nNumber >= 1000000)
   {
      // do nothing
   }
   else if (nNumber <= 20)
   {
      AddNumber_0_20(listPath, nNumber);
   }
   else
   {
      int nHighestDigits = nNumber / 1000;
      int nLowestDigits  = nNumber % 1000;

      if (nHighestDigits == 1)
      {
         listPath.push_back(catalog.Number_1000());
      }
      else if (nHighestDigits > 1)
      {
         AddNumber_0_999(listPath, nHighestDigits);
         listPath.push_back(catalog.Number_x000());
      }
      if (nLowestDigits > 0)
      {
         AddNumber_0_999(listPath, nLowestDigits);
      }
   }
}

// ============================================================================

class NumberToSound::PrivateData
{
public:
   PrivateData() {}

   NumberToSound_english  m_English;
   NumberToSound_german   m_German;
   SystemSoundCatalog     m_Catalog;
};


// ============================================================================

NumberToSound::NumberToSound()
{
   m_pPriv = std::make_unique<NumberToSound::PrivateData>();
}

// ----------------------------------------------------------------------------

NumberToSound::~NumberToSound()
{
}

// ----------------------------------------------------------------------------

void NumberToSound::AddNumber(std::list<std::string>& listPath, int nNumber)
{
   if (nNumber < 0 || nNumber >= 1000000)
   {
      listPath.push_back(m_pPriv->m_Catalog.Text_Invalid());
   }
   else
   {
      if (SystemConfigFile::GetNumberFormat() == SystemConfigFile::NumberFormat::GERMAN)
      {
         m_pPriv->m_German.AddNumber_0_999999(listPath, nNumber);
      }
      else
      {
         m_pPriv->m_English.AddNumber_0_999999(listPath, nNumber);
      }
   }
}
