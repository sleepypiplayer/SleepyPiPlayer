// ----------------------------------------------------------------------------
// SleepyPiPlayer:  SystemSoundCatalog
//     provide path to mp3-files used to generate Audio-Feedback
// ----------------------------------------------------------------------------
#pragma once
#include <string>
#include <memory>
#include <list>

class SystemSoundCatalog
{
public:
   SystemSoundCatalog();
   virtual ~SystemSoundCatalog();

   // -----------------------------------------
   // all methods return the path to a MP3-file
   // -----------------------------------------

   std::string Silence();         // 0.74 seconds of silence
   std::string Text_Volume();
   std::string Text_File();
   std::string Text_Directory();
   std::string Text_Minute();
   std::string Text_Service();
   std::string Text_Of();
   std::string Text_Shutdown();     // "a key press prevents from shutdown"
   std::string Text_ShuttingDown(); // "shutting down" / "ich schalte aus"
   std::string Text_ShutdownIn();   // "shutdown in" / "Abschaltung in" ...
   std::string Text_Minutes();      // ... "minutes" / "Minuten"
   std::string Text_ChecksumProblem();
   std::string Text_Mp3FileProblem();
   std::string Text_NoMP3Found();
   std::string Text_And();    // "und"  von "ein-und-zwanzig"
   std::string Text_Invalid();// "ungültig"  (number is negative or too big)

   std::string GetChar(char txtCharacter);  // '1': eins / "one"
   void SpellString(std::list<std::string>& listOutput, std::string txtInput);

   std::string Number_0000();    // "null"   "zero"  -- less silence than GetChar('0')
   std::string Number_00x1();    // "ein" von "ein-und-zwanzig"
   std::string Number_0001();    // "eins"   "one"
   std::string Number_0002();    // "zwei"   "two"
   std::string Number_0003();    // "drei"   "three"
   std::string Number_0004();
   std::string Number_0005();
   std::string Number_0006();
   std::string Number_0007();
   std::string Number_0008();
   std::string Number_0009();
   std::string Number_0010();
   std::string Number_0011();
   std::string Number_0012();
   std::string Number_0013();
   std::string Number_0014();
   std::string Number_0015();
   std::string Number_0016();
   std::string Number_0017();
   std::string Number_0018();
   std::string Number_0019();
   std::string Number_0020();
   std::string Number_0030();
   std::string Number_0040();
   std::string Number_0050();
   std::string Number_0060();
   std::string Number_0070();
   std::string Number_0080();
   std::string Number_0090();
   std::string Number_0100();
   std::string Number_0200();
   std::string Number_0300();
   std::string Number_0400();
   std::string Number_0500();
   std::string Number_0600();
   std::string Number_0700();
   std::string Number_0800();
   std::string Number_0900();
   std::string Number_1000();   // "eintausend"  "one-thousand"
   std::string Number_x000();   // "tausend"     "thousand"      // part of "fifty-thousand"


private:
   class PrivateData;
   std::unique_ptr<PrivateData> m_pPriv;
};
