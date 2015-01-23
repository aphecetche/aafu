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
#include <map>
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

  Int_t CheckOneDataSet(const char* dsname, std::ofstream& out);
  Int_t CheckOneDataSet(const char* dsname);

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

  virtual void GetDataSetList(TList& list, const char* path="/*/*/*");
  
  void ShowStagerLog();
  
  void ShowXrdDmLog();
  
  void ShowLog();
  
  void ShowPackages();
  
  void ShowDiskUsage();
  
  void ShowXferLog(const char* file);
  
  void ClearPackages();
  
  void ResetRoot();
  
  void Reset(const char* option);
  
  void ShowConfig();
  
  char FileTypeToLookFor() const { return fFileTypeToLookFor; }

  void SetFileTypeToLookFor(char type) { fFileTypeToLookFor=type;  }

  virtual void Print(Option_t* opt="") const;

  Bool_t Connect(const char* option="masteronly");
  
  static void CopyFromRemote(const char* txtfile="saf.aods.txt");

  void ShowTransfers();

  void GenerateReports();
  
protected:

  TString DecodeDataType(const char* dataType, TString& what, TString& treeName, TString& anchor, Int_t aodPassNumber) const;

  TString GetFileType(const char* path) const;

  TString GetStringFromExec(const char* cmd, const char* ord="*");

  void GetSearchAndBaseName(Int_t runNumber, const char* sbasename, const char* what, const char* dataType,
                            const char* esdpass, Int_t aodPassNumber,
                            TString& mbasename, TString& search) const;

  void ReadIntegers(const char* filename, std::vector<int>& integers);
  
  int ReadTree(const char* treename);
  
  int TestROOTFile(const char* file, const char* treename);
  
protected:
  TString fConnect; // Connect string (afmaster)
  Bool_t fDryRun; // whether to do real things or just show what would be done
  Bool_t fMergedOnly; // pick only the merged AODs when merging is in the same directory as non-merged...
  TString fPrivateProduction; // dataset(s) basename for private productions
  Bool_t fSimpleRunNumbers; // true if run number are not with format %09d but %d instead
  TString fFilterName; // filter name (default:"")
  TString fMaster; // hostname of the master
  Bool_t fIsDynamicDataSet; // static of dynamic datasets ?
  TString fHomeDir; // home dir of the proof-aaf installation
  TString fLogDir; // log dir of the proof-aaf installation
  Char_t fFileTypeToLookFor; // file type (f for file or l for link) to look for in GenerateReports
  
  ClassDef(VAF,9)
};

#endif
