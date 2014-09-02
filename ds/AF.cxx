#include "AF.h"

#include "TString.h"
#include "TEnv.h"
#include "AFDynamic.h"
#include "AFStatic.h"
#include "TSystem.h"
#include "Riostream.h"

//__________________________________________________________________________________________________
AF::AF(const char* af) : fImpl(0x0)
{
  TString envFile(gSystem->ExpandPathName("$HOME/.aaf"));
  
  if ( gSystem->AccessPathName(envFile.Data()) )
  {
    std::cerr << "Cannot work without an environment file. Please make a $HOME/.aaf file !" << std::endl;
    throw;
  }
  
  TEnv env;
  
  env.ReadFile(envFile,kEnvAll);
  
  TString afMaster = env.GetValue(Form("%s.master",af),"unknown");
  TString afUser = env.GetValue(Form("%s.user",af),"unknown");
  Bool_t afDynamic = env.GetValue(Form("%s.dynamic",af),kFALSE);

  if ( afMaster != "unknown" && afUser != "unknown" )
  {
    if ( afDynamic )
    {
      fImpl = new AFDynamic(afUser,afMaster);
    }
    else
    {
      fImpl = new AFStatic(afUser,afMaster);
    }
  }
  else
  {
    std::cerr << "Could not get user/master/dynamic information for AF named " << af << std::endl;
  }
  
  if ( fImpl )
  {
    TString homeDir = env.GetValue(Form("%s.home",af),"/home/PROOF-AAF");
    TString logDir = env.GetValue(Form("%s.log",af),"/home/PROOF-AAF");
    
    fImpl->SetHomeAndLogDir(homeDir.Data(),logDir.Data());
    fImpl->SetDataDisk(env.GetValue(Form("%s.datadisk",af),"/data"));
  }
}
