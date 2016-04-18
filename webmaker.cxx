#include "AFWebMaker.h"
#include <iostream>
#include "dirent.h"
#include <cstring>

int main(int argc, char* argv[])
{
  std::string topdir;
  std::string prefix("/data");
  std::string pattern("nan");
  int debug(0);

  if ( argc == 1 )
  {
    std::cout << "Usage : webmaker --directory [where to find the files] --pattern [starting part of the filenames to look for] --prefix [prefix to strip from the fullpath of the results of the find command] (--debug) (--debug) (--debug) (--debug)" << std::endl;

  }
  for ( int i = 1; i < argc; ++i)
  {
    if ( !strcmp(argv[i],"--directory") )
    {
      topdir = argv[i+1];
      ++i;
    }

    else if ( !strcmp(argv[i],"--pattern") )
    {
      pattern = argv[i+1];
      ++i;
    }

    else if ( !strcmp(argv[i],"--prefix") )
    {
      prefix = argv[i+1];
      ++i;
    }

    else if ( !strcmp(argv[i],"--debug") )
    {
      debug++;
    }
    else {
      std::cerr << "Unknown option " << argv[i] << std::endl;
    }
  }

  if ( topdir.length() == 0 )
  {
    std::cerr << "No top directory given. Exiting now." << std::endl;
    return -2;
  }

  DIR* dirp = opendir(topdir.c_str());
  if (!dirp)
  {
    std::cerr << "Could not access directory " << topdir << std::endl;
    return -1;
  }

  AFWebMaker wm(topdir,pattern,prefix,debug);

  wm.GenerateReports();

  return 0;
}
