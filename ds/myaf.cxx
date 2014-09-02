//
// utility program to query a number of things from an AF, like list of datasets, disk usage,
// log files, conf. files, etc... as well as doing basic operations like reset for instance.
//
//
// # myaf [aaf] [what] [option] [detail]
//
// # if [what] is ds, option is the regexp of the dataset searched for
// # if [what] is reset, option is true or false (hard or soft reset)
// # if [what] is showds, option is dataset name
// # if [what] is repairds, option is datasetname
// # if [what] is xferlog, option is the filename of the file for which the transfer log should be searched for
// # if [what] is corrupt, option is filename to mark as corrupted and detail is the dataset name
// # if [what] is conf, option and detail are not used and the configuration file of the AAF is shown

//# example : myaf saf ds '*27*'
//# note the '' to protect the regular expression
//#

#include <iostream>
#include <string>
#include "AF.h"

//_________________________________________________________________________________________________
int usage() {
 
  std::cout << "Usage : myaf aaf what option" << std::endl;
  std::cout << "aaf can be caf, saf, saf2, skaf" << std::endl;
  std::cout << "what can be 'ds', 'reset','df', 'clear', 'packages', 'showds', 'repairds', 'lstxt', 'corrupt', 'xferlog', 'synclog', 'conf' or 'stagerlog'" << std::endl;
  return -1;
}

//_________________________________________________________________________________________________
int main(int argc, char* argv[])
{
  if (argc<2 || argc>5) {
    return usage();
  }
  
  std::string aaf = argv[1];
  
  AF af(aaf.c_str());
  
  if (!af.IsValid()) {
    return -2;
  }
  
  std::string what, option, detail;
  
  if ( argc>2 ) {
    what = argv[2];
  }

  if ( argc>3 ) {
    option = argv[3];
  }

  if ( argc>4 ) {
    detail = argv[4];
  }

  std::cout << "af=" << aaf << " what=" << what << " option=" << option << " detail=" << detail << std::endl;

  if ( what == "stagerlog" ) {
    af.ShowStagerLog();
  }

  if ( what == "xferlog") {
    af.ShowXferLog(option.c_str());
  }
  
  if ( what == "xrddmlog" ) {
    af.ShowXrdDmLog();
  }
  
  if ( what == "packages" ) {
    af.ShowPackages();
  }
  
  if ( what == "df" ) {
    af.ShowDiskUsage();
  }
  
  if  ( what == "clear" ) {
    af.ClearPackages();
  }
  
  if ( what == "resetroot" ) {
    af.ResetRoot();
  }

  if ( what == "reset" ) {
    af.Reset(atoi(option.c_str()));
  }

  if ( what == "config" ) {
    af.ShowConfig();
  }
    
  return 0;
}
