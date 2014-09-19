#include "AFStatic.h"

#include "Riostream.h"
#include "TFileCollection.h"
#include "TFileInfo.h"
#include "TGrid.h"
#include "TGridCollection.h"
#include "TGridResult.h"
#include "THashList.h"
#include "TString.h"
#include "TSystem.h"
#include "TUrl.h"
#include <set>
#include "TProof.h"
#include "TFileMerger.h"
#include "TEnv.h"
#include <list>
#include <map>
#include <string>
#include <algorithm>
#include "TObjArray.h"

namespace
{
  Double_t byte2GB(1024*1024*1024);
}



//______________________________________________________________________________
void MakeRawCollection(const char* runlist, const char* collectionName, const char* collectionScript)
{
  TGrid::Connect("alien://");
  
  std::ifstream in(runlist);
  Int_t run;
  
  std::ofstream out(collectionScript);
  
  while ( in >> run )
  {
    TString basedir("/alice/data/2009/LHC09a/");
    
    TString dir(Form("%s%09d/raw/09*.root",basedir.Data(),run));
    TGridResult* r = gGrid->Ls(dir.Data());
    Int_t i(0);
    while ( r->GetFileName(i) ) 
    {
      out << Form("gbbox addFileToCollection %s%09d/raw/%s /alice/cern.ch/user/l/laphecet/%s",basedir.Data(),run,r->GetFileName(i++),collectionName) << endl;
    }
    delete r;
  }
  
  out.close();
}

//______________________________________________________________________________
AFStatic::AFStatic(const char* master) : VAF(master)
{
  fIsDynamicDataSet = kFALSE;
}

//______________________________________________________________________________
TFileCollection* AFStatic::CreateCollectionFromRunList(const char* collectionType,
                                                 const std::vector<int> runs,
                                                 const char* dataType,
                                                 const char* esdpass,
                                                 int aodPassNumber,
                                                 const char* basename,
                                                 bool computeTotalSize,
                                                 Int_t fileLimit)
{
  // collectionType can be : 
  //
  // script:colname : return will be 0x0 and scriptcolname.sh will be created to generate an alien collection named colname
  // ascii:filename : return will be 0x0 and filename.txt created with list of alien urls
  // xml:filename : return will be 0x0 and filename.xml created with full XML collection
  // name : return will be a TFileCollection object, named "name"
  //
  // dataType is case insensitive and will be converted into []
  //
  // *stat* -> [EventStat_temp.root]
  // *zip* -> [root_archive.zip] or [AOD_archive.zip]
  // *esd* -> [AliESDs.root]
  // *esd_outer* -> [AliESDs_Outer.root]
  // *esd_barrel* -> [AliESDs_Barrel.root]
  // *aod* -> [AliAOD.root]
  // *aodmuon* -> [AliAOD.Muons.root]
  // *aoddimuon* -> [AliAOD.Dimuons.root]
  // *esd.tag* -> [ESD.tag]  
  // *raw* -> [raw data file]
  //
  // Important note : to be "fast" (or at least not too slow), please try to
  // be as specific as possible with the basename...
  //
  
  if (!gGrid) TGrid::Connect("alien://");
  if (!gGrid) 
  {
    return 0x0;
  }
  
  TString sbasename(basename);
  
  if ( !sbasename.BeginsWith("/alice/data") &&
       !sbasename.BeginsWith("/alice/sim") )
  {
    if (!IsPrivateProduction())
    {
      cerr << "ERROR : Path is not from /alice/data or /alice/sim, but PrivateProduction() was not called !" << endl;
      return 0x0;
    }
  }
  else
  {
    if ( IsPrivateProduction() )
    {
      cout << "WARNING : PrivateProduction() active but path is /alice/data or /alice/sim : will ignore PrivateProduction" << endl;
    }
  }
                            
  TString what;
  TString defaultTreeName("");
  TString anchor;
  
  TString sdatatype = DecodeDataType(dataType,what,defaultTreeName,anchor,aodPassNumber);
  
  if ( what=="unknown" ) 
  {
    cout << "type=" << dataType << " is not supported." << endl;
    return 0x0;
  }
  
  ofstream out;
  
  TString scollectionType(collectionType);
  
  TFileCollection* fileCollection(0x0);
  Bool_t IsXML(kFALSE);
  Bool_t IsAliEnCollection(kFALSE);
  
  TString name(scollectionType);
  
  if ( scollectionType.Contains(":") )
  {
    TObjArray* parts = scollectionType.Tokenize(":");
    if ( parts->GetEntries() != 2 ) 
    {
      cout << "Malformed collectionType " << collectionType << endl;
      return 0x0;
    }
    else
    {
      TString ctype(static_cast<TObjString*>(parts->At(0))->String());
      name = static_cast<TObjString*>(parts->At(1))->String();
      
      ctype.ToUpper();
      if (ctype=="ASCII")
      {
        out.open(Form("%s.txt",name.Data()));    
      }
      else if ( ctype == "XML" ) 
      {
        IsXML = kTRUE;
        out.open(Form("%s.xml",name.Data()));    
      }
      else if ( ctype == "SCRIPT" ) 
      {
        IsAliEnCollection = kTRUE;
        out.open(Form("%s%s.sh",ctype.Data(),name.Data()));
      }
      else
      {
        cout << "Unknown collectionType " << ctype.Data() << endl;
        return 0x0;
      }      
    }
  }
  else
  {
    fileCollection = new TFileCollection(name.Data(),name.Data());
  }
  
  if ( IsXML ) 
  {
    out << Form("<?xml version=\"1.0\"?>\n<alien>\n  <collection name=\"%s\">",name.Data()) << endl;
  }
  
  Int_t count(0);
  Long64_t size(0); // size of requested files
  Long64_t totalSize(0); // size of the root archive containing the files
  
  if ( what == "root_archive.zip" ) computeTotalSize = kFALSE;
  
  for ( std::vector<int>::const_iterator it = runs.begin(); it != runs.end(); ++it ) 
  { 
    Int_t runNumber = (*it);
    
    TString search;
    TString mbasename;

    GetSearchAndBaseName(runNumber,basename, what, sdatatype.Data(), esdpass, aodPassNumber,mbasename, search);
    
    cout << Form("basename=%s search=%s",mbasename.Data(),search.Data()) << endl;
    
    TGridResult* res = gGrid->Query(mbasename.Data(),search.Data());
    
    TGridResult* res2 = 0x0;
    
    if ( computeTotalSize && what != "root_archive.zip" ) 
    {
      search.ReplaceAll(what,"root_archive.zip");
      
      res2 = gGrid->Query(mbasename.Data(),search.Data());
    }
    
    Int_t nFiles = res->GetEntries();
    
    if ( !nFiles && ( aodPassNumber < 0 ) )
    {
      // try "merged" esds path
      cout << "No result. Trying another path..." << endl;
      cout << Form("basename=%s search=%s",mbasename.Data(),search.Data()) << endl;

//      mbasename=Form("%s/%09d/ESDs/%s/*/*",basename,runNumber,esdpass);
      mbasename=Form("%s/%09d/ESDs/%s/*",basename,runNumber,esdpass);
      search = what.Data(); 
      res = gGrid->Query(mbasename.Data(),search.Data());
      if ( computeTotalSize && what != "root_archive.zip" ) 
      {
        search.ReplaceAll(what,"root_archive.zip");        
        res2 = gGrid->Query(mbasename.Data(),search.Data());
      }
    }
    
    nFiles = res->GetEntries();
    
    if (!nFiles) 
    {
      continue;
    }
    
    for (Int_t i = 0; i < nFiles; ++i) 
    {
      ++count;
      
      if ( TString(res->GetKey(i,"lfn")).Contains("Stage")) continue;
      
      if ( fileLimit > 0 && count >= fileLimit ) break;
      
      Long64_t thisSize = TString(res->GetKey(i,"size")).Atoll();
      
      if ( IsAliEnCollection ) 
      {
        const TString alienpath("/alice/cern.ch/user/l/laphecet/Collections");
        TUrl url(res->GetKey(i,"turl"));
        out << Form("gbbox addFileToCollection %s %s/%s",url.GetFile(),alienpath.Data(),name.Data()) << endl;        
      }
      else if ( IsXML ) 
      {
        out << Form("    <event name=\"%d\">",count) << endl;
        out << Form("      <file name=\"%s\" aclId=\"%s\" broken=\"%s\" ctime=\"%s\" "
                    "dir=\"%s\" entryId=\"%s\" expiretime=\"%s\" gowner=\"%s\" "
                    "guid=\"%s\" guidtime=\"%s\" lfn=\"%s\" md5=\"%s\" owner=\"%s\" "
                    " perm=\"%s\" replicated=\"%s\" size=\"%s\" turl=\"%s\" type=\"%s\" />",
                    gSystem->BaseName(res->GetKey(i,"lfn")),
                    res->GetKey(i,"aclId"),
                    res->GetKey(i,"broken"),
                    res->GetKey(i,"ctime"),
                    res->GetKey(i,"dir"),
                    res->GetKey(i,"entryId"),
                    res->GetKey(i,"expiretime"),
                    res->GetKey(i,"gowner"),
                    res->GetKey(i,"guid"),
                    res->GetKey(i,"guidtime"),
                    res->GetKey(i,"lfn"),
                    res->GetKey(i,"md5"),
                    res->GetKey(i,"owner"),
                    res->GetKey(i,"perm"),
                    res->GetKey(i,"replicated"),
                    res->GetKey(i,"size"),
                    res->GetKey(i,"turl"),
                    res->GetKey(i,"type")) << endl;
        out <<      "    </event>" << endl;
      }
      else if ( !fileCollection ) 
      {
        out << res->GetKey(i,"turl") << endl;
      }
      else if ( fileCollection )
      {
        TString surl = res->GetKey(i,"turl");
        if ( FilterName().Length() > 0 )
        {
          surl.ReplaceAll(".root",Form(".%s.root",FilterName().Data()));
        }
        fileCollection->Add(new TFileInfo(surl.Data(),
                                          thisSize,
                                          res->GetKey(i,"guid"),
                                          res->GetKey(i,"md5")));
      }
      
      size += thisSize;
      
      if (res2)
      {
        totalSize += TString(res2->GetKey(i,"size")).Atoll();
      }
    }
    delete res;
    delete res2;
  }
  
  TString summary(Form("numberoffiles=\"%d\" size=\"%7.2f GB\" ",count,size/byte2GB));
  
  if ( computeTotalSize ) 
  {
    summary += Form("archivesize=\"%7.2f GB\"",totalSize/byte2GB);
  }
  
  cout << Form("Collection %s created : %s",name.Data(),summary.Data()) << endl;
  
  if ( IsXML ) 
  {
    out << Form("  <summary %s />",summary.Data()) << endl;  
    out << "  </collection>" << endl;
    out << "</alien>" << endl;
  }
  
  out.close();
  
  if (fileCollection) 
  {
    fileCollection->Update();
    fileCollection->SetDefaultTreeName(defaultTreeName.Data());
    
    if (anchor.Length())
    {
      TIter next(fileCollection->GetList());
      TFileInfo* fileInfo;
      
      while ( ( fileInfo = static_cast<TFileInfo*>(next()) ) )
      {
        TUrl* url;
        fileInfo->ResetUrl();
        while ( ( url = fileInfo->NextUrl() ) )
        {
          url->SetAnchor(anchor.Data());
        }
      }
    }
  }
  
  return fileCollection;
}


//______________________________________________________________________________
TFileCollection* AFStatic::CreateCollectionFromRunList(const char* collectionType,
                                                 const char* runlist,
                                                 const char* dataType,
                                                 const char* esdpass,
                                                 int aodPassNumber,
                                                 const char* basename,
                                                 bool computeTotalSize,
                                                 Int_t fileLimit)
{
  
  std::vector<int> runs;
  
  ReadIntegers(runlist,runs);
  
  cout << runs.size() << " runs" << endl;
  
  return CreateCollectionFromRunList(collectionType,runs,dataType,esdpass,aodPassNumber,basename,computeTotalSize,fileLimit);
}

//______________________________________________________________________________
TFileCollection* AFStatic::CreateCollectionFromRunList(const char* collectionType,
                                                 Int_t runNumber,
                                                 const char* dataType,
                                                 const char* esdpass,
                                                 int aodPassNumber,
                                                 const char* basename,
                                                 bool computeTotalSize,
                                                 Int_t fileLimit)
{
  std::vector<int> runs;
  runs.push_back(runNumber);
  return CreateCollectionFromRunList(collectionType,runs,dataType,esdpass,aodPassNumber,basename,computeTotalSize,fileLimit);
  
}

//______________________________________________________________________________
void AFStatic::CreateDataSets(const std::vector<int>& runs,
                        const char* dataType,
                        const char* esdpass,
                        int aodPassNumber,
                        const char* basename,
                        Int_t fileLimit)
{
  // create one dataset per run
  
  if (!DryRun())
  {
    if (!Connect())
    {
      std::cerr << "Failed to connect !" << std::endl;
      return;
    }
  }
  
  Int_t nfiles(0);
  Long64_t size(0);
  TString prefix("");
  
  for ( std::vector<int>::size_type i = 0; i < runs.size(); ++i ) 
  {
    int run = runs[i];
    TFileCollection* fc = CreateCollectionFromRunList(Form("RUN%d",run),
                                                      run,
                                                      dataType,
                                                      esdpass,
                                                      aodPassNumber,
                                                      basename,
                                                      false,
                                                      fileLimit);
    if (!fc) continue;
    
    if (fc && fc->GetNFiles()==0) continue;
    
    TString dsname;
    TString dt(dataType);
    dt.ToUpper();
    dt.ReplaceAll("ZIP","");
    dt.ReplaceAll(" ","");
    TString sesdpass(esdpass);
    
    if ( dt.Contains("RAW") )
    {
      sesdpass="";
      aodPassNumber=-1;
    }
    
    TString period;
    
    TObjArray* a = TString(basename).Tokenize("/");
    TIter next(a);
    TObjString* s;
    while ( ( s = static_cast<TObjString*>(next()) ) )
    {
      if (s->String().BeginsWith("LHC"))
      {
        period = s->String();
        break;
      }
    }
    delete a;
    
    if ( !IsPrivateProduction() ) 
    {
      if (aodPassNumber>=0)
      {
        dsname = Form("%s%s_%s_%s%03d_%09d",prefix.Data(),period.Data(),sesdpass.Data(),dt.Data(),aodPassNumber,run);
      }
      else
      {
        dsname = Form("%s%s_%s_%s_%09d",prefix.Data(),period.Data(),sesdpass.Data(),dt.Data(),run);
      }
    }
    else
    {
      if ( run < 100000 ) 
      {
        dsname = Form("%s_%d",PrivateProduction().Data(),run);                
      }
      else
      {
        dsname = Form("%s_%09d",PrivateProduction().Data(),run);        
      }
    }
    
    if ( FilterName().Length() > 0 )
    {
      dsname += "_";
      dsname += FilterName();
    }

    dsname.ReplaceAll("__","_");
    
    fc->SetName(dsname.Data());
    
    if ( !gProof )
    {
      fc->Print("f");      
    }
    else
    {
      gProof->RegisterDataSet(dsname.Data(),fc);
    }
    
    nfiles += fc->GetNFiles();
    size += fc->GetTotalSize();
  }
  
  cout << "----- TOTAL " << endl;
  
  cout << Form("nruns=%d nfiles = %d | size = %7.2f GB",(Int_t)runs.size(),nfiles,size/byte2GB) << endl;
}

