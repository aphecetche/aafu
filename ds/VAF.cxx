#include "VAF.h"

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
#include "TCollection.h"
#include "TObjString.h"
#include "TObjArray.h"
#include "TProof.h"
#include "TMap.h"
#include "TDatime.h"
#include "TFile.h"
#include "TClass.h"
#include "TMethodCall.h"
#include <cassert>
#include "TSystem.h"
#include "TGrid.h"

namespace
{
  Double_t byte2GB(1024*1024*1024);
}

ClassImp(AFFileInfo)

AFFileInfo::AFFileInfo(const char* lsline, const char* prefix, const char* hostname) : TNamed()
{
  // decoding here is linked to the stat -c command in the VAF::GetFileMap method (see below)
  TObjArray* a = TString(lsline).Tokenize(" ");
  fSize = static_cast<TObjString*>(a->At(0))->String().Atoll();
  fTime = TDatime(static_cast<TObjString*>(a->At(1))->String().Atoi());
  TString tmp = static_cast<TObjString*>(a->At(2))->String();
  fFullPath = tmp(strlen(prefix),tmp.Length()-strlen(prefix));
  fHostName = hostname;
}

std::ostream& operator<<(std::ostream& os, const AFFileInfo& fileinfo)
{
  os << fileinfo.fTime.AsString() << " " << fileinfo.fSize << " " << fileinfo.fFullPath << " " << fileinfo.fHostName;
  return os;
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
VAF::VAF(const char* master) : fConnect(""), fDryRun(kTRUE), fMergedOnly(kTRUE),
fSimpleRunNumbers(kFALSE), fFilterName(""), fMaster(master), fAnalyzeDeleteScriptName("analyze-delete.sh"),
fHomeDir(""),fLogDir(""), fTreeMapTemplateHtmlFileName("treemap.html.template"), fFileTypeToLookFor('f'),
fHtmlDir(Form("%s/html",gSystem->pwd()))
{
  if ( TString(master) != "unknown" )
  {
    fConnect = master;
    fConnect += "/?N";
  }
  
//  std::cout << "Connect string to be used = " << fConnect.Data() << std::endl;
}

//______________________________________________________________________________
void VAF::CopyFromRemote(const char* txtfile) const
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
      if ( TString(line).Contains("root_archive.zip") )
      {
        gSystem->Exec(Form("unzip %s -d %s",file.Data(),gSystem->DirName(file.Data())));
        gSystem->Exec(Form("rm %s",file.Data()));
      }
  	}
  }
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
void VAF::AnalyzeFileMap(const TMap& fileMap, const char* deletePath)
{
  ///
  /// Analyse disk space usage of an VAF
  ///
  /// @param deletePath a "xrd rm" command will be created for every file that matches this simple pattern will
  ///  and put in a fAnalyzeDeleteScriptName script
  ///
  
  ofstream* out(0x0);
  
  if (strlen(deletePath) > 0)
  {
    out=new ofstream(fAnalyzeDeleteScriptName.Data());
  }
  
  std::string worker;
  
  typedef std::map<std::string, unsigned long > ByWorkerMap;
  
  std::map<std::string,ByWorkerMap > workers;
  ByWorkerMap* currentMap(0x0);

  TIter next(&fileMap);
  TObjString* s;
  
  while ( ( s = static_cast<TObjString*>(next()) ) )
  {
    worker = s->String();
    
    workers[worker] = ByWorkerMap();
    currentMap = &(workers[worker]);
    std::cout << "worker = " << worker << std::endl;

    TList* fileInfoList = static_cast<TList*>(fileMap.GetValue(worker.c_str()));
    
    TIter nextInfo(fileInfoList);
    AFFileInfo* fi;
    
    while ( ( fi = static_cast<AFFileInfo*>(nextInfo())) )
    {
      TString path = fi->fFullPath;
      
      if ( out && path.Contains(deletePath) )
      {
        int ix = path.Index("/alice");
        
        (*out) << "xrd " << worker << " rm " << path(ix,path.Length()-ix) << std::endl;
      }
      
      TString file = GetFileType(path.Data());
      
      
      unsigned long thisSize = fi->fSize;
      
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
  
  std::cout << "All sizes reported in GB" << std::endl;
  
  std::string hline = std::string(120,'-');
  
  std::cout << Form("| %40s |"," ");
  
  for ( ByWorkerMap::const_iterator it = workerSizes.begin();
       it != workerSizes.end(); ++it )
  {
    TString host(it->first.c_str());
    TObjArray* a = host.Tokenize(".");
    TObjString* h = static_cast<TObjString*>(a->First());
    
    std::cout << Form("%10s |",h->String().Data());
    delete a;
  }
  
  std::cout << " Total     |" << std::endl;
  
  std::cout << "|" << std::string(42,'-') << "|";
  
  for ( ByWorkerMap::const_iterator it = workerSizes.begin();
       it != workerSizes.end(); ++it )
  {
    std::cout << "----------:|";
  }
  
  std::cout << "----------:|";
  
  std::cout << std::endl;
  
  // now loop over files a first time to order them by size
  
  std::map<unsigned int long, std::string> filesBySize;
  
  for ( std::set<std::string>::const_iterator itfile = files.begin(); itfile != files.end(); ++itfile )
  {
    unsigned long filesize(0);
    
    std::string filename = *itfile;
    
    for ( ByWorkerMap::const_iterator it = workerSizes.begin();
         it != workerSizes.end(); ++it  )
    {
      std::string worker = it->first;
      
      ByWorkerMap& cmap = workers[worker];
      
      ByWorkerMap::const_iterator ij = cmap.find(filename);
      
      if ( ij != cmap.end())
      {
        filesize += ij->second;
      }
    }
    
    filesBySize[filesize] = filename;
  }
  
  // now loop over the files a second time, ordered by size
  
  for ( std::map<unsigned int long, std::string>::const_iterator itfile = filesBySize.begin(); itfile != filesBySize.end(); ++itfile )
  {
    unsigned long filesize(0);
    
    std::string filename = itfile->second;
    
    std::cout << Form("| %40s |",filename.c_str());
    
    // ... and dump it for each worker
    for ( ByWorkerMap::const_iterator it = workerSizes.begin();
         it != workerSizes.end(); ++it  )
    {
      std::string worker = it->first;
      
      ByWorkerMap& cmap = workers[worker];
      
      ByWorkerMap::const_iterator ij = cmap.find(filename);
      
      if ( ij == cmap.end())
      {
        std::cout << "        -- |";
      }
      else
      {
        filesize += ij->second;
        std::cout << Form(" %9.2f |",ij->second/byte2GB);
      }
    }
    
    std::cout << Form(" %9.2f |",filesize/byte2GB) << std::endl;
  }
  
  std::cout << Form("| %40s |"," ");
  
  unsigned long totalSize(0);
  
  for ( ByWorkerMap::const_iterator it = workerSizes.begin();
       it != workerSizes.end(); ++it )
  {
    totalSize += it->second;
    
    std::cout << Form(" %9.2f |",it->second/byte2GB);
  }
  
  std::cout << Form(" %9.2f |",totalSize/byte2GB);
  
  std::cout << std::endl;
  
  delete out;
}

//______________________________________________________________________________
Bool_t VAF::Connect(const char* option)
{
  // FIXME: how to tell, for an existing connection, which option was used ?
  
  if ( gProof ) return kTRUE;
  
  if ( fConnect.Length()==0 ) return kFALSE;
  
  TProof::Open(fConnect.Data(),option);
  
  return (gProof != 0x0);
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
void VAF::WriteFileMap(const char* outputfile)
{
  /// Compute and write the file map into a root file
  /// If outputfile is empty, use fMaster.filemap.[date in seconds].root as a name
  
  TString soutputfile(outputfile);
  
  if ( soutputfile.Length() == 0 )
  {
    soutputfile.Form("%s.filemap.%u.root",fMaster.Data(),TDatime().Convert());
  }
  
  TMap m;
  GetFileMap(m);
  TFile* file = TFile::Open(soutputfile.Data(),"recreate");
  m.Write("fileMap",TObject::kSingleKey);
  delete file;
}

//______________________________________________________________________________
void VAF::WriteFileInfoList(const char* outputfile)
{
  /// Compute and write the file list into a root file
  /// If outputfile is empty, use fMaster.fileinfolist.[date in seconds].root as a name

  TString soutputfile(outputfile);
  
  if ( soutputfile.Length() == 0 )
  {
    soutputfile.Form("%s.fileinfolist.%u.root",fMaster.Data(),TDatime().Convert());
  }

  TList fileInfoList;
  GetFileInfoList(fileInfoList);
  TFile* file = TFile::Open(outputfile,"recreate");
  fileInfoList.Write("fileInfoList",TObject::kSingleKey);
  delete file;
}

//______________________________________________________________________________
void VAF::GetFileInfoListFromMap(TList& fileInfoList, const TMap& m)
{
  // Fill the file info from the file map (reverse operation of GroupFileInfoList)
  
  TIter nextWorker(&m);
  TObjString* worker;
  
  while ( ( worker = static_cast<TObjString*>(nextWorker())) )
  {
    TList* files = static_cast<TList*>(m.GetValue(worker->String()));
    TIter next(files);
    AFFileInfo* fileinfo;
    
    while ( ( fileinfo = static_cast<AFFileInfo*>(next())) )
    {
      fileInfoList.Add(new AFFileInfo(*fileinfo));
    }
  }
}

//______________________________________________________________________________
void VAF::GetFileInfoList(TList& fileInfoList)
{
  TMap m;
  m.SetOwnerKeyValue(kTRUE,kTRUE);
  GetFileMap(m);
  
  GetFileInfoListFromMap(fileInfoList,m);
}

//______________________________________________________________________________
Int_t VAF::DecodePath(const char* path, TString& period, TString& esdPass, TString& aodPass,
                      Int_t& runNumber, TString& user) const
{
  TString dir = gSystem->DirName(path);
  
  TObjArray* parts = dir.Tokenize("/");
  
  Int_t rv(-1);
  
  period = "";
  esdPass = "";
  aodPass = "";
  runNumber = -1;
  
  for ( Int_t i = 0; i <= parts->GetLast(); ++i )
  {
    TString s = static_cast<TObjString*>(parts->At(i))->String();

    if ( s.BeginsWith("LHC") )
    {
      period = s;
    }
    
    if ( s.BeginsWith("ESDs") )
    {
      esdPass = static_cast<TObjString*>(parts->At(i+1))->String();
    }
    
    if ( s.BeginsWith("cpass") || s.BeginsWith("vpass") )
    {
      esdPass = s;
    }
    
    if ( s.BeginsWith("AOD") )
    {
      aodPass = s;
    }
    
    if ( s.Length() == 9 && s.BeginsWith("000") )
    {
      runNumber = TString(s(3,6)).Atoi();
      rv = TString(path).Index(s.Data());
    }

    if ( s.Length() == 6 && s.IsDigit() )
    {
      runNumber = s.Atoi();
      rv = TString(path).Index(s.Data());
    }

  }

  if ( dir.BeginsWith("/alice/cern.ch/user") )
  {
    TObjArray* a = dir.Tokenize("/");
    user = static_cast<TObjString*>(a->At(4))->String();
    delete a;
    rv=0;
  }
  
  if (period.Length()==0 && !TString(path).Contains("/alice/cern.ch/user") )
  {
    // must have a period for anything not user land
    rv = -1;
  }
  
  if ( esdPass.Length()== 0 && TString(path).Contains("/alice/data") && !TString(path).Contains("/raw/"))
  {
    if ( aodPass.Length() == 0 )
    {
      // must find the esd pass and/or the aod pass for official data that is not raw data !
      rv = -2;
    }
  }
  
  if ( rv < 0 )
  {
    std::cout << Form("rv=%d path=%s\n period=%s esdpass=%s aodpass=%s runnumber=%d user=%s",rv,
                    path,period.Data(),esdPass.Data(),aodPass.Data(),runNumber,user.Data())
    << std::endl;
  }
  
  return rv;
}

//______________________________________________________________________________
void VAF::AddFileToGroup(TMap& groups, const TString& file, const AFFileInfo& fileInfo)
{
  TList* list = static_cast<TList*>(groups.GetValue(file.Data()));
  if (!list)
  {
    list = new TList;
    list->SetOwner(kTRUE);
    groups.Add(new TObjString(file.Data()),list);
  }
  list->Add(new AFFileInfo(fileInfo));
}

//______________________________________________________________________________
ULong64_t VAF::SumSize(const TList& list) const
{
  /// Get the sum of the sizes (in bytes) of all the files in list
  
  AFFileInfo* fi;
  TIter next(&list);
  ULong64_t size(0);
  ULong64_t m(0);
  AFFileInfo* mfi(0x0);
  
  while ( ( fi = static_cast<AFFileInfo*>(next())))
  {
    if ( fi->fSize > m )
    {
      m = fi->fSize;
      mfi = fi;
    }
    size += fi->fSize;
  }
  
  return size;
}

//______________________________________________________________________________
TString VAF::GetHtmlPieFileName() const
{
  TString name;
  
  name.Form("%s.piecharts.html",fMaster.Data());

  return name;
}

//______________________________________________________________________________
TString VAF::GetHtmlTreeMapFileName() const
{
  TString name;
  
  name.Form("%s.treemap.html",fMaster.Data());
  
  return name;
}

//______________________________________________________________________________
void VAF::GenerateHTMLReports()
{
  std::cout << "Getting fileInfoList..." << std::endl;
  TList fileInfoList;
  GetFileInfoList(fileInfoList);

  std::cout << "done." << std::endl;

  GenerateHTMLReports(fileInfoList);
  
  std::cout << "!!!!!! Remember to copy the af.css file to the " << fHtmlDir.Data()
  << " directory !" << std::endl;
}

//______________________________________________________________________________
void VAF::GenerateHTMLReports(const TList& fileInfoList)
{
  /// Generate HTML reports for this AF, using the file info found in the fileInfoList
  
  // Make the treemap chart
  
  std::cout << "Generating the treemap..." << std::endl;
  GenerateHTMLTreeMap(fileInfoList);
  std::cout << "done." << std::endl;

  TMap g;
  std::cout << "Grouping fileInfoList..." << std::endl;
  GroupFileInfoList(fileInfoList,g);
  std::cout << "done." << std::endl;
  

  std::cout << "Generating pie charts..." << std::endl;
  // Make the pie charts
  GenerateHTMLPieChars(g);
  std::cout << "done." << std::endl;

  // and finally make the index.html
  
  std::ofstream out(Form("%s/index.html",fHtmlDir.Data()));
  
  TString html;
  
  html += "<html>\n";
  html += "<head>\n";
  html += Form("<title>%s</title>\n",fMaster.Data());
  html += "<link rel=\"stylesheet\" type=\"text/css\" href=\"af.css\"/>\n";
  html += "<body>\n";
  html += Form("<h1>Data on %s</h1>\n",fMaster.Data());
  
  html += "<ul>\n";
  
  html += Form("<li><a href=\"%s\">Pie charts of disk usage by file type, data type, pass, etc... </a></li>\n",GetHtmlPieFileName().Data());

  html += Form("<li><a href=\"%s\">Tree map of disk usage</a></li>\n",GetHtmlTreeMapFileName().Data());

  html += "</ul>\n";
  
  html += Form("<p class=\"footer\">Generated on %s</h2>\n",TDatime().AsString());
  
  html += "</body>\n";
  html += "</html>\n";
  
  out << html.Data();
  
  out.close();
}

//______________________________________________________________________________
void VAF::GenerateHTMLTreeMap(const TList& fileInfoList)
{
  AFFileInfo* fileInfo;
  TIter next(&fileInfoList);
  TMap m;
  
  // first loop to make a map of the paths hierarchy stopping at the depth of the root file - 2
  while ( ( fileInfo = static_cast<AFFileInfo*>(next()) ) )
  {
    TString file = gSystem->BaseName(fileInfo->fFullPath.Data());
    
    TObjArray* a = fileInfo->fFullPath.Tokenize("/");
    TString truncatedPath;
    
    for ( Int_t i = 0; i < a->GetLast()-1; ++i )
    {
      truncatedPath += "/";
      truncatedPath += static_cast<TObjString*>(a->At(i))->String();
    }

    TList* list = static_cast<TList*>(m.GetValue(truncatedPath));
    
    if (!list)
    {
      list = new TList;
      list->SetOwner(kTRUE);
      m.Add(new TObjString(truncatedPath),list);
    }
    
    list->Add(new AFFileInfo(*fileInfo));
  }
  
  TIter nextTruncated(&m);
  TObjString* s;

  TString table;

  table = "        ['Location', 'Parent', '(size)', '(color)'],\n";

  // second loop to generate the treemap, starting from the leaves
  TMap parentMap;
  
  while ( ( s = static_cast<TObjString*>(nextTruncated())))
  {
    TString path = s->String();
    
    TList* list = static_cast<TList*>(m.GetValue(path.Data()));
    
    TObjArray* a = path.Tokenize("/");
    
    TString parent;
    
    for ( Int_t i = 0; i <= a->GetLast(); ++i )
    {
      parent += "/";
      parent += static_cast<TObjString*>(a->At(i))->String();
      
      TString node;
      
      for ( Int_t j = 0; j <= i; ++j )
      {
          node += "/";
          node += static_cast<TObjString*>(a->At(j))->String();
      }
      
      TList* alist = static_cast<TList*>(parentMap.GetValue(node.Data()));
      if (!alist)
      {
        alist = new TList;
        alist->SetOwner(kTRUE);
        parentMap.Add(new TObjString(node.Data()),alist);
      }
      TIter next(list);
      AFFileInfo* fi;
      while ( ( fi = static_cast<AFFileInfo*>(next())))
      {
        alist->Add(new AFFileInfo(*fi));
      }
    }
    
    delete a;
  }
  
  TList* list = static_cast<TList*>(parentMap.GetValue("/alice"));
  ULong64_t totalSize = SumSize(*list);
  
  TIter nextParent(&parentMap);
  TObjString* sparent;
  
  while ( ( sparent = static_cast<TObjString*>(nextParent()) ) )
  {
    TString line;

    TString str = sparent->String();
    
    TString shortname = gSystem->BaseName(str.Data());
    TString parent = gSystem->DirName(str.Data());

    TList* list = static_cast<TList*>(parentMap.GetValue(str.Data()));
    
    ULong64_t size = SumSize(*list);
    
    Double_t color = size*1.0 / totalSize;
    
    if (parent=="/")
    {
      line.Form("[ {v:\'%s\',f:\'%s (%5.1f GB)\'},null,%llu,%5.3f],\n",str.Data(),shortname.Data(),size*1.0/byte2GB,size,color);
    }
    else
    {
      line.Form("[ {v:\'%s\',f:\'%s (%5.1f GB)\'},\'%s\',%llu,%5.3f],\n",str.Data(),shortname.Data(),size*1.0/byte2GB,parent.Data(),size,color);
    }
    
    table += line;
  }

  TString html;
  
  html += "<html>\n";
  html += "<head>\n";
  html += Form("<title>%s</title>\n",fMaster.Data());
  html += "<link rel=\"stylesheet\" type=\"text/css\" href=\"af.css\"/>\n";
  html += "<script type=\"text/javascript\" src=\"https://www.google.com/jsapi\"></script>\n";
  html += "<script type=\"text/javascript\">\n";
  html += "google.load(\"visualization\", \"1\", {packages:[\"treemap\"]});\n";
  html += "google.setOnLoadCallback(drawChart);\n";
  html += "function drawChart() {\n";

  html += "var data = google.visualization.arrayToDataTable(\n";
  html += "[\n";
  
  html += table.Data();
  
  html += "]);\n";

  html += "var tree = new google.visualization.TreeMap(document.getElementById('chart_div'));\n";
  
  html += "var options = {\n";
  html += "minColor: '#ffffb2',\n";
  html += "midColor: '#fd8d3c',\n";
  html += "maxColor: '#bd0026',\n";
  html += "showScale: true,\n";
  html += "maxDepth: 1,\n";
  html += "generateTooltip: showSizeTooltip\n";
  html += "};\n";
  
	html += "tree.draw(data,options);\n";
  
  html += "function showSizeTooltip(row,size,value) {\n";
  html += "  var s = size/1024/1024/1024;\n";
  html += "  return '<div style=\"background:#fd9; padding:10px; border-style:solid\">' +\n";
  html += "  data.getValue(row, 0) + ' is ' + s.toFixed(1) + ' GB </div>';\n";
  html += "}\n";
  
  html += "}\n";
  html += "</script>\n";
  html += "</head>\n";

  html += "<body>\n";
  html += "<div id=\"chart_div\" style=\"width: 1200px; height: 600px;\"></div>\n";
  html += "</body>\n";
  html += "</html>\n";

  gSystem->mkdir(fHtmlDir.Data(),true);

  TString outfile;
  
  outfile.Form("%s/%s",fHtmlDir.Data(),GetHtmlTreeMapFileName().Data());
  std::cout << outfile << std::endl;
  
  std::ofstream out(outfile.Data());
  out << html.Data();
  out.close();
}

//______________________________________________________________________________
ULong64_t VAF::GenerateASCIIFileList(const char* key, const char* value, const TList& list) const
{
  TString filename;
  
  gSystem->mkdir(fHtmlDir.Data(),true);
  
  filename.Form("%s/%s.%s.%s.txt",fHtmlDir.Data(),fMaster.Data(),key,value);
  
  ofstream out(filename.Data());
  
  TIter next(&list);
  AFFileInfo* fi;
  
  while ( ( fi = static_cast<AFFileInfo*>(next())) )
  {
    out << (*fi) << std::endl;
  }
  
  out.close();

  FileStat_t buf;

  gSystem->GetPathInfo(filename.Data(),buf);

  return buf.fSize;
}

//______________________________________________________________________________
void VAF::GenerateHTMLPieChars(const TMap& groups)
{
  std::map<std::string,std::string> tables;
  
  TIter nextGroup(&groups);
  TObjString* s;
  
  tables["FILETYPE"] += "[ 'FileType', 'Size' ],\n ";
  tables["PERIOD"] += "[ 'Period', 'Size' ],\n";
  tables["DATATYPE"] += "[ 'Data type', 'Size' ],\n";
  
  tables["USER"] += " ['User', 'Size'],\n";
  tables["ESDPASS"] += " ['Pass', 'Size'],\n";
  tables["AOD"] += " ['AOD', 'Size'],\n";

  TString filesDeclarations = "var filesDeclarations = [";
  
  while ( ( s = static_cast<TObjString*>(nextGroup()) ) )
  {
    TList* list = dynamic_cast<TList*>(groups.GetValue(s->String().Data()));
    
    ULong64_t size = SumSize(*list);
    
    TObjArray* a = s->String().Tokenize(":");
    if ( a->GetSize() < 2 ) continue;
    
    TString key = static_cast<TObjString*>(a->First())->String();
    TString value = static_cast<TObjString*>(a->At(1))->String();
    delete a;
    
    if ( tables.count(key.Data()) )
    {
      tables[key.Data()] += Form("[ '%s', { v:%7.2f, f:'%7.2f GB'} ],\n",value.Data(),size/byte2GB,size/byte2GB);
      ULong64_t fileSize = GenerateASCIIFileList(key.Data(),value.Data(),*list);
      
      TString filename;
      
      filename.Form("%s.%s.%s.txt",fMaster.Data(),key.Data(),value.Data());
      
      filesDeclarations += Form("{ name: '%s', size : %llu, lc : %d },\n",
                               filename.Data(),fileSize,list->GetSize());
    }
  }

  filesDeclarations += "];\n";
  
  TString html;
  
  html += "<html>\n";
  html += "<head>\n";
  html += Form("<title>%s</title>\n",fMaster.Data());
  html += "<link rel=\"stylesheet\" type=\"text/css\" href=\"af.css\"/>\n";
  html += "<script type=\"text/javascript\" src=\"//ajax.googleapis.com/ajax/libs/jquery/1.11.1/jquery.min.js\"></script>\n";
  html += "<script type=\"text/javascript\" src=\"https://www.google.com/jsapi\"></script>\n";
  html += "<script type=\"text/javascript\">\n";
  html += "google.load(\"visualization\", \"1\", {packages:[\"corechart\"]});\n";
  html += "google.setOnLoadCallback(drawChart);\n";
  html += "function drawChart() {\n";
  
  html += filesDeclarations;
  
  for ( std::map<std::string,std::string>::const_iterator it = tables.begin(); it != tables.end(); ++it )
  {
    TObjArray* a = TString(it->second.c_str()).Tokenize("\n");
    if ( a->GetSize() < 2 )
    {
      delete a;
      continue;
    }
    
    delete a;
    
    html += Form("var data%s = google.visualization.arrayToDataTable([\n",it->first.c_str());
    html += it->second.c_str();
    html += "]);\n";
    html += Form("var options%s = { \n",it->first.c_str());
    html += Form("title: 'Disk space by %s',\n",it->first.c_str());
    html += "pieSliceText: 'label' };\n";
    html += Form("var chart%s = new google.visualization.PieChart(document.getElementById('piechart_%s'));\n",
                 it->first.c_str(),it->first.c_str());
    html += Form("chart%s.draw(data%s,options%s);\n",it->first.c_str(),it->first.c_str(),it->first.c_str());
    
    html += Form("function selectHandler%s() {\n",it->first.c_str());
    html += Form("var selectedItem = chart%s.getSelection()[0];\n",it->first.c_str());
    html += "if (selectedItem) {\n";
    html += Form("  var sel = data%s.getValue(selectedItem.row,0);\n",it->first.c_str());

    html += Form("  var filename = '%s.%s.' + data%s.getValue(selectedItem.row,0) + '.txt';\n",
                 fMaster.Data(),it->first.c_str(),
                 it->first.c_str());

    
    html += "showFile(filename);\n";
    html += "}\n";
    html += "}\n";
    
    html += Form("google.visualization.events.addListener(chart%s,'select',selectHandler%s);\n",
                 it->first.c_str(),it->first.c_str());
  }
  
  
  html += " var x = null; var y = null;";
  html += "document.addEventListener('mousemove', onMouseUpdate, false);\n";
  html += "document.addEventListener('mouseenter', onMouseUpdate, false);\n";
  
  html += "function onMouseUpdate(e) { x = e.pageX; y = e.pageY;}\n";
  
  html += "function findObject(a, name) {for (var i = 0; i < a.length; i++) {if (a[i].name === name) { return  a[i];}}return null;}\n";
  
  html += "$(\"#listjumper\").click(function() { $(this).css(\"display\",\"none\"); });\n";
  
  html += "function showFile(filename) {\n";

  html += "var listjumper = $(\"#listjumper\");\n";
    
  html += "var file = findObject(filesDeclarations,filename);\n";
  
  html += "var size = file.size/1024.0/1024;\n";
  
  html += "var msg = '<p>Click the link below to get access to the list of ' + file.lc + ' files in this category</p>';\n";
    
  html += "msg += '<p><a href=\"' + filename + '\">' + filename + '</a> (' + size.toFixed(2) + ' MB text file)</p>';\n";
    
  html += "listjumper.html(msg).css( { display: 'block', top: y , left: x } ) }\n";
  
  html += "} \n";

  html += "</script>\n";
  html += "</head>\n";
  html += "<body>\n";


  html += Form("<div><h1>Disk space usage details on %s</h1></div>\n",fMaster.Data());

  for ( std::map<std::string,std::string>::const_iterator it = tables.begin(); it != tables.end(); ++it )
  {
    html += Form("<div id=\"piechart_%s\" style=\"width: 1200px; height:500px\"></div>\n",
                 it->first.c_str());
  }
  
  html += "<div id=\"listjumper\">-</div>\n";
  
  html += "</body>\n";
  html += "</html>\n";

  gSystem->mkdir(fHtmlDir.Data(),true);
  
  TString outfile;
  
  outfile.Form("%s/%s",fHtmlDir.Data(),GetHtmlPieFileName().Data());
  std::cout << outfile << std::endl;

  std::ofstream out(outfile.Data());
  out << html.Data();

}

//______________________________________________________________________________
void VAF::GroupFileInfoList(const TList& fileInfoList, TMap& groups)
{
  groups.SetOwnerKeyValue(kTRUE,kTRUE);
  
  AFFileInfo* fileInfo;
  TIter next(&fileInfoList);
  
  while ( ( fileInfo = static_cast<AFFileInfo*>(next()) ) )
  {
    TString file = GetFileType(fileInfo->fFullPath.Data());
    
    // first group by file type
    AddFileToGroup(groups,Form("FILETYPE:%s",file.Data()),*fileInfo);
    
    // then broad categories : offical DATA, official SIM, and user land
    if ( fileInfo->fFullPath.BeginsWith("/alice/data") )
    {
      AddFileToGroup(groups,"DATATYPE:DATA",*fileInfo);
    }

    if ( fileInfo->fFullPath.BeginsWith("/alice/sim") )
    {
      AddFileToGroup(groups,"DATATYPE:SIM",*fileInfo);
    }

    if ( fileInfo->fFullPath.BeginsWith("/alice/cern.ch/user") )
    {
      AddFileToGroup(groups,"DATATYPE:USER",*fileInfo);
    }

    // Now group by period / esdPass / aodPass
    
    TString period("");
    TString esdPass("");
    TString aodPass("");
    Int_t runNumber(-1);
    TString user("");
    
    Int_t rv = DecodePath(fileInfo->fFullPath.Data(),period,esdPass,aodPass,runNumber,user);
    
    if (rv<0)
    {
      std::cout << "Could not find period/esdpass/aodpass/runnumber for path " << fileInfo->fFullPath.Data() << std::endl;
      return;//FIXME: remove this which is just for debug !
      continue;
    }
    
    if ( user.Length() > 0 )
    {
      AddFileToGroup(groups,Form("USER:%s",user.Data()),*fileInfo);

    }
    
    if ( runNumber > 0 )
    {
      // this is not strictly needed but might help to point to "golden" runs
      AddFileToGroup(groups,Form("RUN:%09d",runNumber),*fileInfo);
    }
    
    if ( period.Length() > 0 )
    {
      AddFileToGroup(groups,Form("PERIOD:%s",period.Data()),*fileInfo);

      if ( esdPass.Length() > 0 )
      {
        AddFileToGroup(groups,Form("ESDPASS:%s_%s",period.Data(),esdPass.Data()),*fileInfo);

        if ( aodPass.Length() > 0 )
        {
          AddFileToGroup(groups,Form("AOD:%s_%s_%s",period.Data(),esdPass.Data(),aodPass.Data()),*fileInfo);

        
          AddFileToGroup(groups,Form("DS:%s_%s_%s_%09d",period.Data(),esdPass.Data(),aodPass.Data(),runNumber),*fileInfo);

        }
      }
      else {
        if ( aodPass.Length() > 0 )
        {
          AddFileToGroup(groups,Form("AOD:%s_%s",period.Data(),aodPass.Data()),*fileInfo);
          
          
          AddFileToGroup(groups,Form("DS:%s_%s_%09d",period.Data(),aodPass.Data(),runNumber),*fileInfo);
          
        }
        
      }
    }
  }
  
  TObjArray keys;
  
  TIter nextGroup(&groups);
  TObjString* s;
  
  while ( ( s = static_cast<TObjString*>(nextGroup()) ) )
  {
    keys.Add(new TObjString(*s));
  }
  
  keys.Sort();
  
  TIter nextSortedGroup(&keys);
  
  while ( ( s = static_cast<TObjString*>(nextSortedGroup()) ) )
  {
    TList* list = dynamic_cast<TList*>(groups.GetValue(s->String().Data()));
    assert (list!=0);
    ULong64_t size = SumSize(*list);
    std::cout << Form("%-50s has %6d files for a total of %7.2f GB",s->String().Data(),list->GetSize(), size/byte2GB) << std::endl;
  }
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
void VAF::GroupDatasets()
{
  if (!Connect()) return;
  
  TList list;
  
  GetDataSetList(list);
  
  GroupDatasets(list);
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
void VAF::ShowStagerLog()
{
  /// Dump on screen the log of the stager daemon
  if (Connect("masteronly"))
  {
    gProof->Exec(Form(".!cat %s/afdsmgrd/afdsmgrd.log",LogDir().Data()),"0");
  }
}

//______________________________________________________________________________
void VAF::ShowXrdDmLog()
{
  if (Connect("workers=1x"))
  {
    gProof->Exec(Form(".!echo '**************************************'; hostname -a ; cat %s/xrootd/xrddm.log",LogDir().Data()));
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
void VAF::ShowDiskUsage()
{
  if (Connect("workers=1x"))
  {
    TUrl u(gProof->GetDataPoolUrl());
    
    gProof->Exec(Form(".!hostname ; df -h %s",u.GetFile()));
  }
}

//______________________________________________________________________________
void VAF::ShowXferLog(const char* file)
{
  if (Connect("workers=1x"))
  {
    gProof->Exec(Form(".!hostname ; cat %s/xrootd/xrddm/%s/xrddm_*.log",LogDir().Data(),file));
  }
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
void VAF::ResetRoot()
{
  TProof::Mgr(fConnect.Data())->SetROOTVersion("current");
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
void VAF::GetFileMap(TMap& files, const char* worker)
{
  // Retrieve the complete list of files on this AF.
  // The map is indexed by hostname. For each hostname there a list
  // of fileinfo containing the result of 'ls -l'
  
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
  
  for ( Int_t i = 0; i < nworkers; ++i )
  {
    TSlaveInfo* slave = static_cast<TSlaveInfo*>(list->At(i));
    
    if (strlen(worker) && slave->fHostName != worker ) continue;
    
    TList* fileList = new TList;
    fileList->SetOwner(kTRUE);
    
    files.Add(new TObjString(slave->fHostName),fileList);
    
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
      TString tmp(os->String());
      
      AFFileInfo* fi = new AFFileInfo(tmp.Data(),u.GetFile(),slave->fHostName);
      if ( fi->fSize > 0 )
      {
        fileList->Add(fi);
      }
      else
      {
        delete fi;
      }
    }
    
    delete b;
  }
  
}

//______________________________________________________________________________
void VAF::Print(Option_t* opt) const
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
