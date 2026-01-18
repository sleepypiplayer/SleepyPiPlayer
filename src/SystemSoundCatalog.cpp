// ----------------------------------------------------------------------------
// SleepyPiPlayer:  SystemSoundCatalog
//     provide path to mp3-files used to generate Audio-Feedback
// license: free software   (sleepypiplayer(at)saftfresse.de)  [A.D.2025]
// - Do whatever you want with the software.
// - Do not claim my work as your own.
// - THIS SOFTWARE COMES WITH NO WARRANTIES, USE AT YOUR OWN RISK
// ----------------------------------------------------------------------------
#include "SystemSoundCatalog.h"
#include "SystemConfigFile.h"
#include <filesystem>


// ============================================================================
class SystemSoundCatalog::PrivateData
{
public:
   PrivateData();

   std::string GetDirectory()
   {
      if (m_txtMp3Directory.empty())
      {
         if (SystemConfigFile::GetSystemSoundPath().length() > 0)
         {
            m_txtMp3Directory = SystemConfigFile::GetSystemSoundPath();
            m_txtMp3Directory += SEPARATOR;
         }
      }
      return m_txtMp3Directory;
   }

   std::string m_txtMp3Directory;
   const char* m_CharFile[256];
   const char  SEPARATOR = std::filesystem::path::preferred_separator;
};

// ----------------------------------------------------------------------------

SystemSoundCatalog::PrivateData::PrivateData()
{
   for (int i = 0; i < 256; i++)
   {
      switch ((char)i)
      {
      case 'a':
      case 'A':
         m_CharFile[i] = "sys_char_a.mp3";  break;
      case 'b':
      case 'B':
         m_CharFile[i] = "sys_char_b.mp3";  break;
      case 'c':
      case 'C':
         m_CharFile[i] = "sys_char_c.mp3";  break;
      case 'd':
      case 'D':
         m_CharFile[i] = "sys_char_d.mp3";  break;
      case 'e':
      case 'E':
         m_CharFile[i] = "sys_char_e.mp3";  break;
      case 'f':
      case 'F':
         m_CharFile[i] = "sys_char_f.mp3";  break;
      case 'g':
      case 'G':
         m_CharFile[i] = "sys_char_g.mp3";  break;
      case 'h':
      case 'H':
         m_CharFile[i] = "sys_char_h.mp3";  break;
      case 'i':
      case 'I':
         m_CharFile[i] = "sys_char_i.mp3";  break;
      case 'j':
      case 'J':
         m_CharFile[i] = "sys_char_j.mp3";  break;
      case 'k':
      case 'K':
         m_CharFile[i] = "sys_char_k.mp3";  break;
      case 'l':
      case 'L':
         m_CharFile[i] = "sys_char_l.mp3";  break;
      case 'm':
      case 'M':
         m_CharFile[i] = "sys_char_m.mp3";  break;
      case 'n':
      case 'N':
         m_CharFile[i] = "sys_char_n.mp3";  break;
      case 'o':
      case 'O':
         m_CharFile[i] = "sys_char_o.mp3";  break;
      case 'p':
      case 'P':
         m_CharFile[i] = "sys_char_p.mp3";  break;
      case 'q':
      case 'Q':
         m_CharFile[i] = "sys_char_q.mp3";  break;
      case 'r':
      case 'R':
         m_CharFile[i] = "sys_char_r.mp3";  break;
      case 's':
      case 'S':
         m_CharFile[i] = "sys_char_s.mp3";  break;
      case 't':
      case 'T':
         m_CharFile[i] = "sys_char_t.mp3";  break;
      case 'u':
      case 'U':
         m_CharFile[i] = "sys_char_u.mp3";  break;
      case 'v':
      case 'V':
         m_CharFile[i] = "sys_char_v.mp3";  break;
      case 'w':
      case 'W':
         m_CharFile[i] = "sys_char_w.mp3";  break;
      case 'x':
      case 'X':
         m_CharFile[i] = "sys_char_x.mp3";  break;
      case 'y':
      case 'Y':
         m_CharFile[i] = "sys_char_y.mp3";  break;
      case 'z':
      case 'Z':
         m_CharFile[i] = "sys_char_z.mp3";  break;

      case '0': m_CharFile[i] = "sys_digit_0.mp3";  break;
      case '1': m_CharFile[i] = "sys_digit_1.mp3";  break;
      case '2': m_CharFile[i] = "sys_digit_2.mp3";  break;
      case '3': m_CharFile[i] = "sys_digit_3.mp3";  break;
      case '4': m_CharFile[i] = "sys_digit_4.mp3";  break;
      case '5': m_CharFile[i] = "sys_digit_5.mp3";  break;
      case '6': m_CharFile[i] = "sys_digit_6.mp3";  break;
      case '7': m_CharFile[i] = "sys_digit_7.mp3";  break;
      case '8': m_CharFile[i] = "sys_digit_8.mp3";  break;
      case '9': m_CharFile[i] = "sys_digit_9.mp3";  break;

      case '/':
      case '\\':
         m_CharFile[i] = "sys_char__slash.mp3";  break;
      case '-':
      case '_':
         m_CharFile[i] = "sys_char__dash.mp3";  break;
      case ' ':
         m_CharFile[i] = "sys_char__space.mp3";  break;
      case '.':
         m_CharFile[i] = "sys_char__dot.mp3";  break;

      default:
         m_CharFile[i] = "sys_char_special.mp3";
         break;
      }
   }
}


// ============================================================================

SystemSoundCatalog::SystemSoundCatalog()
{
   m_pPriv = std::make_unique<SystemSoundCatalog::PrivateData>();

   std::string txtFilePath = Text_File();
   const std::filesystem::path pathFile{txtFilePath};
   if (! std::filesystem::exists(pathFile))
   {
      printf("\n\n==========================================\n");
      printf("SystemSoundFiles not found\n");
      printf("%s\n", txtFilePath.c_str());
      printf("==========================================\n");
   }
}

// ----------------------------------------------------------------------------

SystemSoundCatalog::~SystemSoundCatalog()
{
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Silence()
{
   return m_pPriv->GetDirectory() + "sys_silence.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Text_Volume()
{
   return m_pPriv->GetDirectory() + "sys_text_volume.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Text_File()
{
   return m_pPriv->GetDirectory() + "sys_text_file.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Text_Directory()
{
   return m_pPriv->GetDirectory() + "sys_text_directory.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Text_Service()
{
   return m_pPriv->GetDirectory() + "sys_text_service.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Text_Of()
{
   return m_pPriv->GetDirectory() + "sys_text_of.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Text_And()    // "und"  von "ein-und-zwanzig"
{
   return m_pPriv->GetDirectory() + "sys_text_and.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Text_Invalid()// "ungültig"  (negative or too big)
{
   return m_pPriv->GetDirectory() + "sys_text_invalid.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Text_Shutdown()
{
   return m_pPriv->GetDirectory() + "sys_text_shutdown.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Text_ShuttingDown()
{
   return m_pPriv->GetDirectory() + "sys_text_shutting_down.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Text_ShutdownIn()   // "shutdown in" / "Abschaltung in" ...
{
   return m_pPriv->GetDirectory() + "sys_text_shutdown_in.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Text_Minutes()      // ... "minutes" / "Minuten"
{
   return m_pPriv->GetDirectory() + "sys_text_minutes.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Text_ChecksumProblem()
{
   return m_pPriv->GetDirectory() + "sys_text_checksum.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Text_Mp3FileProblem()
{
   return m_pPriv->GetDirectory() + "sys_text_mp3_problem.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Text_NoMP3Found()
{
   return m_pPriv->GetDirectory() + "sys_text_no_mp3.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Text_Minute()
{
   return m_pPriv->GetDirectory() + "sys_text_minute.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::GetChar(char txtCharacter)
{
   return m_pPriv->GetDirectory() + m_pPriv->m_CharFile[(unsigned char)(txtCharacter)];
}

// ----------------------------------------------------------------------------

void SystemSoundCatalog::SpellString(std::list<std::string>& listOutput, std::string txtInput)
{
   int nLength = txtInput.length();
   if (nLength <= 32)
   {
      for (int i = 0; i < nLength; i++)
      {
         listOutput.push_back(GetChar(txtInput.at(i)));
      }
   }
   else
   {
      for (int i = 0; i < 12 && i < nLength; i++)
      {
         listOutput.push_back(GetChar(txtInput.at(i)));
      }
      listOutput.push_back(GetChar('.'));
      listOutput.push_back(GetChar('.'));
      listOutput.push_back(GetChar('.'));
      for (int i = nLength-19; i < nLength; i++)
      {
         listOutput.push_back(GetChar(txtInput.at(i)));
      }
   }
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0000()      // "null"
{
   return m_pPriv->GetDirectory() + "sys_num_0000.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0001()      // "eins" / "one"
{
   return m_pPriv->GetDirectory() + "sys_num_0001.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_00x1()      // "ein" von "ein-und-zwanzig"
{
   return m_pPriv->GetDirectory() + "sys_num_00x1.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0002()      // "zwei" / "two"
{
   return m_pPriv->GetDirectory() + "sys_num_0002.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0003()      // "drei" / "three"
{
   return m_pPriv->GetDirectory() + "sys_num_0003.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0004()
{
   return m_pPriv->GetDirectory() + "sys_num_0004.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0005()
{
   return m_pPriv->GetDirectory() + "sys_num_0005.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0006()
{
   return m_pPriv->GetDirectory() + "sys_num_0006.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0007()
{
   return m_pPriv->GetDirectory() + "sys_num_0007.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0008()
{
   return m_pPriv->GetDirectory() + "sys_num_0008.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0009()
{
   return m_pPriv->GetDirectory() + "sys_num_0009.mp3";
}


// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0010()     // "zehn"
{
   return m_pPriv->GetDirectory() + "sys_num_0010.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0011()
{
   return m_pPriv->GetDirectory() + "sys_num_0011.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0012()
{
   return m_pPriv->GetDirectory() + "sys_num_0012.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0013()
{
   return m_pPriv->GetDirectory() + "sys_num_0013.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0014()
{
   return m_pPriv->GetDirectory() + "sys_num_0014.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0015()
{
   return m_pPriv->GetDirectory() + "sys_num_0015.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0016()
{
   return m_pPriv->GetDirectory() + "sys_num_0016.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0017()
{
   return m_pPriv->GetDirectory() + "sys_num_0017.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0018()
{
   return m_pPriv->GetDirectory() + "sys_num_0018.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0019()
{
   return m_pPriv->GetDirectory() + "sys_num_0019.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0020()     // "zwanzig"  "twenty"
{
   return m_pPriv->GetDirectory() + "sys_num_0020.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0030()
{
   return m_pPriv->GetDirectory() + "sys_num_0030.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0040()
{
   return m_pPriv->GetDirectory() + "sys_num_0040.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0050()
{
   return m_pPriv->GetDirectory() + "sys_num_0050.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0060()
{
   return m_pPriv->GetDirectory() + "sys_num_0060.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0070()
{
   return m_pPriv->GetDirectory() + "sys_num_0070.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0080()
{
   return m_pPriv->GetDirectory() + "sys_num_0080.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0090()
{
   return m_pPriv->GetDirectory() + "sys_num_0090.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0100()
{
   return m_pPriv->GetDirectory() + "sys_num_0100.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0200()
{
   return m_pPriv->GetDirectory() + "sys_num_0200.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0300()
{
   return m_pPriv->GetDirectory() + "sys_num_0300.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0400()
{
   return m_pPriv->GetDirectory() + "sys_num_0400.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0500()
{
   return m_pPriv->GetDirectory() + "sys_num_0500.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0600()
{
   return m_pPriv->GetDirectory() + "sys_num_0600.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0700()
{
   return m_pPriv->GetDirectory() + "sys_num_0700.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0800()
{
   return m_pPriv->GetDirectory() + "sys_num_0800.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_0900()
{
   return m_pPriv->GetDirectory() + "sys_num_0900.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_1000()  // "eintausend"  "one-thousand"
{
   return m_pPriv->GetDirectory() + "sys_num_1000.mp3";
}

// ----------------------------------------------------------------------------

std::string SystemSoundCatalog::Number_x000()  // "tausend"     "thousand"
{
   return m_pPriv->GetDirectory() + "sys_num_x000.mp3";
}
