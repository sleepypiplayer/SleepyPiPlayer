// ----------------------------------------------------------------------------
// SleepyPiPlayer:  NumberToSound
//     convert an integer-number to a sequence of MP3-sound-files
// ----------------------------------------------------------------------------
#pragma once
#include <list>
#include <string>
#include <memory>

// ----------------------------------------------------------------------------
/** base-class of convertion in different languages */
class NumberToSound
{
public:
   NumberToSound();
   virtual ~NumberToSound();

   // convert numbers in the range of 0 .. 999999 to a sequence of system-sound-files
   // listPath.push_back( system_file_path_to_digit )
   void AddNumber(std::list<std::string>& listPath, int nNumber);

private:
   class PrivateData;
   std::unique_ptr<PrivateData> m_pPriv;
};
