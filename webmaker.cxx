#include "AFWebMaker.h"

int main(int argc, char* argv[])
{
  std::string topdir;
  std::string prefix("/data");
  std::string pattern("nan");
  std::string mdfile;
  
  if (argc>1) {
    topdir = argv[1];
  }

  if (argc>2) {
    pattern = argv[2];
  }

  if (argc>3) {
    prefix = argv[3];
  }

  if (argc>4) {
    mdfile = argv[4];
  }

  AFWebMaker wm(topdir,pattern,prefix);

  wm.GenerateReports();
  
  if ( mdfile.size() > 0 )
  {
    wm.GenerateUserManual(mdfile);
  }
  
  return 0;
}