#include "AFDynamic.h"

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
#include "TMacro.h"
#include "TDatime.h"
#include <cassert>

namespace
{
  Double_t byte2GB(1024*1024*1024);
}

//______________________________________________________________________________
AFDynamic::AFDynamic(const char* user, const char* master) : VAF(user,master)
{
}

//______________________________________________________________________________
TString AFDynamic::CreateQueryString(Int_t runNumber,
                                     const char* dataType,
                                     const char* esdpass,
                                     int aodPassNumber,
                                     const char* basename) const
{
  TString query;
  
  TString what;
  TString treeName;
  TString anchor;
  
  TString sdatatype = DecodeDataType(dataType,what,treeName,anchor,aodPassNumber);
  
  TString search;
  TString mbasename;
  
  GetSearchAndBaseName(runNumber,basename, what, sdatatype.Data(), esdpass, aodPassNumber,mbasename, search);
  
  query.Form("Find;BasePath=%s;FileName=%s",mbasename.Data(),search.Data());
  
  if ( treeName.Length() > 0 )
  {
    query += ";Tree=";
    query += treeName;
  }
  
  if ( anchor.Length() > 0 )
  {
    query += ";Anchor=";
    query += anchor;
  }

  return query;
}

//______________________________________________________________________________
void AFDynamic::CreateDataSets(const std::vector<int>& runs,
                        const char* dataType,
                        const char* esdpass,
                        int aodPassNumber,
                        const char* basename,
                        Int_t fileLimit)
{
  // create one dataset per run
  
  if ( !Connect()) return;
  
  if ( fileLimit > 0 )
  {
    std::cout << __FILE__ << " - WARNING : fileLimit parameter not supported (yet?) by dynamic datasets. Will ignore it." << std::endl;
  }
  
  TList list;
  TFileCollection total;
  
  for ( std::vector<int>::size_type i = 0; i < runs.size(); ++i )
  {
    TString query = CreateQueryString(runs[i],dataType,esdpass,aodPassNumber,basename);
  
    std::cout << query.Data() << std::endl;
  
    TFileCollection* fc = gProof->GetDataSet(query.Data());
  
    if ( fc )
    {
      fc->Print();
      list.Add(fc);
      total.SetDefaultTreeName(fc->GetDefaultTreeName());
    }
    
    if (!DryRun())
    {
      gProof->RequestStagingDataSet(query.Data());
    }
  }

  total.Merge(&list);

  total.Print();
  
  Int_t nfiles = total.GetNFiles();
  Long64_t size = total.GetTotalSize();
  
  cout << Form("nruns=%d nfiles = %d | size = %7.2f GB",(Int_t)runs.size(),nfiles,size/byte2GB) << endl;
}

//______________________________________________________________________________
Bool_t AFDynamic::DecodeSearchId(const char* searchId,
                                 TString& basepath, TString& filename, TString& anchor,
                                 Bool_t& isarchive, TString& treename, TString& regexp) const
{
  //  Find_FileName_AliAOD.Muons.root_BasePath__alice_data_2011_LHC11h_000169838_ESDs_pass2_muon_AOD119
  
  Bool_t ok(kFALSE);
  
  basepath="";
  filename="";
  anchor="";
  treename="";
  regexp="";
  
  isarchive = kFALSE;
  
  TString search(searchId);
  
  if (!search.BeginsWith("Find"))
  {
    std::cerr << "Dont know yet how to work with string starting by anything but Find" << std::endl;
    return kFALSE;
  }
  
  search.ReplaceAll("Find","");
  
  std::vector<std::string> keywords;
  
  keywords.push_back("_FileName_");
  keywords.push_back("_BasePath_");
  keywords.push_back("_Anchor_");
  keywords.push_back("_Tree_");
  keywords.push_back("_Regexp_");

  filename = ExtractKeyWord(search,"_FileName_",keywords);
  basepath = ExtractKeyWord(search,"_BasePath_",keywords);
  anchor = ExtractKeyWord(search,"_Anchor_",keywords);
  treename = ExtractKeyWord(search,"_Tree_",keywords);
  regexp = ExtractKeyWord(search,"_Regexp_",keywords);
  
  if ( filename.Length() > 0 && basepath.Length() > 0 )
  {
    ok = kTRUE;
  }
  
  return ok;
}

//______________________________________________________________________________
TString
AFDynamic::ExtractKeyWord(const TString& search, const char* keyword, std::vector<std::string>& keywords) const
{
  Ssiz_t index = search.Index(keyword);
  
  if (index != kNPOS )
  {
    // find the end of the value for that keyword (=end of string or next keyword)
    Int_t end(search.Length()-1);
    
    for ( std::vector<std::string>::size_type i = 0; i < keywords.size(); ++i )
    {
      if ( keywords[i] == keyword ) continue;
      
      Ssiz_t x = search.Index(keywords[i],keywords[i].size(),index+TString(keyword).Length(),TString::kIgnoreCase);
      
      if(  x != kNPOS )
      {
        if ( x - 1 < end )
        {
          end = x-1;
        }
      }
    }
    
    return search(index+TString(keyword).Length(),end-index-TString(keyword).Length()+1);
  }

  return "";
}

//______________________________________________________________________________
TString
AFDynamic::SearchId(const char* basepath, const char* filename, const char* anchor,
                    Bool_t isarchive, const char* treename, const char* regexp) const
{
  /// Inspired from TAliEnFind::GetSearchId(). Of course if it changes there,
  /// we have a problem...
  ///
  
  TString searchIdStr;
  
  searchIdStr.Form("BasePath=%s FileName=%s Anchor=%s ArchSubst=%d "
                     "TreeName=%s Regexp=%s",basepath,filename,anchor,isarchive,
                   treename,regexp);
  TMD5 md5;// = new TMD5();
  
  md5.Update( (const UChar_t *)searchIdStr.Data(),
                (UInt_t)searchIdStr.Length() );
  md5.Final();

  return md5.AsString();
}

//______________________________________________________________________________
void AFDynamic::GetDataSetList(TList& list)
{
  /// Get (an approximate) list of datasets. Due to the way dynamic datasets are handled,
  /// we have no guarantee the list returned is the full one (i.e. if some adming
  /// wiped out the dataset.list file, we're screwed), but that should be good enough
  /// for most purposes...
  
  TString datasetList(HomeDir());
  
  datasetList += "/stagereqrepo/dataset.list"; // FIXME : the stagereqrepo should be found in the xrootd config...
  
  if ( !Connect()) return;

  TString cmd(".! cat ");
  
  cmd += datasetList;
  
  gProof->Exec(cmd.Data(),"0",kTRUE);
  
  TMacro* macro = gProof->GetMacroLog();
  
  TList* macroLines = macro->GetListOfLines();
  
  assert(macroLines->GetSize()==1);
 
  TString singleLine = static_cast<TObjString*>(macroLines->First())->String();
  TObjArray* lines = singleLine.Tokenize("\n");
  TIter next(lines);
  TObjString* s;
  
  while ( ( ( s = static_cast<TObjString*>(next())) ) )
  {
    TObjArray* parts = s->String().Tokenize(" ");
    TDatime d(static_cast<TObjString*>(parts->At(0))->String().Atoi());
    TString path(static_cast<TObjString*>(parts->At(1))->String());
    
//    TString hash(static_cast<TObjString*>(parts->At(2))->String());
    
    TString basepath;
    TString filename;
    TString anchor;
    Bool_t isarchive(kFALSE);
    TString treename;
    TString regexp;
    
    path = gSystem->BaseName(path.Data());
    
    path.ReplaceAll("____","_");
    path.ReplaceAll("___","_");
    path.ReplaceAll("__","_");
    
    Bool_t decodeIsOK = DecodeSearchId(path.Data(),
                                       basepath, filename, anchor,
                                       isarchive, treename, regexp);


    basepath.ReplaceAll("_","/");
//    basepath.ReplaceAll("muon/pass2","muon_pass2");
    
//    if ( basepath[0] != '/' )
//    {
//      TString tmp(basepath);
//      basepath = "/";
//      basepath += tmp;
//    }

    TString hash = SearchId(basepath.Data(),filename.Data(),anchor.Data(),
                            isarchive,treename.Data(),regexp.Data());
    
    std::cout << path.Data()
    << std::endl
     << " + ok=" << decodeIsOK << " + basepath=" << basepath.Data() << " + filename=" << filename.Data() <<
    " + tree=" << treename << " + anchor=" << anchor
    << std::endl
    << " recomposed hash = " << hash.Data() << std::endl;
    
    gProof->Exec(Form(".!ls %s/datasetcache/_cache_/_cache_/*%s*",HomeDir().Data(),hash.Data()));
    
    delete parts;
  }
  
  delete lines;
}

