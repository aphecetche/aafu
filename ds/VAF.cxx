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

namespace
{
  Double_t byte2GB(1024*1024*1024);
}

ClassImp(AFFileInfo)

AFFileInfo::AFFileInfo(const char* lsline, const char* prefix) : TNamed()
{
  // decoding here is linked to the stat -c command in the VAF::GetFileMap method (see below)
  TObjArray* a = TString(lsline).Tokenize(" ");
  fMode = static_cast<TObjString*>(a->At(0))->String();
  fUser = static_cast<TObjString*>(a->At(1))->String();
  fGroup = static_cast<TObjString*>(a->At(2))->String();
  fSize = static_cast<TObjString*>(a->At(3))->String().Atoll();
  fTime = TDatime(static_cast<TObjString*>(a->At(4))->String().Atoi());
  TString tmp = static_cast<TObjString*>(a->At(5))->String();
  fFullPath = tmp(strlen(prefix),tmp.Length()-strlen(prefix));
}

std::ostream& operator<<(std::ostream& os, const AFFileInfo& fileinfo)
{
  std::cout << fileinfo.fTime.AsString() << " " << fileinfo.fSize << " " << fileinfo.fFullPath;
  return os;
}

//______________________________________________________________________________
VAF::VAF(const char* master) : fConnect(""), fDryRun(kTRUE), fMergedOnly(kTRUE),
fSimpleRunNumbers(kFALSE), fFilterName(""), fMaster(master), fAnalyzeDeleteScriptName("analyze-delete.sh"),
fHomeDir(""),fLogDir(""), fTreeMapHtmlFileName("treemap.html")
{
  if ( TString(master) != "unknown" )
  {
    fConnect = master;
    fConnect += "/?N";
  }
  
//  std::cout << "Connect string to be used = " << fConnect.Data() << std::endl;
}

//______________________________________________________________________________
void VAF::AnalyzeFileList(const char* deletePath)
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

  TMap m;
  m.SetOwnerKeyValue(kTRUE,kTRUE);
  GetFileMap(m);
  
  std::string worker;
  
  typedef std::map<std::string, unsigned long > ByWorkerMap;
  
  std::map<std::string,ByWorkerMap > workers;
  ByWorkerMap* currentMap(0x0);

  TIter next(&m);
  TObjString* s;
  
  while ( ( s = static_cast<TObjString*>(next()) ) )
  {
    worker = s->String();
    
    workers[worker] = ByWorkerMap();
    currentMap = &(workers[worker]);
    std::cout << "worker = " << worker << std::endl;

    TList* fileInfoList = static_cast<TList*>(m.GetValue(worker.c_str()));
    
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
      if ( ( ( swhat.Contains("_Outer") || swhat.Contains("_Barrel") ) && TString(esdpass) == "cpass1" )
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
void VAF::GetFileInfoList(TList& fileInfoList)
{
  TMap m;
  m.SetOwnerKeyValue(kTRUE,kTRUE);
  GetFileMap(m);
  
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
Int_t VAF::DecodePath(const char* path, TString& period, TString& esdPass, TString& aodPass, Int_t& runNumber) const
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
    
    if ( s.BeginsWith("AOD") )
    {
      aodPass = s;
    }
    
    if ( s.Length() == 9 && s.BeginsWith("000") )
    {
      runNumber = TString(s(3,6)).Atoi();
      rv = TString(path).Index(s.Data());
    }
  }

  if (period.Length()==0 && !TString(path).Contains("/alice/cern.ch/user") )
  {
    // must have a period for anything not user land
    return -1;
  }
  
  if ( esdPass.Length()== 0 && TString(path).Contains("/alice/data") )
  {
    // must find the esd pass for official data !
    return -1;
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
  
  while ( ( fi = static_cast<AFFileInfo*>(next())))
  {
    size += fi->fSize;
  }
  
  return size;
}

//______________________________________________________________________________
void VAF::GenerateHTMLTreeMap()
{
  TList fileInfoList;
  fileInfoList.SetOwner(kTRUE);
  GetFileInfoList(fileInfoList);

  AFFileInfo* fileInfo;
  TIter next(&fileInfoList);
//  Int_t n(10000);
  TMap m;
  
  // first loop to make a map of the paths hierarchy stopping at the depth of the root file - 2
  while ( ( fileInfo = static_cast<AFFileInfo*>(next()) ) /* && n > 0 */)
  {
//    --n;
    
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

  std::ifstream in(Form("%s.template",fTreeMapHtmlFileName.Data()));
  TString text;
  
  text.ReadFile(in);
  
  text.ReplaceAll("INSERTDATAHERE",table.Data());
  
  std::ofstream out(fTreeMapHtmlFileName.Data());
  out << text.Data();
  
}

//______________________________________________________________________________
void VAF::GroupFileList(TMap& groups)
{
  TList fileInfoList;
  fileInfoList.SetOwner(kTRUE);
  GetFileInfoList(fileInfoList);
  
  groups.SetOwnerKeyValue(kTRUE,kTRUE);
  
  AFFileInfo* fileInfo;
  TIter next(&fileInfoList);
  
  while ( ( fileInfo = static_cast<AFFileInfo*>(next()) ) )
  {
    TString file = gSystem->BaseName(fileInfo->fFullPath.Data());
    
    // first group by file type

    AddFileToGroup(groups,file,*fileInfo);
    
    // then broad categories : offical DATA, official SIM, and user land
    if ( fileInfo->fFullPath.BeginsWith("/alice/data") )
    {
      AddFileToGroup(groups,"DATA",*fileInfo);
    }

    if ( fileInfo->fFullPath.BeginsWith("/alice/sim") )
    {
      AddFileToGroup(groups,"SIM",*fileInfo);
    }

    if ( fileInfo->fFullPath.BeginsWith("/alice/cern.ch/user") )
    {
      AddFileToGroup(groups,"USER",*fileInfo);
    }

    // Now group by period / esdPass / aodPass
    
    TString period("");
    TString esdPass("");
    TString aodPass("");
    Int_t runNumber(-1);
    
    Int_t rv = DecodePath(fileInfo->fFullPath.Data(),period,esdPass,aodPass,runNumber);
    
    if (rv<0)
    {
      std::cout << "Could not find period/esdpass/aodpass/runnumber for path " << fileInfo->fFullPath.Data() << std::endl;
      continue;
    }
    
    if ( runNumber > 0 )
    {
      // this is not strictly needed but might help to point to "golden" runs
      AddFileToGroup(groups,Form("%09d",runNumber),*fileInfo);
    }
    
    if ( period.Length() > 0 )
    {
      AddFileToGroup(groups,period,*fileInfo);

      if ( esdPass.Length() > 0 )
      {
        AddFileToGroup(groups,Form("%s/%s",period.Data(),esdPass.Data()),*fileInfo);

        if ( aodPass.Length() > 0 )
        {
          AddFileToGroup(groups,Form("%s/%s/%s",period.Data(),esdPass.Data(),aodPass.Data()),*fileInfo);

        
          AddFileToGroup(groups,Form("%s/%s/%s/%09d",period.Data(),esdPass.Data(),aodPass.Data(),runNumber),*fileInfo);

        }

      }
    }
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
    // FIXME : check the match with path !
    std::cout << dsName->String().Data() << std::endl;
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
    gProof->Exec(Form(".!echo '**************************************'; hostname -a ; cat %s/xrddm.log",LogDir().Data()));
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
    gProof->Exec(Form(".!hostname ; cat %s/xrddm/xrddm.log",LogDir().Data()));
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
void VAF::ShowConfig()
{
   if (Connect("masteronly"))
   {
     gProof->Exec(Form(".!echo '********************* prf-main.cf *************';cat %s/proof/xproofd/prf-main.cf;echo '***************** xrootd.cf ********************';cat %s/xrootd/etc/xrootd/xrootd.cf;echo '***************** afdsmgrd.conf  ********************'; cat %s/proof/xproofd/afdsmgrd.conf",HomeDir().Data(),HomeDir().Data(),HomeDir().Data()));
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
void VAF::GetFileMap(TMap& files)
{
  // Retrieve the complete list of files on this AF.
  // The map is indexed by hostname. For each hostname there a list
  // of fileinfo containing the result of 'ls -l'
  
  if (Connect("workers=1x"))
  {
    TUrl u(gProof->GetDataPoolUrl());
    
    TList* list = gProof->GetListOfSlaveInfos();
    Int_t nworkers = list->GetSize();
    
    for ( Int_t i = 0; i < nworkers; ++i )
    {
      TSlaveInfo* slave = static_cast<TSlaveInfo*>(list->At(i));
      
      TList* fileList = new TList;
      fileList->SetOwner(kTRUE);
      
      files.Add(new TObjString(slave->fHostName),fileList);
      
      gProof->Exec(Form(".! find %s/alice -type f -exec stat -c '%%A %%U %%G %%s %%Y %%n' {} \\;",u.GetFile()),slave->GetOrdinal(),kTRUE);
      
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
        TString tmp(s2->String());
        
        fileList->Add(new AFFileInfo(tmp.Data(),u.GetFile()));
      }
      
      delete l;
    }
  }
}


