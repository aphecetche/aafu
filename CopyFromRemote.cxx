#include "CopyFromRemote.h"
#include <fstream>
#include "TString.h"
#include "TUrl.h"
#include "TGrid.h"
#include "TSystem.h"
#include <iostream>
#include "TFile.h"

void CopyFromRemote(const char* txtfile)
{
  /// Copy a list of remote urls locally, keeping the absolute path
  /// (the destination local directory must exist and be writeable, of course)
  
  char line[1024];
  std::ifstream in(gSystem->ExpandPathName(txtfile));
  
  while ( in.getline(line,1024,'\n') )
  {
  	TUrl url(line);
  	
    if ( TString(url.GetProtocol()) == "alien" )
    {
      if (!gGrid)
      {
        TGrid::Connect("alien://");
        if (!gGrid)
        {
                std::cout << "Cannot get alien connection\n";
          return;
        }
      }
      
    }
  
  	TString file(url.GetFile());
  
//    file.ReplaceAll("/alice","/home/laphecet/data/alice");	
//    file.ReplaceAll("/PWG3","/home/laphecet/PWG3");

  	TString dir(gSystem->DirName(file));
  	
  	gSystem->mkdir(dir.Data(),kTRUE);
    
    if ( gSystem->AccessPathName(file.Data())==kFALSE)
    {
            std::cout << "Skipping copy of " << file.Data() << " as it already exists\n";
    }
    else
    {
      std::cout << "TFile::Cp(" << line << "," << file.Data() << ")" << std::endl;
  	  TFile::Cp(line,file.Data());
      if ( TString(line).Remove(TString::kTrailing,' ').EndsWith("zip") )
      {
        gSystem->Exec(Form("unzip %s -d %s",file.Data(),gSystem->DirName(file.Data())));
        gSystem->Exec(Form("rm %s",file.Data()));
      }
      if (strlen(url.GetAnchor())>0)
      {
        gSystem->Exec(Form("unzip %s %s -d %s",file.Data(),url.GetAnchor(),gSystem->DirName(file.Data())));
        gSystem->Exec(Form("rm %s",file.Data()));
      }
  	}
  }
}

