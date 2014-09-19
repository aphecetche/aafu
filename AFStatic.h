#ifndef AFSTATIC_H
#define AFSTATIC_H

#include "VAF.h"

///
/// Set of utilities to deal with datasets ("old style", i.e. TDataSetManagerFile) on an
/// ALICE Analysis Facility
///

void MakeRawCollection(const char* runlist="runList.txt", 
                       const char* collectionName="toto", 
                       const char* collectionScript="makeesdcollection.sh");

class AFStatic : public VAF
{
public:
  AFStatic(const char* master);
  
public:
  
  virtual void CreateDataSets(const std::vector<int>& runs,
                      const char* dataType = "aodmuon",
                      const char* esdpass="pass2",
                      int aodPassNumber=49,
                      const char* basename="/alice/data/2010/LHC10h",
                      Int_t fileLimit=-1);
  
private:

  TFileCollection* CreateCollectionFromRunList(const char* collectionType,
                                               const std::vector<int> runs,
                                               const char* dataType,
                                               const char* esdpass="pass1_plusplusplus",
                                               int aodPassNumber=1,
                                               const char* basename="/alice/data/2010/LHC10e",
                                               bool computeTotalSize=true,
                                               Int_t fileLimit=-1);
  
  TFileCollection* CreateCollectionFromRunList(const char* collectionType,
                                               const char* runlist,
                                               const char* dataType,
                                               const char* esdpass="pass1_plusplusplus",
                                               int aodPassNumber=1,
                                               const char* basename="/alice/data/2010/LHC10e",
                                               bool computeTotalSize=true,
                                               Int_t fileLimit=-1);
  
  TFileCollection* CreateCollectionFromRunList(const char* collectionType,
                                               Int_t runNumber,
                                               const char* dataType,
                                               const char* esdpass="pass1_plusplusplus",
                                               int aodPassNumber=1,
                                               const char* basename="/alice/data/2010/LHC10e",
                                               bool computeTotalSize=true,
                                               Int_t fileLimit=-1);
  
  ClassDef(AFStatic,1)
};

#endif
