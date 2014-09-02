#ifndef AFDYNAMIC_H
#define AFDYNAMIC_H

#include "VAF.h"
#include <string>

///
/// Set of utilities to deal with datasets ("new style", i.e. TDataSetManagerAlien) on an
/// ALICE Analysis Facility
///

class AFDynamic : public VAF
{
public:
  AFDynamic(const char* user, const char* master);
  
public:
  
  virtual void CreateDataSets(const std::vector<int>& runs,
                      const char* dataType = "aodmuon",
                      const char* esdpass="pass2",
                      int aodPassNumber=49,
                      const char* basename="/alice/data/2010/LHC10h",
                      Int_t fileLimit=-1);

  virtual void GetDataSetList(TList& list);

private:

  Bool_t DecodeSearchId(const char* searchId,
                        TString& basepath, TString& filename, TString& anchor,
                        Bool_t& isarchive, TString& treename, TString& regexp) const;

  TString CreateQueryString(Int_t runNumber,
                            const char* dataType,
                            const char* esdpass,
                            int aodPassNumber,
                            const char* basename) const;

  TString SearchId(const char* basepath, const char* filename, const char* anchor,
                   Bool_t isarchive, const char* treename, const char* regexp) const;

  TString ExtractKeyWord(const TString& search, const char* keyword, std::vector<std::string>& keywords) const;

  ClassDef(AFDynamic,1)
};

#endif
