//
// utility program to query a number of things from an AF, like list of datasets, disk usage,
// log files, conf. files, etc... as well as doing basic operations like reset for instance.
//

#include <iostream>
#include <string>
#include "VAF.h"

//_________________________________________________________________________________________________
int usage() {
 
  std::cout << "Usage : myaf aaf command [detail]" << std::endl;
  std::cout << "- aaf is the nickname of the AAF you want to connect to (must be including in the $HOME/.aaf configuration file)" << std::endl;
  std::cout << "- command is what you want myaf to do : " << std::endl;
  std::cout << "-- reset [hard] : reset the AF" << std::endl;
  std::cout << "-- ds pattern : show the list of dataset(s) matching the pattern" << std::endl;
  std::cout << "-- showds : show the content of one dataset (or several if option is the name of a text file containing the IDs of datasets)"<< std::endl;
  std::cout << "-- clear : clear the list of packages from the user"<< std::endl;
  std::cout << "-- packages : show the list of available packages "<< std::endl;
  std::cout << std::endl;
  std::cout << "-- stagerlog : (advanced) show the logfile of the stager daemon" << std::endl;
  std::cout << "-- df : (advanced) get the disk usage on the AF"<< std::endl;
  std::cout << "-- xferlog filename : (advanced) get the log of a failed transfer "<< std::endl;
  std::cout << "-- conf : (advanced) show the configuration files of the AF "<< std::endl;
  std::cout << "-- xfers : (advanced) show the # of current file transfers the AF "<< std::endl;
  return -1;
}

//_________________________________________________________________________________________________
int main(int argc, char* argv[])
{
  if (argc<2 || argc>5) {
    return usage();
  }
  
  std::string aaf = argv[1];
  
  VAF* af = VAF::Create(aaf.c_str());
  
  if (!af) {
    return -2;
  }
  
  std::string command, option, detail;
  
  if ( argc>2 ) {
    command = argv[2];
  }

  if ( argc>3 ) {
    option = argv[3];
  }

  if ( argc>4 ) {
    detail = argv[4];
  }

  std::cout << "af=" << aaf << " command=" << command << " option=" << option << " detail=" << detail << std::endl;

  if ( command == "stagerlog" ) {
    af->ShowStagerLog();
  }

  if ( command == "xferlog") {
    af->ShowXferLog(option.c_str());
  }
  
  if ( command == "xrddmlog" ) {
    af->ShowXrdDmLog();
  }
  
  if ( command == "packages" ) {
    af->ShowPackages();
  }
  
  if ( command == "df" ) {
    af->ShowDiskUsage();
  }
  
  if  ( command == "clear" ) {
    af->ClearPackages();
  }
  
  if ( command == "resetroot" ) {
    af->ResetRoot();
  }

  if ( command == "reset" ) {
    af->Reset(option.c_str());
  }

  if ( command == "config" ) {
    af->ShowConfig();
  }
  
  if ( command == "ds" ) {
    af->ShowDataSetList(option.c_str());
  }
  
  if ( command == "showds" ) {
    af->ShowDataSetContent(option.c_str());
  }
  
  if ( command == "xfers") {
    af->ShowTransfers();
  }
    
  return 0;
}
