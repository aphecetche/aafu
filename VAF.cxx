#include "VAF.h"

#include "AFWebMaker.h"
#include "Riostream.h"
#include "TClass.h"
#include "TCollection.h"
#include "TDatime.h"
#include "TEnv.h"
#include "TFile.h"
#include "TFileCollection.h"
#include "TFileInfo.h"
#include "TFileMerger.h"
#include "TGrid.h"
#include "TGrid.h"
#include "TGridCollection.h"
#include "TGridResult.h"
#include "THashList.h"
#include "TMap.h"
#include "TMath.h"
#include "TMethodCall.h"
#include "TObjArray.h"
#include "TObjString.h"
#include "TProof.h"
#include "TProof.h"
#include "TString.h"
#include "TSystem.h"
#include "TSystem.h"
#include "TTree.h"
#include "TUrl.h"
#include <algorithm>
#include <cassert>
#include <list>
#include <map>
#include <set>
#include <string>

namespace
{
  Double_t byte2GB(1024*1024*1024);
  Double_t byte2MB(1024*1024);
  
}

//______________________________________________________________________________
VAF::VAF(const char* master) : fConnect(""), fDryRun(kTRUE), fMergedOnly(kTRUE),
fSimpleRunNumbers(kFALSE), fFilterName(""), fMaster(master),
fHomeDir(""),fLogDir(""), fFileTypeToLookFor('f'), fAliPhysics(""), fForceUpdate(kFALSE)
{
  if ( TString(master) != "unknown" )
  {
    fConnect = master;
    fConnect += "/?N";
  }
  
//  std::cout << "Connect string to be used = " << fConnect.Data() << std::endl;
}

//______________________________________________________________________________
Int_t VAF::CheckOneDataSet(const char* dsname)
{
  std::ofstream out("check-one-dataset.sh");
  
  Int_t rv = CheckOneDataSet(dsname,out);
  
  out.close();

  return rv;
}

//______________________________________________________________________________
Int_t VAF::CheckOneDataSet(const char* dsname, std::ofstream& out)
{
  /// Check one data set
  /// The check itself is handled by the TestROOTFile method
  /// Return the number of bad files, and put their names
  /// in the badFiles map
  
  if (!Connect()) return -1;
  
  std::cout << "Testing dataset " << dsname << " ... " << std::endl;
  
  TFileCollection* fc = gProof->GetDataSet(dsname);
  
  if (!fc)
  {
    std::cout << "cannot get dataset " << dsname << std::endl;
    return -2;
  }
  else
  {
    std::cout << "Scanning " << fc->GetList()->GetLast() << " files" << std::endl;
  }
  TIter next(fc->GetList());
  TFileInfo* fi;
  Int_t nbad(0);
  TString treeName(fc->GetDefaultTreeName());
  treeName.ReplaceAll("/","");
  
  while ( ( fi = static_cast<TFileInfo*>(next()) ) )
  {
    TUrl url(*(fi->GetFirstUrl()));
    std::string filename(url.GetUrl());
    
    if (!fi->TestBit(TFileInfo::kStaged) || fi->TestBit(TFileInfo::kCorrupted)) continue;
    
    int rv = TestROOTFile(filename.c_str(),treeName.Data());

    if (rv<0)
    {
      
      out << Form("echo \"xrd %s rm %s\"",url.GetHost(),url.GetFile()) << std::endl;
      
      out << "xrd " << url.GetHost() << " rm " << url.GetFile() << std::endl;      ++nbad;
      
    }
  }
  
  std::cout << "nad=" << nbad << std::endl;
  
  return nbad;
}

//______________________________________________________________________________
void VAF::ClearPackages()
{
  if ( Connect() )
  {
    gProof->ClearPackages();
  }
}

//______________________________________________________________________________
Bool_t VAF::Connect(const char* option)
{
  // FIXME: how to tell, for an existing connection, which option was used ?
  
  if ( gProof ) return kTRUE;
  
  if ( fConnect.Length()==0 ) return kFALSE;
  
  if (!fConnect.Contains("@"))
  {
    std::ifstream in(Form("/tmp/gclient_token_%d",gSystem->GetUid()));
    TString s;
    TString user;
    while (s.ReadLine(in))
    {
      if (s.BeginsWith("User"))
      {
        TObjArray* a = s.Tokenize('=');
        user = static_cast<TObjString*>(a->Last())->String();
        user = user(1,user.Length()-1);
        delete a;
        break;
      }
    }
    if (user.Length())
    {
      TString tmp = fConnect;
      fConnect = user;
      fConnect += "@";
      fConnect += tmp;
    }
  }
  
  TProof::Open(fConnect.Data(),option);
  
  return (gProof != 0x0);
}

//______________________________________________________________________________
void VAF::CompareRunLists(const char* runlist1, const char* runlist2)
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
void VAF::CopyFromRemote(const char* txtfile)
{
  /// Copy a list of remote urls locally, keeping the absolute path
  /// (the destination local directory must exist and be writeable, of course)
  
  char line[1024];
  ifstream in(gSystem->ExpandPathName(txtfile));
  
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
          cout << "Cannot get alien connection" << endl;
          return;
        }
      }
      
    }
  	TString file(url.GetFile());
  	
  	TString dir(gSystem->DirName(file));
  	
  	gSystem->mkdir(dir.Data(),kTRUE);
    
    if ( gSystem->AccessPathName(file.Data())==kFALSE)
    {
      cout << "Skipping copy of " << file.Data() << " as it already exists" << endl;
    }
    else
    {
  	  TFile::Cp(line,file.Data());
      if ( TString(line).EndsWith(".zip") )
      {
        gSystem->Exec(Form("unzip %s -d %s",file.Data(),gSystem->DirName(file.Data())));
        gSystem->Exec(Form("rm %s",file.Data()));
      }
  	}
  }
}

//__________________________________________________________________________________________________
VAF* VAF::Create(const char* af)
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
  Bool_t afDynamic = env.GetValue(Form("%s.dynamic",af),kFALSE);
  
  VAF* vaf(0x0);
  
  if ( afMaster != "unknown" )
  {
    std::string className("AFStatic");
    
    if ( afDynamic )
    {
      className = "AFDynamic";
    }
    
    TClass* c = TClass::GetClass(className.c_str());
    
    if (!c)
    {
      std::cerr << Form("Cannot get class %s",className.c_str()) << std::endl;
      return 0x0;
    }
    
    TMethodCall call;
    
    call.InitWithPrototype(c,className.c_str(),"char*");
    
    Long_t returnLong(0x0);
    
    Long_t params[] = { (Long_t)(&(afMaster.Data()[0])) };
    call.SetParamPtrs((void*)(params));
    call.Execute((void*)(0x0),returnLong);
    
    if (!returnLong)
    {
      std::cout << "Cannot create an AF of class " << className << std::endl;
      return 0x0;
    }
    
    vaf = reinterpret_cast<VAF*> (returnLong);
  }
  else
  {
    std::cerr << "Could not get master information for AF named " << af << std::endl;
  }
  
  if ( vaf )
  {
    TString homeDir = env.GetValue(Form("%s.home",af),"/home/PROOF-AAF");
    TString logDir = env.GetValue(Form("%s.log",af),"/home/PROOF-AAF");
    
    vaf->SetHomeAndLogDir(homeDir.Data(),logDir.Data());
    
    TString fileType = env.GetValue(Form("%s.filetype",af),"f");
    
    vaf->SetFileTypeToLookFor(fileType[0]);
  }
  
  return vaf;
}

//______________________________________________________________________________
void VAF::CreateDataSets(const char* runlist,
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
void VAF::CreateDataSets(Int_t runNumber,
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
TString VAF::DecodeDataType(const char* dataType, TString& what, TString& treeName, TString& anchor, Int_t aodPassNumber) const
{
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
  
  TString sdatatype(dataType);
  sdatatype.ToUpper();
  TString defaultWhat("unknown");
  
  what = defaultWhat;
  
  treeName = "";
  
  anchor="";
  
  if ( sdatatype.Contains("ESD.TAG") )
  {
    what = "*ESD.tag.root";
  }
  else if ( sdatatype.Contains("ESD_OUTER") )
  {
    what = "AliESDs_Outer.root";
    treeName="/esdTree";
  }
  else if ( sdatatype.Contains("ESD_BARREL") )
  {
    what = "AliESDs_Barrel.root";
    treeName="/esdTree";
  }
  else if ( sdatatype.Contains("ESD") )
  {
    what = "AliESDs.root";
    treeName="/esdTree";
  }
  else if ( sdatatype.Contains("AODMU") )
  {
    what = "AliAOD.Muons.root";
    treeName="/aodTree";
  }
  else if ( sdatatype.Contains("AODDIMU") )
  {
    what = "AliAOD.Dimuons.root";
    treeName="/aodTree";
  }
  else if ( sdatatype.Contains("AODHF") )
  {
    what = "AliAOD.VertexingHF.root";
    treeName = "/aodTree";
  }
  else if ( sdatatype.Contains("AODJE") )
  {
    what = "AliAOD.Jets.root";
    treeName = "/aodTree";
  }
  else if ( sdatatype.Contains("AOD") )
  {
    what = "AliAOD.root";
    treeName = "/aodTree";
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
      //      if ( aodPassNumber < 0 )
      //      {
      //        what = "aod_archive.zip";
      //      }
      //      else
      if ( aodPassNumber == 0 )
      {
        what = "aod_archive.zip";
        //        what = "AOD_archive.zip";
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
  
  return sdatatype;
}

//______________________________________________________________________________
void VAF::GetDataSetList(TList& list, const char* path)
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
void VAF::GetDataSetsSize(const char* dsList, Bool_t showDetails)
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
void VAF::GenerateReports()
{
  // Retrieve the complete list of files on this AF for each worker,
  // and delegates the actual report generation to AFWebMaker class
  
  if (!Connect("workers=1x")) return;
  
  TUrl u(gProof->GetDataPoolUrl());
  
  TList* list = gProof->GetListOfSlaveInfos();
  Int_t nworkers = list->GetSize();
  
  std::set<std::string> topdirs;
  
  // to overcome possible limitation in the size of the log file which is used to
  // transmit back the macrolog, we split the request per year where possible,
  // and per "top" directory otherwise
  
  // the /alice/sim directory is not strictly ordered by year...
  // so have to get a full list of the first level below it...
  
  topdirs.insert(Form("%s/alice/cern.ch",u.GetFile()));
  
  TString s;
  
  s += GetStringFromExec(Form(".! find %s/alice/sim -type d -mindepth 1 -maxdepth 1 ",u.GetFile()));
  
  s += GetStringFromExec(Form(".! find %s/alice/data/2010 -type d -mindepth 1 -maxdepth 1 ",u.GetFile()));
  s += GetStringFromExec(Form(".! find %s/alice/data/2011 -type d -mindepth 1 -maxdepth 1 ",u.GetFile()));
  s += GetStringFromExec(Form(".! find %s/alice/data/2012 -type d -mindepth 1 -maxdepth 1 ",u.GetFile()));
  s += GetStringFromExec(Form(".! find %s/alice/data/2013 -type d -mindepth 1 -maxdepth 1 ",u.GetFile()));
  s += GetStringFromExec(Form(".! find %s/alice/data/2014 -type d -mindepth 1 -maxdepth 1 ",u.GetFile()));
  
  //  s += GetStringFromExec(Form(".! find %s/alice/data/2015 -type d -mindepth 1 -maxdepth 1 ",u.GetFile()));
  
  TObjArray* a = s.Tokenize("\n");
  TObjString* os;
  TIter next(a);
  
  while ( ( os = static_cast<TObjString*>(next())) )
  {
    topdirs.insert(os->String().Data());
  }
  
  delete a;
  
  AFWebMaker wm("","",u.GetFile(),0);
  
  for ( Int_t i = 0; i < nworkers; ++i )
  {
    TSlaveInfo* slave = static_cast<TSlaveInfo*>(list->At(i));
    
    std::vector<std::string> lines;
    
    s = "";
    
    for ( std::set<std::string>::const_iterator it = topdirs.begin(); it != topdirs.end(); ++it )
    {
      std::cout << "Looking for files for slave " << slave->fHostName << " " << slave->GetOrdinal()
      << " in directory " << it->c_str() << std::endl;
      
      TString cmd;
      
      cmd.Form(".! find %s -type %c -exec stat -L -c '%%s %%Y %%n' {} \\; | grep -v lock | grep -v LOCK",
               it->c_str(),FileTypeToLookFor());
      
      //      std::cout << cmd.Data() << std::endl;
      
      s += GetStringFromExec(cmd.Data(),slave->GetOrdinal());
    }
    
    TObjArray* b = s.Tokenize("\n");
    TIter next2(b);
    
    while ( ( os = static_cast<TObjString*>(next2())) )
    {
      lines.push_back(os->String().Data());
    }
    
    delete b;
    
    wm.FillFileInfoMap(lines,slave->fHostName.Data());
  }
  
  wm.GenerateReports();
}

//______________________________________________________________________________
TString VAF::GetFileType(const char* path) const
{
  TString file = gSystem->BaseName(path);
  
  if ( file.Contains("FILTER_RAWMUON") )
  {
    file = "FILTER_RAWMUON";
  }
  
  if ( file.BeginsWith("15000") )
  {
    file = "RAW 2015";
  }
  
  if ( file.BeginsWith("14000") )
  {
    file = "RAW 2014";
  }
  
  if ( file.BeginsWith("13000") )
  {
    file = "RAW 2013";
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
  
  return file;
}

//______________________________________________________________________________
void VAF::GetOneDataSetSize(const char* dsname, Int_t& nFiles, Int_t& nCorruptedFiles, Long64_t& size, Bool_t showDetails)
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

//__________________________________________________________________________________________________
void VAF::GetSearchAndBaseName(Int_t runNumber, const char* basename,
                               const char* what,
                               const char* dataType,
                               const char* esdpass,
                               Int_t aodPassNumber,
                               TString& mbasename, TString& search) const
{
  TString sbasename(basename);
  TString swhat(what);
  TString sdatatype(dataType);
  
  mbasename = "";
  search = "";
  
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
    if ( PrivateProduction().Contains("ROBERTA") )
    {
      mbasename += "/1";
    }
    search = swhat.Data();
  }
  else
  {
    if ( sdatatype.Contains("ESD") )
    {
      if ( sbasename.BeginsWith("/alice/sim") )
      {
        mbasename.Form("%s/%d",basename,runNumber);
      }
      else if ( ( ( swhat.Contains("_Outer") || swhat.Contains("_Barrel") ) && TString(esdpass) == "cpass1" )
               || TString(esdpass) == "pass1" )
      {
        mbasename=Form("%s/%09d/%s/*.*",basename,runNumber,esdpass);
      }
      else
      {
        mbasename=Form("%s/%09d/ESDs/%s/*.*",basename,runNumber,esdpass);
      }
      search = swhat.Data();
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
        search = swhat.Data();
      }
      else if (aodPassNumber>=0)
      {
        if ( aodPassNumber==0)
        {
          mbasename = Form("%s/%09d/ESDs/%s/AOD/*",basename,runNumber,esdpass);
        }
        else if ( MergedOnly() )
        {
          mbasename = Form("%s/%09d/%s/AOD%03d/*",basename,runNumber,esdpass,aodPassNumber);
          //            mbasename = Form("%s/%09d/ESDs/%s/AOD%03d/AOD/*",basename,runNumber,esdpass,aodPassNumber);
        }
        else
        {
          mbasename = Form("%s/%09d/ESDs/%s/AOD%03d",basename,runNumber,esdpass,aodPassNumber);
        }
        search = swhat.Data();
      }
      else if ( aodPassNumber == -1 )
      {
        // aods in same dir as ESDs (e.g. LHC11h_HLT)
        mbasename = Form("%s/%09d/ESDs/%s/*.*",basename,runNumber,esdpass);
        search = swhat.Data();
      }
      else if ( aodPassNumber == -2 )
      {
        // aods in their own directory (not under runNumber/ESDs/pass... but directly under runNumber/pass... e.g. LHC12c pass1...)
        mbasename = Form("%s/%09d/%s/AOD/*",basename,runNumber,esdpass);
        search = swhat.Data();        
      }
      else if ( aodPassNumber == -3 )
      {
        // aods in the same directory as the rest, but without "ESDs" in the path
        mbasename = Form("%s/%09d/%s/*",basename,runNumber,esdpass);
        search = swhat.Data();
        
      }

    }
    if ( sdatatype.Contains("RAW") )
    {
      mbasename=Form("%s/%09d/raw",basename,runNumber);
      search = Form("*%09d*.root",runNumber);
    }
  }
  
  mbasename.ReplaceAll("///","/");
  mbasename.ReplaceAll("//","/");
  
  //  cout << Form("basename=%s search=%s",mbasename.Data(),search.Data()) << endl;
}

//______________________________________________________________________________
TString VAF::GetStringFromExec(const char* cmd, const char* ord)
{
  // Execute a command on each Proof worker and get back the result as a (possibly giant)
  // string
  
  TString rv;
  
  if (Connect("workers=1x"))
  {
    gProof->Exec(cmd,ord,kTRUE);
    
    TMacro* macro = gProof->GetMacroLog();
    
    TIter next(macro->GetListOfLines());
    TObjString* s;
    TString fullLine;
    
    while ( ( s = static_cast<TObjString*>(next())) )
    {
      fullLine += s->String();
    }
    
    TObjArray* l = fullLine.Tokenize("\n");
    TObjString* s2;
    TIter next2(l);
    
    while ( ( s2 = static_cast<TObjString*>(next2())) )
    {
      if ( s2->String().Contains("No such file or directory") ) continue;
      rv += s2->String();
      rv += "\n";
    }
  }
  
  return rv;
}

//______________________________________________________________________________
void VAF::GroupDatasets()
{
  if (!Connect()) return;
  
  TList list;
  
  GetDataSetList(list);
  
  GroupDatasets(list);
}

//______________________________________________________________________________
void VAF::GroupDatasets(const TList& dslist)
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
    std::cout << std::endl << std::endl;
  }
  
}

//______________________________________________________________________________
void VAF::GroupDatasets(const char* dslist)
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
void VAF::MergeDataSets(const char* dsList)
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
void VAF::MergeOneDataSet(const char* dsname)
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
void VAF::Print(Option_t* /*opt*/) const
{
  std::cout << ClassName()
  << " talking to " << fMaster.Data() << std::endl
  << " LogDir         :  " << fLogDir.Data() << std::endl
  << " HomeDir        :  " << fHomeDir.Data() << std::endl
  << " FileType       :  " << FileTypeToLookFor() << std::endl
  << " DynamicDataSet : " << fIsDynamicDataSet << std::endl;
  
  if ( fDryRun )
  {
    std::cout << "Currently in dry run mode " << std::endl;
  }
}

//______________________________________________________________________________
void VAF::ReadIntegers(const char* filename, std::vector<int>& integers)
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
void VAF::RemoveDataFromOneDataSet(const char* dsName, std::ofstream& out)
{
  TFileCollection* fc = gProof->GetDataSet(dsName);
  
  TIter next(fc->GetList());
  TFileInfo* fi;
  while ( ( fi = static_cast<TFileInfo*>(next()) ) )
  {
    TUrl url(*(fi->GetFirstUrl()));
    
    if  (TString(url.GetHost()).Contains(fMaster.Data())) continue; // means not staged, so nothing to remove...
    
    out << Form("echo \"xrd %s rm %s\"",url.GetHost(),url.GetFile()) << std::endl;
    
    out << "xrd " << url.GetHost() << " rm " << url.GetFile() << std::endl;
  }
  
}

//______________________________________________________________________________
void VAF::RemoveDataFromOneDataSet(const char* dsName)
{
  if ( fConnect.Length()==0 ) return;
  TProof::Open(fConnect.Data(),"masteronly");
  if (!gProof) return;
  

  std::ofstream out("delete-one-dataset.sh");
  
  RemoveDataFromOneDataSet(dsName,out);
  
  out.close();
}

//______________________________________________________________________________
void VAF::RemoveDataFromDataSetFromFile(const char* dslist)
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
void VAF::RemoveDataSets(const char* dslist)
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
void VAF::Reset(const char* option)
{
  TString soption(option);
  soption.ToUpper();
  Bool_t hard(kFALSE);
  if ( soption.Contains("HARD") )
  {
    hard = kTRUE;
  }
  
  TProof::Reset(fConnect.Data(),hard);
}

//______________________________________________________________________________
void VAF::ResetRoot()
{
  TProof::Mgr(fConnect.Data())->SetROOTVersion("current");
}

//______________________________________________________________________________
void VAF::ShowDataSetContent(const char* dssetfile)
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
void VAF::ShowDataSetContent(const TList& list)
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
void VAF::ShowDataSetList(const char* path)
{
  TList datasets;
  GetDataSetList(datasets); // ,path);

  TIter next(&datasets);
  TObjString* dsName;

  while ((dsName = static_cast<TObjString*>(next())))
  {
    if ( dsName->String().Contains(path) )
    {
      std::cout << dsName->String().Data() << std::endl;
    }
  }
}

//______________________________________________________________________________
void VAF::ShowDataSets(const char* runlist)
{
  // Show all datasets for the runs in run list
  std::vector<int> runs;
  ReadIntegers(runlist,runs);
  if (runs.empty()) return;
  
  if ( Connect() )
  {
    for ( std::vector<int>::size_type i = 0; i < runs.size(); ++i )
    {
      ShowDataSetList(Form("*%d*",runs[i]));
    }
  }
}

//______________________________________________________________________________
void VAF::ShowDiskUsage()
{
  if (Connect("workers=1x"))
  {
    TUrl u(gProof->GetDataPoolUrl());
    
    gProof->Exec(Form(".!hostname ; df -h %s",u.GetFile()));
  }
}

//______________________________________________________________________________
void VAF::ShowPackages()
{
  if (Connect("masteronly"))
  {
    gProof->ShowPackages();
  }
}

//______________________________________________________________________________
void VAF::ShowStagerLog()
{
  /// Dump on screen the log of the stager daemon
  if (Connect("masteronly"))
  {
    gProof->Exec(Form(".!cat %s/afdsmgrd/afdsmgrd.log",LogDir().Data()),"0");
  }
}

//______________________________________________________________________________
void VAF::ShowXferLog(const char* file)
{
  if (Connect("workers=1x"))
  {
//    gProof->Exec(Form(".!hostname ; cat %s/xrootd/xrddm/%s/xrddm_*.log",LogDir().Data(),file));
    gProof->Exec(Form(".!hostname ; cat %s/xrootd/data/%s.anew/saf-stage.log",LogDir().Data(),file));
  }
}

//______________________________________________________________________________
void VAF::ShowXrdDmLog()
{
  if (Connect("workers=1x"))
  {
    gProof->Exec(Form(".!echo '**************************************'; hostname -a ; cat %s/xrootd/xrddm.log",LogDir().Data()));
    gProof->Exec(".!echo '++++++++++++++++++++++++++++++++++++++'; hostname -a ; cat /var/log/xrootd/xrddm.log");
//    gProof->Exec(".!echo '++++++++++++++++++++++++++++++++++++++'; hostname -a ; tail -100 /var/log/xrootd/saf_stage.log");
  }
  
}

//______________________________________________________________________________
void VAF::ShowLog()
{
  if (Connect("workers=1x"))
  {
    gProof->Exec(".!echo '++++++++++++++++++++++++++++++++++++++'; hostname -a ; tail -100 /var/log/xrootd/saf_stage.log");
  }
  
}

//______________________________________________________________________________
void VAF::ShowTransfers()
{
  /// Show CpMacro* transfers
  
  if ( Connect("workers=1x") )
  {
    gProof->Exec(".!hostname ; ps -edf | grep CpMacro | grep root.exe | grep -v ps | grep -v sh | wc -l");
  }
  
}

//______________________________________________________________________________
void VAF::ShowConfig()
{
   if (Connect("workers=1x"))
   {
     std::vector<std::string> files;
     
     files.push_back("%s/proof/xproofd/prf-main.cf");
     files.push_back("%s/xrootd/etc/xrootd/xrootd.cf");
     files.push_back("%s/proof/xproofd/afdsmgrd.conf");
     files.push_back("%s/xrootd/scripts/frm-stage-with-xrddm.sh");
     files.push_back("/usr/local/xrddm/etc/xrddm_env.sh");
     files.push_back("/usr/local/xrddm/etc/xrddm.cfg");
     
     TString cmd(".!");
     
     for (std::vector<std::string>::size_type i = 0; i < files.size(); ++i )
     {
       TString thefile;
       
       thefile.Form(files[i].c_str(),HomeDir().Data());
       
       cmd += "echo '************* ";
       cmd += thefile;
       cmd += " **********' ; cat ";
       cmd += thefile;
       cmd += ";";
     }
     
     gProof->Exec(cmd.Data());
   }
}

//______________________________________________________________________________
int VAF::ReadTree(const char* treename)
{
  TTree* tree = static_cast<TTree*>(gDirectory->Get(treename));
  if (!tree) return -2;
  
  Long64_t nentries = tree->GetEntries();
  
  for (Long64_t i = 0; i < nentries; ++i)
  {
    if ( tree->GetEntry(i) <= 0 ) return -2;
  }
  
  return nentries;
}

//______________________________________________________________________________
void VAF::TestDataSets(const char* txtfile, Bool_t requestStagingIfNotStaged)
{
  /// Check if the datasets in the txtfile are already staged.
  /// If not, and if requestStagingIfNotStaged is true, will launch a staging request
  
  if (!Connect("masteronly")) return;
  
  std::ifstream in(gSystem->ExpandPathName(txtfile));
  std::string line;
  
  std::vector<std::string> msgs;
  std::vector<std::string> requests;
  
  Long64_t totalSize(0);
  
  // loop on text file
  while (std::getline(in,line))
  {
    TFileCollection* fc = gProof->GetDataSet(line.c_str());
    if (!fc)
    {
      msgs.push_back(Form("%s does not exist (query return nothing)",line.c_str()));
      continue;
    }
    
    if ( fc->GetNStagedFiles() == 0 )
    {
      delete fc;
      std::string lineup = line;
      
      // if nothing staged yet, check first if it's not the caching
      // mechanism that is giving us an outdated answer
      lineup += ";ForceUpdate";
      
      fc = gProof->GetDataSet(lineup.c_str());
      
      if ( fc->GetNStagedFiles() == 0 )
      {
        // no, that's not the cache's fault, the files are actually not staged.
        // so they'll need to be requested
        requests.push_back(line.c_str());
      }
    }
    
    totalSize += fc->GetTotalSize();
    
    msgs.push_back(Form("%s staged %4.0f %% nfiles %4lld nstaged %4lld size %6d MB",line.c_str(),fc->GetStagedPercentage(),fc->GetNFiles(),fc->GetNStagedFiles(),TMath::Nint(fc->GetTotalSize()/byte2MB*1.0)));
    
  }

  // printout
  std::vector<std::string>::size_type i;

  for ( i = 0 ; i < msgs.size(); ++i )
  {
    std::cout << msgs[i] << std::endl;
  }

  std::cout << Form("Total size %7.3f GB",totalSize/byte2GB) << std::endl;
  
  if (requests.size())
  {
    if (!requestStagingIfNotStaged)
    {
      std::cout << "The following staging requests would have to be performed" << std::endl;
    }
    
    for ( i = 0 ; i < requests.size(); ++i )
    {
      std::cout << requests[i] << std::endl;
      if (requestStagingIfNotStaged)
      {
        gProof->RequestStagingDataSet(requests[i].c_str());
      }
    }
  }

}

//______________________________________________________________________________
int VAF::TestROOTFile(const char* file, const char* treename)
{
  std::cout << "TestROOTFile " << file << " for tree " << treename << "..." << std::flush;
  Long64_t size(0);
  int rv(-1);
  
  if ( gSystem->AccessPathName(file) == 1 )
  {
    std::cout << " does not exists" << std::endl;
    return 1;
  }
  else
  {
    TFile* f = TFile::Open(file);
    if (!f)
    {
      std::cout << "Cannot open " << file << std::endl;
      return -1;
      
    }
    std::cout << " > " << std::flush;
    size += f->GetSize();
    if ( size > 0 )
    {
      rv = ReadTree(treename);
    }
    f->Close();
    delete f;
  }
  std::cout << Form("%10d entries read successfully",rv) << std::endl;
  
  if (rv<0) std::cout << "TestROOTFile : " << file << " has a problem" << std::endl;
  return rv;
}
