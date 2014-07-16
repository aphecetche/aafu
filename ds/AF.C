#include "AF.h"

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
AF::AF(const char* af) : fConnect(""), fDryRun(kTRUE), fMergedOnly(kTRUE), fSimpleRunNumbers(kFALSE), fFilterName("")
{
  TString envFile(gSystem->ExpandPathName("$HOME/.aaf"));
  
  if ( gSystem->AccessPathName(envFile.Data()) )
  {
    std::cerr << "Cannot work without an environment file. Please make a $HOME/.aaf file !" << std::endl;
    throw;
  }
  
  TEnv env(envFile);
  
  TString afHost = env.GetValue(af,"unknown");
  TString afUser = env.GetValue("user","unknown");
  
  if ( afHost != "unknown" && afUser != "unknown" )
  {
    fConnect = afUser;
    fConnect += "@";
    fConnect += afHost;
    fConnect += "/?N";
  }
  
  std::cout << "Connect string to be used = " << fConnect.Data() << std::endl;
}

//______________________________________________________________________________
Bool_t AF::Connect(const char* option)
{
  // FIXME: how to tell, for an existing connection, which option was used ?
  
  if ( gProof ) return kTRUE;
  
  if ( fConnect.Length()==0 ) return kFALSE;
  
  TProof::Open(fConnect.Data(),option);
  
  return (gProof != 0x0);
}

//______________________________________________________________________________
TFileCollection* AF::CreateCollectionFromRunList(const char* collectionType,
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
                            
  TString sdatatype(dataType);
  sdatatype.ToUpper();
  TString defaultWhat("unknown");
  TString what(defaultWhat);
  TString defaultTreeName("");
  TString anchor;
  
  if ( sdatatype.Contains("ESD.TAG") )
  {
    what = "*ESD.tag.root";
  }
  else if ( sdatatype.Contains("ESD_OUTER") )
  {
    what = "AliESDs_Outer.root";
    defaultTreeName="/esdTree";
  }
  else if ( sdatatype.Contains("ESD_BARREL") )
  {
    what = "AliESDs_Barrel.root";
    defaultTreeName="/esdTree";
  }
  else if ( sdatatype.Contains("ESD") )
  {
    what = "AliESDs.root";
    defaultTreeName="/esdTree";
  }
  else if ( sdatatype.Contains("AODMU") )
  {
    what = "AliAOD.Muons.root";
    defaultTreeName="/aodTree";
  }
  else if ( sdatatype.Contains("AODDIMU") )
  {
    what = "AliAOD.Dimuons.root";
    defaultTreeName="/aodTree";
  }
  else if ( sdatatype.Contains("AODHF") )
  {
    what = "AliAOD.VertexingHF.root";
    defaultTreeName = "/aodTree";
  }
  else if ( sdatatype.Contains("AODJE") )
  {
    what = "AliAOD.Jets.root";
    defaultTreeName = "/aodTree";
  }
  else if ( sdatatype.Contains("AOD") )
  {
    what = "AliAOD.root";
  }
  else if ( sdatatype.Contains("STAT") )
  {
    what = "EventStat_temp.root";
  }
  else if ( sdatatype.Contains("RAW") )
  {
    what = "*.root";
  }
  if ( sdatatype.Contains("ZIP") )
  {
    if ( what != defaultWhat ) 
    {
      anchor = what;
      anchor.ReplaceAll("ZIP","");
    }
//    else
//    {
//      cout << "type=" << dataType << " is not enough to know what to look for..." << endl;
//      return 0x0;
//    }
    if ( sdatatype.Contains("AOD") )
    {
      if ( aodPassNumber < 0 ) 
      {
        what = "aod_archive.zip";
      }
      else if ( aodPassNumber == 0 ) 
      {
        what = "AOD_archive.zip";
      }
      else
      {
        what = "root_archive.zip";        
      }
    }
    else
    {
      what = "root_archive.zip";
    }
  }
  
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
    
    if ( IsPrivateProduction() && !sbasename.BeginsWith("/alice/data") && !sbasename.BeginsWith("/alice/sim") )
    {
      if ( IsSimpleRunNumbers() )
      {
        mbasename = Form("%s/%d",basename,runNumber);
      }
      else
      {
        mbasename = Form("%s/%09d",basename,runNumber);        
      }
      if ( fPrivateProduction.Contains("ROBERTA") )
      {
        mbasename += "/1";
      }
      search = what.Data();
    }
    else
    {
      if ( sdatatype.Contains("ESD") )
      {
        if ( ( ( what.Contains("_Outer") || what.Contains("_Barrel") ) && TString(esdpass) == "cpass1" )
             || TString(esdpass) == "pass1" )
        {
          mbasename=Form("%s/%09d/%s/*.*",basename,runNumber,esdpass);        
        }
        else
        {
          mbasename=Form("%s/%09d/ESDs/%s/*.*",basename,runNumber,esdpass);
        }
        search = what.Data();      
      }
      if ( sdatatype.Contains("AOD") || sdatatype.Contains("STAT") ) 
      {
        if ( sbasename.BeginsWith("/alice/sim") )
        {
          //        	/alice/sim/2011/LHC11e1_rawocdb/139517/AOD068
          if ( aodPassNumber>0 )
          {
            mbasename = Form("%s/%s/%d/AOD%03d/*",basename,esdpass,runNumber,aodPassNumber);
          }
          else if ( aodPassNumber == 0 )
          {
            mbasename = Form("%s/%s/%d/AOD/*",basename,esdpass,runNumber);
          }
          else
          {
            mbasename = Form("%s/%d/*/",basename,runNumber);
          }
          search = what.Data();
        }
        else if (aodPassNumber>=0)
        {
          if ( aodPassNumber==0)
          {
            mbasename = Form("%s/%09d/ESDs/%s/AOD/*",basename,runNumber,esdpass);            
          }
          else if ( fMergedOnly )
          {
            mbasename = Form("%s/%09d/%s/AOD%03d/*",basename,runNumber,esdpass,aodPassNumber);
//            mbasename = Form("%s/%09d/ESDs/%s/AOD%03d/AOD/*",basename,runNumber,esdpass,aodPassNumber);
          }
          else
          {
            mbasename = Form("%s/%09d/ESDs/%s/AOD%03d",basename,runNumber,esdpass,aodPassNumber);
          }
          search = what.Data();      
        }
        else if ( aodPassNumber == -1 )
        {
          // aods in same dir as ESDs (e.g. LHC11h_HLT)
          mbasename = Form("%s/%09d/ESDs/%s/*.*",basename,runNumber,esdpass);
          search = what.Data();                
        }
        else if ( aodPassNumber == -2 )
        {
          // aods in their own directory (not under runNumber/ESDs/pass... but directly under runNumber/pass... e.g. LHC12c pass1...)
          mbasename = Form("%s/%09d/%s/AOD/*",basename,runNumber,esdpass);
          search = what.Data();
          
        }
      }
      if ( sdatatype.Contains("RAW") )
      {
        mbasename=Form("%s/%09d/raw",basename,runNumber);
        search = Form("*%09d*.root",runNumber);
      }
    }
    
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
        if ( fFilterName.Length() > 0 )
        {
          surl.ReplaceAll(".root",Form(".%s.root",fFilterName.Data()));
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
void AF::ReadIntegers(const char* filename, std::vector<int>& integers)
{
  /// Read integers from filename, where integers are either
  /// separated by "," or by return carriage
  ifstream in(gSystem->ExpandPathName(filename));
  int i;
  
  std::set<int> runset;
  
  char line[10000];
  
  in.getline(line,10000,'\n');
  
  TString sline(line);
  
  if (sline.Contains(","))
  {
    TObjArray* a = sline.Tokenize(",");
    TIter next(a);
    TObjString* s;
    while ( ( s = static_cast<TObjString*>(next()) ) )
    {
      runset.insert(s->String().Atoi());
    }
    delete a;
  }
  else
  {
    runset.insert(sline.Atoi());
    
    while ( in >> i )
    {
      runset.insert(i);
    }
  }
  
  for ( std::set<int>::const_iterator it = runset.begin(); it != runset.end(); ++it ) 
  {
    integers.push_back((*it)); 
  }
  
  std::sort(integers.begin(),integers.end());
}


//______________________________________________________________________________
void AF::ShowDataSets(const char* runlist)
{
  // Show all datasets for the runs in run list
  std::vector<int> runs;
  ReadIntegers(runlist,runs);
  if (runs.empty()) return;
  
  if ( Connect() )
  {
    for ( std::vector<int>::size_type i = 0; i < runs.size(); ++i )
    {
      TList list;
      GetDataSetList(list,Form("*%d*",runs[i]));
      TIter next(&list);
      TObjString* s;
      while ( ( s = static_cast<TObjString*>(next()) ) )
      {
        cout << s->String().Data() << endl;
      }
    }
  }
}

//______________________________________________________________________________
void AF::ShowDataSetContent(const char* dssetfile)
{
  /// Show the content of one or several datasets
  
  TList list;
  list.SetOwner(kTRUE);
  
  if ( gSystem->AccessPathName(dssetfile) )
  {
    // single dataset (i.e. not a text file containing a list of dataset...)
    
    std::cout << "INFO: " << dssetfile << " is not a file name, assuming it's a dataset" << std::endl;
    list.Add(new TObjString(dssetfile));
  }
  else
  {
    ifstream in(gSystem->ExpandPathName(dssetfile));
    char line[1024];
    
    while ( ( in.getline(line,1023,'\n') ) )
    {
      list.Add(new TObjString(line));
    }
  }
  
  ShowDataSetContent(list);
}

//______________________________________________________________________________
void AF::ShowDataSetContent(const TList& list)
{
  if ( Connect() )
  {
    TIter next(&list);
    TObjString* str;
  
    while ( ( str = static_cast<TObjString*>(next())) )
    {
      TFileCollection* fc = gProof->GetDataSet(str->String().Data());
      if (!fc) continue;
    
      TIter nextFileInfo(fc->GetList());
      TFileInfo* fi;
      while ( ( fi = static_cast<TFileInfo*>(nextFileInfo()) ) )
      {
        TUrl url(*(fi->GetFirstUrl()));
      
        cout << url.GetUrl() << endl;
      }
    }
  }
}

//______________________________________________________________________________
void AF::CompareRunLists(const char* runlist1, const char* runlist2)
{
  std::vector<int> v1, v2;
  
  ReadIntegers(runlist1,v1);
  ReadIntegers(runlist2,v2);
  
  cout << "Only in v1:" << endl;
  
  int n(0);
  
  for ( std::vector<int>::size_type i = 0; i < v1.size(); ++i ) 
  {
    int run = v1[i];
    std::vector<int>::const_iterator it = std::find(v2.begin(),v2.end(),run);
    if ( it == v2.end() )
    {
      cout << run << endl;
      ++n;
    }
  }
  cout << n << " runs" << endl;
  
  n = 0;
  
  cout << "Only in v2:" << endl;
  for ( std::vector<int>::size_type i = 0; i < v2.size(); ++i ) 
  {
    int run = v2[i];
    std::vector<int>::const_iterator it = std::find(v1.begin(),v1.end(),run);
    if ( it == v1.end() )
    {
      cout << run << endl;
      ++n;
    }
  }
  cout << n << " runs" << endl;
  
}

//______________________________________________________________________________
TFileCollection* AF::CreateCollectionFromRunList(const char* collectionType,
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
TFileCollection* AF::CreateCollectionFromRunList(const char* collectionType,
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
void AF::CreateDataSets(const std::vector<int>& runs,
                        const char* dataType,
                        const char* esdpass,
                        int aodPassNumber,
                        const char* basename,
                        Int_t fileLimit)
{
  // create one dataset per run
  
  TProof* p(0x0);
    
  if (!DryRun())
  {
    if (!Connect()) return;
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
    
    if ( fFilterName.Length() > 0 )
    {
      dsname += "_";
      dsname += fFilterName;
    }

    dsname.ReplaceAll("__","_");
    
    fc->SetName(dsname.Data());
    
    if ( !p )
    {
      fc->Print("f");      
    }
    else
    {
      p->RegisterDataSet(dsname.Data(),fc);
    }
    
    nfiles += fc->GetNFiles();
    size += fc->GetTotalSize();
  }
  
  cout << "----- TOTAL " << endl;
  
  cout << Form("nruns=%d nfiles = %d | size = %7.2f GB",(Int_t)runs.size(),nfiles,size/byte2GB) << endl;
}

//______________________________________________________________________________
void AF::CreateDataSets(const char* runlist,
                        const char* dataType,
                        const char* esdpass,
                        int aodPassNumber,
                        const char* basename,
                        Int_t fileLimit)
{
  std::vector<int> runs;
  
  ReadIntegers(runlist,runs);
  
  CreateDataSets(runs,dataType,esdpass,aodPassNumber,basename,fileLimit);
}

//______________________________________________________________________________
void AF::CreateDataSets(Int_t runNumber,
                        const char* dataType,
                        const char* esdpass,
                        int aodPassNumber,
                        const char* basename,
                        Int_t fileLimit)
{
  std::vector<int> runs;
  
  runs.push_back(runNumber);
  
  CreateDataSets(runs,dataType,esdpass,aodPassNumber,basename,fileLimit);
}

//______________________________________________________________________________
void AF::GetOneDataSetSize(const char* dsname, Int_t& nFiles, Int_t& nCorruptedFiles, Long64_t& size, Bool_t showDetails)
{
  // compute the size of one dataset

  nFiles = 0;
  nCorruptedFiles = 0;
  size = 0;

  if ( Connect() )
  {
    TFileCollection* fc = gProof->GetDataSet(dsname);
  
    if (!fc) return;
  
    nFiles = fc->GetNFiles();
    nCorruptedFiles = fc->GetNCorruptFiles();
    size = fc->GetTotalSize();
  
    if (showDetails)
    {
      cout << Form("%s nfiles = %5d | ncorrupted = %5d | size = %7.2f GB",dsname,nFiles,nCorruptedFiles,size/byte2GB) << endl;
    }
  }
}

//______________________________________________________________________________
void AF::GetDataSetsSize(const char* dsList, Bool_t showDetails)
{
  // compute the size of a list of datasets
  
  ifstream in(gSystem->ExpandPathName(dsList));
	char line[1024];
  
  Int_t nFiles(0);
  Int_t nCorruptedFiles(0);
  Long64_t size(0);

  Int_t totalFiles(0);
  Int_t totalCorruptedFiles(0);
  Long64_t totalSize(0);

  Int_t n(0);
  
	while ( in.getline(line,1024,'\n') )
	{
    GetOneDataSetSize(line,nFiles,nCorruptedFiles,size,showDetails);
    ++n;
    totalFiles += nFiles;
    totalCorruptedFiles += nCorruptedFiles;
    totalSize += size;
  }
  
  cout << Form("ndatasets=%3d nfiles = %5d | ncorrupted = %5d | size = %7.2f GB",n,totalFiles,totalCorruptedFiles,totalSize/byte2GB) << endl;
}

//______________________________________________________________________________
void AF::MergeOneDataSet(const char* dsname)
{
  if (!Connect()) return;
  
  TFileCollection* fc = gProof->GetDataSet(dsname);
  
  if (!fc)
  {
    cout << "Could not get dataset " << dsname << endl;
    return;
  }
  
  TFileMerger fm(kTRUE,kFALSE);
  TString output(dsname);
  
  output.ReplaceAll("/","_");
  output += ".merged";
  
  TIter next(fc->GetList());
  TFileInfo* fi;
  while ( ( fi = static_cast<TFileInfo*>(next()) ) )
  {
    TUrl url(*(fi->GetFirstUrl()));
    
    cout << url.GetUrl() << endl;
    cout << "Adding " << url.GetUrl() << flush;
    fm.AddFile(url.GetUrl(),kFALSE);
    cout << "done." << endl;
  }
  
  cout << "output=" << output.Data() << endl;
  
  if (!DryRun())
  {
    fm.OutputFile(output.Data());
    cout << "starting to merge" << endl;    
    fm.Merge();
  }
  else
  {
    cout << "DRY RUN ONLY. NO MERGING DONE" << endl;
  }
}

//______________________________________________________________________________
void AF::MergeDataSets(const char* dsList)
{
  if ( !Connect() ) return;

  ifstream in(gSystem->ExpandPathName(dsList));
	char line[1024];
  
	while ( in.getline(line,1024,'\n') ) 
	{
    MergeOneDataSet(line);
  }
}

//______________________________________________________________________________
void AF::GroupDatasets(const TList& dslist)
{
  std::map<std::string, std::list<std::string> > groups;
  std::map<std::string, Long64_t > groupSize;
  std::map<Long64_t, std::list<std::string> > groupOrderedBySize;
  
  TIter next(&dslist);
  TObjString* str;
  
  while ( (str = static_cast<TObjString*>(next())) )
  {
    TString sname(str->String());
    sname = sname.Strip();
    
    TString test(sname.Data());
    
    Int_t ix = test.Index("_000");
    
    TString id(sname.Data());
    
    if ( ix >= 0 )
    {
      TString runNumber = test(ix,10);
      test.ReplaceAll(runNumber,"");
      test.ReplaceAll(" ","");
      id = test;
    }
    
    groups[id.Data()].push_back(sname.Data());

    Long64_t size(0);
    Int_t nfiles,ncorrupted;
    GetOneDataSetSize(sname.Data(),nfiles,ncorrupted,size,kFALSE);
    groupSize[id.Data()] += size;
  }
  
  std::map<std::string,std::list<std::string> >::const_iterator it;
  
  for ( it = groups.begin(); it != groups.end(); ++it )
  {
    
    groupOrderedBySize[groupSize[it->first]].push_back(it->first);
    
    const std::list<std::string>& li = it->second;
    std::list<std::string>::const_iterator lit;
    
    std::cout << it->first;
    
    std::cout << Form(" NDATASETS = %3lu SIZE = %6.3f GB ",li.size(),groupSize[it->first]*1.0/byte2GB) << std::endl;
    
    for ( lit = li.begin(); lit != li.end(); ++lit )
    {
    	std::cout << "   " << (*lit) << std::endl;
    }
  }
  
  std::cout << "-----------------------------------------" << std::endl;
  std::cout << "-----------------------------------------" << std::endl;
  std::cout << "-----------------------------------------" << std::endl;
  
  std::map<Long64_t,std::list<std::string> >::const_iterator sit;
  
  for ( sit = groupOrderedBySize.begin(); sit != groupOrderedBySize.end(); ++sit )
  {
    std::cout << Form("SIZE = %8.3f GB - ",sit->first*1.0/byte2GB);
    
    const std::list<std::string>& li = sit->second;
    if ( li.size() == 1 )
    {
      const std::list<std::string>& lst = groups[li.front()];
      
      std::cout << li.front() << " (contains " << lst.size() << " datasets) - ";
    }
    else
    {
      std::cout << "   " << li.size() << " datasets : ";
      std::list<std::string>::const_iterator lit;
      
      for ( lit = li.begin(); lit != li.end(); ++lit )
      {
        std::cout << " [ " << (*lit) << " ] ";
      }
    }
    std::cout << std::endl;
  }

}

//______________________________________________________________________________
void AF::GetDataSetList(TList& list, const char* path)
{
  list.SetOwner(kTRUE);
  list.Clear();
  
  if ( Connect() )
  {
    TMap* datasets = gProof->GetDataSets(path, ":lite:");
    
    if (!datasets)
    {
      return;
    }
    
    datasets->SetOwnerKeyValue();  // important to avoid leaks!
    TIter dsIterator(datasets);
    TObjString* dsName;
    
    while ((dsName = static_cast<TObjString *>(dsIterator.Next())))
    {
      list.Add(new TObjString(*dsName));
    }
  }
}

//______________________________________________________________________________
void AF::GroupDatasets()
{
  if (!Connect()) return;
  
  TList list;
  
  GetDataSetList(list);
  
  GroupDatasets(list);
}

//______________________________________________________________________________
void AF::GroupDatasets(const char* dslist)
{
  /// given a filename containing a list of dataset names
  /// try to group them (criteria being ignoring the run numbers to group)
  ///
  ifstream in(dslist);
  char name[1024];

  TList list;
  list.SetOwner(kTRUE);
  
  while (in.getline(name,1024,'\n'))
  {
    TString sname(name);
    sname = sname.Strip();
   
    list.Add(new TObjString(sname));
  }

  GroupDatasets(list);
}

//______________________________________________________________________________
void AF::RemoveDataFromOneDataSet(const char* dsName, std::ofstream& out)
{
  TFileCollection* fc = gProof->GetDataSet(dsName);
  
  TIter next(fc->GetList());
  TFileInfo* fi;
  while ( ( fi = static_cast<TFileInfo*>(next()) ) )
  {
    TUrl url(*(fi->GetFirstUrl()));
    
    if  (TString(url.GetHost()).Contains("nansafmaster")) continue; // means not staged, so nothing to remove...
    
    out << Form("echo \"xrd %s rm %s\"",url.GetHost(),url.GetFile()) << std::endl;
    
    out << "xrd " << url.GetHost() << " rm " << url.GetFile() << std::endl;
  }
  
}

//______________________________________________________________________________
void AF::RemoveDataFromOneDataSet(const char* dsName)
{
  if ( fConnect.Length()==0 ) return;
  TProof::Open(fConnect.Data(),"masteronly");
  if (!gProof) return;
  

  std::ofstream out("delete-one-dataset.sh");
  
  RemoveDataFromOneDataSet(dsName,out);
  
  out.close();
}

//______________________________________________________________________________
void AF::RemoveDataFromDataSetFromFile(const char* dslist)
{
  if ( fConnect.Length()==0 ) return;
  TProof::Open(fConnect.Data(),"masteronly");
  if (!gProof) return;

  std::ofstream out("delete-multiple-dataset.sh");
  
  ifstream in(gSystem->ExpandPathName(dslist));
  char line[1024];
  
  while ( in.getline(line,1024,'\n') )
  {
    std::cout << "Preparing cmd line for dataset " << line << std::endl;
    RemoveDataFromOneDataSet(line,out);
  }
  
  out.close();
}

//______________________________________________________________________________
void AF::RemoveDataSets(const char* dslist)
{
  if ( fConnect.Length()==0 ) return;
  TProof::Open(fConnect.Data(),"masteronly");
  if (!gProof) return;

  ifstream in(gSystem->ExpandPathName(dslist));
  char line[1024];
  
  while ( in.getline(line,1024,'\n') )
  {
    std::cout << "Removing data set " << line << std::endl;
    gProof->RemoveDataSet(line);
  }
}

//______________________________________________________________________________
void AF::ShowDataSetList(const char* path)
{
  TList datasets;
  GetDataSetList(datasets,path);

  TIter next(&datasets);
  TObjString* dsName;
  
  while ((dsName = static_cast<TObjString*>(next())))
  {
    std::cout << dsName->String().Data() << std::endl;
  }
}

//______________________________________________________________________________
void AF::AnalyzeFileList(const char* filelist, const char* deletePath)
{
  // analyse filelist created with :
  // TProof::Open("nansafmaster.in2p3.fr/?N","workers=1x");
  // gProof->Exec(".!hostname ; ls -alS /pool/data/01/xrddata/"); > filelist.txt

  ofstream* out(0x0);
  
  if (strlen(deletePath) > 0)
  {
    out=new ofstream("analyze-delete.sh");
  }
  
  ifstream in(gSystem->ExpandPathName(filelist));
  char line[1024];
  std::string worker;
  
  typedef std::map<std::string, unsigned long > ByWorkerMap;

  std::map<std::string,ByWorkerMap > workers;
  ByWorkerMap* currentMap(0x0);
  
  while ( in.getline(line,1024,'\n') )
  {
    TString sline(line);
    if ( sline.BeginsWith("nan") )
    {
      TObjArray* a = sline.Tokenize(".");

      worker=static_cast<TObjString*>(a->At(0))->String();
      
      delete a;
      
      workers[worker] = ByWorkerMap();
      currentMap = &(workers[worker]);
      std::cout << "worker = " << worker << std::endl;
      continue;
    }
    if ( sline.BeginsWith("total") || sline.BeginsWith("d") )
    {
      continue;
    }
    
    TObjArray* a = sline.Tokenize(" ");
    
    TString size = static_cast<TObjString*>(a->At(4))->String().Data();
    TString path = static_cast<TObjString*>(a->At(8))->String().Data();

    delete a;


    path.ReplaceAll("%","/");
    
    if ( out && path.Contains(deletePath) )
    {
      int ix = path.Index("/alice");
      
      (*out) << "xrd " << worker << " rm " << path(ix,path.Length()-ix) << std::endl;
    }
    
    TString file = gSystem->BaseName(path.Data());
    
    if ( file.Contains("FILTER_RAWMUON") )
    {
      file = "FILTER_RAWMUON";
    }
    
    if ( file.BeginsWith("12000") )
    {
      file = "RAW 2012";
    }

    if ( file.BeginsWith("11000") )
    {
      file = "RAW 2011";
    }

    if ( file.BeginsWith("10000") )
    {
      file = "RAW 2010";
    }

    unsigned long thisSize = size.Atoi();
    
//    std::cout << worker << " " << thisSize << " " << file << std::endl;
    
    (*currentMap)[file.Data()] += thisSize;

    // find period
    
    int i = path.Index("LHC");
    
    if ( i > 0 )
    {
      TString period("PERIOD:");
      period += path(i,7);
      (*currentMap)[period.Data()] += thisSize;
    }
    

  }
  
  // try to present the results in a nice table
  
  // so let's get first the list of files
  std::set<std::string> files;
  ByWorkerMap workerSizes;
  
  for ( std::map<std::string,ByWorkerMap >::const_iterator it = workers.begin();
       it != workers.end(); ++it )
  {
    unsigned long thisWorkerSize(0);
    
    for ( ByWorkerMap::const_iterator it2 = it->second.begin();
         it2 != it->second.end(); ++it2 )
    {
      files.insert(it2->first);
      
      if ( !TString(it2->first.c_str()).BeginsWith("PERIOD:") )
      {
        thisWorkerSize += it2->second;
      }
    }
    
    workerSizes[it->first] = thisWorkerSize;
  }

  std::cout << Form("%40s"," ");
  
  for ( ByWorkerMap::const_iterator it = workerSizes.begin();
       it != workerSizes.end(); ++it )
  {
    std::cout << Form("%11s |",it->first.c_str());
  }
  std::cout << std::endl << std::string(120,'-') << std::endl;

  // now loop over files ...
  for ( std::set<std::string>::const_iterator itfile = files.begin(); itfile != files.end(); ++itfile )
  {
    unsigned long filesize(0);
    
    std::string filename = *itfile;
    
    std::cout << Form("%40s",filename.c_str());
    
    // ... and dump it for each worker
    for ( ByWorkerMap::const_iterator it = workerSizes.begin();
         it != workerSizes.end(); ++it  )
    {
      std::string worker = it->first;
      
      ByWorkerMap& cmap = workers[worker];
      
      ByWorkerMap::const_iterator ij = cmap.find(filename);
      
      if ( ij == cmap.end())
      {
        std::cout << "         XX |";
      }
      else
      {
        filesize += ij->second;
        std::cout << Form("%8.2f GB |",ij->second/byte2GB);
      }
    }
    
    std::cout << Form("|  %8.2f GB  | %s",filesize/byte2GB,filename.c_str()) << std::endl;
  }
  
  std::cout << std::endl << std::string(120,'-') << std::endl;
  std::cout << Form("%40s"," ");

  for ( ByWorkerMap::const_iterator it = workerSizes.begin();
         it != workerSizes.end(); ++it )
  {
    std::cout << Form("%8.2f GB |",it->second/byte2GB);
  }
  std::cout << std::endl;
    
  delete out;
}