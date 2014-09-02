#ifndef AF_H
#define AF_H

#include "VAF.h"

///
/// Set of utilities to deal with datasets on an ALICE Analysis Facility
///

class AF : public TObject
{
private:
  VAF* fImpl; // implementation object
  
public:
  AF(const char* af="saf");
  virtual ~AF() { delete fImpl; }
  
  Bool_t IsValid() const { return fImpl !=0 ; }
  
  void DryRun(Bool_t flag) { fImpl->DryRun(flag); }
  
  Bool_t DryRun() const { return fImpl->DryRun(); }

  void MergedOnly(Bool_t flag) { fImpl->MergedOnly(flag); }
  
  Bool_t MergedOnly() const { return fImpl->MergedOnly(); }
  
  void PrivateProduction(const char* name, Bool_t simpleRunNumbers=false) { fImpl->PrivateProduction(name,simpleRunNumbers); }

  TString PrivateProduction() const { return fImpl->PrivateProduction(); }

  void CreateDataSets(const std::vector<int>& runs,
                      const char* dataType = "aodmuon",
                      const char* esdpass="pass2",
                      int aodPassNumber=49,
                      const char* basename="/alice/data/2010/LHC10h",
                      Int_t fileLimit=-1)
  { fImpl->CreateDataSets(runs,dataType,esdpass,aodPassNumber,basename,fileLimit); }
  
  void CreateDataSets(const char* runlist = "aod049.list",
                      const char* dataType = "aodmuon",
                      const char* esdpass="pass2",
                      int aodPassNumber=49,
                      const char* basename="/alice/data/2010/LHC10h",
                      Int_t fileLimit=-1)
  { fImpl->CreateDataSets(runlist,dataType,esdpass,aodPassNumber,basename,fileLimit); }
  
  void CreateDataSets(Int_t runNumber,
                      const char* dataType = "aodmuon",
                      const char* esdpass="pass2",
                      int aodPassNumber=49,
                      const char* basename="/alice/data/2010/LHC10h",
                      Int_t fileLimit=-1)
  { fImpl->CreateDataSets(runNumber,dataType,esdpass,aodPassNumber,basename,fileLimit); }

  void AnalyzeFileList(const char* filelist, const char* deletePath="")
  { fImpl->AnalyzeFileList(filelist,deletePath); }

  void ShowDataSetList(const char* path="/*/*/*")
  {
    fImpl->ShowDataSetList(path);
  }

  void GetDataSetList(TList& list)
  {
    fImpl->GetDataSetList(list);
  }
  
  void ShowStagerLog() { fImpl->ShowStagerLog(); }
  
  void ShowXferLog(const char* file) { fImpl->ShowXferLog(file); }

  void ShowXrdDmLog() { fImpl->ShowXrdDmLog(); }

  void ShowPackages() { fImpl->ShowPackages(); }

  void ShowDiskUsage() { fImpl->ShowDiskUsage(); }

  void ClearPackages() { fImpl->ClearPackages(); }

  void ResetRoot() { fImpl->ResetRoot(); }
  
  void Reset(Bool_t hard) { fImpl->Reset(hard); }

  void ShowConfig() { fImpl->ShowConfig(); }
  
  ClassDef(AF,7)
};

#endif
