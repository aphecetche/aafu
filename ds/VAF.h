#ifndef VAF_H
#define VAF_H

#include <vector>
#include "TObject.h"
#include "TString.h"
class TList;
class TFileCollection;
class TMap;
#include "TNamed.h"
#include "TDatime.h"
#include "Riostream.h"

class AFFileInfo : public TNamed
{
public:
  AFFileInfo(): TNamed(), fSize(0), fTime(), fFullPath("") {}
  AFFileInfo(const char* lsline, const char* prefix, const char* hostname);
  
  friend std::ostream& operator<<(std::ostream& os, const AFFileInfo& fileinfo);
  
  void Print(Option_t* opt="") const { std::cout << (*this) << std::endl; }
  
public:
  Long64_t fSize;
  TDatime fTime;
  TString fFullPath;
  TString fHostName;
  
  ClassDef(AFFileInfo,2)
};


///
/// Interface for class dealing with analysis facility datasets
///

class VAF : public TObject
{
public:
  
  static VAF* Create(const char* af);
  
  VAF(const char* master);
  
  void DryRun(Bool_t flag) { fDryRun = flag; }
  
  Bool_t DryRun() const { return fDryRun; }

  void MergedOnly(Bool_t flag) { fMergedOnly = flag; }
  
  Bool_t MergedOnly() const { return fMergedOnly; }
  
  void PrivateProduction(const char* name, Bool_t simpleRunNumbers=false) { fPrivateProduction = name; fSimpleRunNumbers=simpleRunNumbers; }

  Bool_t IsSimpleRunNumbers() const { return fSimpleRunNumbers; }
  
  Bool_t IsPrivateProduction() const { return fPrivateProduction.Length()>0; }

  TString PrivateProduction() const { return fPrivateProduction; }

  TString FilterName() const { return fFilterName; }
  
  void SetHomeAndLogDir(const char* homedir, const char* logdir) { fHomeDir = homedir; fLogDir = logdir; }
  
  TString HomeDir() const { return fHomeDir; }
  
  TString LogDir() const { return fLogDir; }
  
  void GetFileMap(TMap& files, const char* worker="");
  
  virtual void CreateDataSets(const std::vector<int>& runs,
                              const char* dataType = "aodmuon",
                              const char* esdpass="pass2",
                              int aodPassNumber=49,
                              const char* basename="/alice/data/2010/LHC10h",
                              Int_t fileLimit=-1) = 0;
  
  void CreateDataSets(const char* runlist = "aod049.list",
                      const char* dataType = "aodmuon",
                      const char* esdpass="pass2",
                      int aodPassNumber=49,
                      const char* basename="/alice/data/2010/LHC10h",
                      Int_t fileLimit=-1);
  
  void CreateDataSets(Int_t runNumber,
                      const char* dataType = "aodmuon",
                      const char* esdpass="pass2",
                      int aodPassNumber=49,
                      const char* basename="/alice/data/2010/LHC10h",
                      Int_t fileLimit=-1);

  void GetOneDataSetSize(const char* dsname, Int_t& nFiles, Int_t& nCorruptedFiles, Long64_t& size, Bool_t showDetails=kFALSE);
  void GroupDatasets();

  void GroupDatasets(const TList& list);
  void GroupDatasets(const char* dslist);

  void GetDataSetsSize(const char* dslist, Bool_t showDetails=kFALSE);
  
  void MergeOneDataSet(const char* dsname);
  
  void MergeDataSets(const char* dsList);
  
  void CompareRunLists(const char* runlist1, const char* runlist2);
  
  void RemoveDataFromOneDataSet(const char* dsName, std::ofstream& out);
  void RemoveDataFromOneDataSet(const char* dsName);
  void RemoveDataFromDataSetFromFile(const char* dslist);
  void RemoveDataSets(const char* dslist);
  
  void ShowDataSetContent(const char* dsset);
  void ShowDataSetContent(const TList& dsset);
  
  void ShowDataSets(const char* runlist);
  
  virtual void ShowDataSetList(const char* path="/*/*/*");
  
  void UseFilter(const char* filtername) { fFilterName=filtername; }

  void AnalyzeFileMap(const TMap& fileMap, const char* deletePath="");

  virtual void GetDataSetList(TList& list, const char* path="/*/*/*");
  
  void SetAnalyzeDeleteScriptName(const char* filename) { fAnalyzeDeleteScriptName = filename; }
  
  void ShowStagerLog();
  
  void ShowXrdDmLog();
  
  void ShowPackages();
  
  void ShowDiskUsage();
  
  void ShowXferLog(const char* file);
  
  void ClearPackages();
  
  void ResetRoot();
  
  void Reset(const char* option);
  
  void ShowConfig();
  
  void GetFileInfoList(TList& fileInfoList);
 
  void GetFileInfoListFromMap(TList& fileInfoList, const TMap& m);

  void GroupFileInfoList(const TList& fileInfoList, TMap& groups);
  
  void GenerateHTMLPieChars(const TMap& groups);

  void GenerateHTMLReports();

  void GenerateHTMLReports(const TList& fileInfoList);

  void GenerateHTMLTreeMap(const TList& fileInfoList);

  char FileTypeToLookFor() const { return fFileTypeToLookFor; }

  void SetFileTypeToLookFor(char type) { fFileTypeToLookFor=type;  }

  virtual void Print(Option_t* opt="") const;

  Bool_t Connect(const char* option="masteronly");
  
  void WriteFileInfoList(const char* outputfile="");

  void WriteFileMap(const char* outputfile="");
  
  ULong64_t SumSize(const TList& list) const;
  
  void CopyFromRemote(const char* txtfile="saf.aods.txt") const;

  void ShowTransfers();

protected:
  
  TString GetFileType(const char* path) const;

  TString GetStringFromExec(const char* cmd, const char* ord="*");

  TString DecodeDataType(const char* dataType, TString& what, TString& treeName, TString& anchor, Int_t aodPassNumber) const;

  void GetSearchAndBaseName(Int_t runNumber, const char* sbasename, const char* what, const char* dataType,
                            const char* esdpass, Int_t aodPassNumber,
                            TString& mbasename, TString& search) const;

  void ReadIntegers(const char* filename, std::vector<int>& integers);
  
  Int_t DecodePath(const char* path, TString& period, TString& esdPass, TString& aodPass, Int_t& runNumber, TString& user) const;

  void AddFileToGroup(TMap& groups, const TString& file, const AFFileInfo& fileInfo);

  ULong64_t GenerateASCIIFileList(const char* key, const char* value, const TList& list) const;

  TString GetHtmlPieFileName() const;

  TString GetHtmlTreeMapFileName() const;

protected:
  TString fConnect; // Connect string (afmaster)
  Bool_t fDryRun; // whether to do real things or just show what would be done
  Bool_t fMergedOnly; // pick only the merged AODs when merging is in the same directory as non-merged...
  TString fPrivateProduction; // dataset(s) basename for private productions
  Bool_t fSimpleRunNumbers; // true if run number are not with format %09d but %d instead
  TString fFilterName; // filter name (default:"")
  TString fMaster; // hostname of the master
  TString fAnalyzeDeleteScriptName; // name of the delete script generated (optionaly) by AnalyzeFileList method
  Bool_t fIsDynamicDataSet; // static of dynamic datasets ?
  TString fHomeDir; // home dir of the proof-aaf installation
  TString fLogDir; // log dir of the proof-aaf installation
  TString fTreeMapTemplateHtmlFileName;
  Char_t fFileTypeToLookFor; // file type (f for file or l for link) to look for in GetFileInfoList
  TString fHtmlDir; // directory for HTML report(s)
  
  ClassDef(VAF,8)
};

#endif
