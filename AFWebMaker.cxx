#include "AFWebMaker.h"
#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>
#include "dirent.h"
#include <unistd.h>
#include <cassert>
#include <sys/stat.h>
#include <cstring>
#include <cstdlib>
#include "boost/algorithm/string/trim.hpp"

int AFWebMaker::fgDebugLevel = 0;

#define WARNING() std::cout << "WARNING " << __FILE__ << ":" << __LINE__ << " "

#define ERROR() std::cerr << "ERROR " << __FILE__ << ":" << __LINE__ << " "

#define DEBUG(level) if ( fDebugLevel > level ) std::cout <<  "DEBUG " << __FILE__ << ":" << __LINE__ << " "

#define GDEBUG(level) if ( AFWebMaker::fgDebugLevel > level ) std::cout <<  "GDEBUG " << __FILE__ << ":" << __LINE__ << " "

namespace {

  double byte2GB(1024*1024*1024);

  void Tokenize(const std::string& str, std::vector<std::string>& tokens, char delim)
  {
    tokens.clear();

    std::istringstream in(str);
    std::string tok;

    while ( getline(in,tok,delim) )
    {
      if ( tok.size()  > 0 )
      {
        tokens.push_back(tok);
      }
    }
  }

  void AddList(const AFWebMaker::AFFileInfoList& src, AFWebMaker::AFFileInfoList& dest)
  {
    for ( AFWebMaker::AFFileInfoList::const_iterator it = src.begin(); it != src.end(); ++it )
    {
      dest.push_back((*it));
    }
  }

  bool BeginsWith(const std::string& str, const std::string& begin)
  {
    return !strncmp(str.c_str(),begin.c_str(),begin.size());
  }

  std::string BaseName(const std::string& str)
  {
    return str.substr(str.find_last_of('/')+1);
  }

  std::string DirName(const std::string& str)
  {
    return str.substr(0,str.find_last_of('/'));
  }

}

//_________________________________________________________________________________________________
AFWebMaker::AFFileInfo::AFFileInfo(const std::string& lsline, const std::string& prefix, const std::string& hostname)
{
  // decoding here is linked to the stat -c command in the VAF::GetFileMap method (see below)
  std::vector<std::string> tokens;

  Tokenize(lsline,tokens,' ');

  const std::string ssize = tokens[0];
  const std::string stime = tokens[1];
  const std::string spath = tokens[2];

  fSize = atol(ssize.c_str());
  fTime = atoi(stime.c_str());

  fFullPath = spath.substr(prefix.size(),spath.size() - prefix.size());
  fHostName = hostname;
}

//_________________________________________________________________________________________________
std::string AFWebMaker::AFFileInfo::Basename() const
{
  return ::BaseName(fFullPath);
}

//_________________________________________________________________________________________________
bool AFWebMaker::AFFileInfo::BeginsWith(const std::string& str) const
{
  return ::BeginsWith(fFullPath,str);
}

//_________________________________________________________________________________________________
std::ostream& operator<<(std::ostream& os, const AFWebMaker::AFFileInfo& fileinfo)
{
  std::tm* ptm = std::localtime(&fileinfo.fTime);
  char buffer[32];
  std::strftime(buffer,32,"%a, %d.%m.%Y %H:%M:%S",ptm);

  os << buffer << " " << fileinfo.fSize << " " << fileinfo.fFullPath << " " << fileinfo.fHostName;

  return os;
}

//_________________________________________________________________________________________________
//
//
//
//_________________________________________________________________________________________________

//_________________________________________________________________________________________________
AFWebMaker::AFWebMaker(const std::string& topdir, const std::string& pattern,
                       const std::string& prefix, int debuglevel) :
fTopDir(topdir), fFileListPattern(pattern), fPrefix(prefix), fDebugLevel(debuglevel)
{
  char hostname[1024];

  gethostname(hostname,1023);

  fHostName = hostname;

  DEBUG(0) << "(on host " << fHostName << ") : Will look for files starting with " << fFileListPattern
    << " in directory " << fTopDir << " and will strip " << fPrefix << " from the paths found in those files" << std::endl;

  if (fPrefix.empty())
  {
    fPrefix = "/";
  }
  else
  {
    // normalize prefix to be of the form "/prefix"
    boost::trim(fPrefix);
    boost::trim_if(fPrefix,boost::algorithm::is_any_of("/"));
    fPrefix.insert(0,"/");
  }
}

//_________________________________________________________________________________________________
AFWebMaker::~AFWebMaker()
{
  for ( AFFileInfoMap::iterator it = fGroupMap.begin(); it != fGroupMap.end(); ++it )
  {
    delete it->second;
    it->second = 0;
  }

  for ( AFFileInfoMap::iterator it = fFileInfoMap.begin(); it != fFileInfoMap.end(); ++it )
  {
    delete it->second;
    it->second = 0;
  }

}

//_________________________________________________________________________________________________
void AFWebMaker::AddFileToGroup(const std::string& file, const AFWebMaker::AFFileInfo& fileInfo)
{
  AFFileInfoList* list = 0x0;

  if ( !fGroupMap.count(file) )
  {
    DEBUG(3) << " Creating new list for file " << file << std::endl;
    list = new AFFileInfoList;
    fGroupMap[file] = list;
  }
  else
  {
    list = fGroupMap[file];
  }
  list->push_back(fileInfo);
}

//______________________________________________________________________________
std::string AFWebMaker::CSS()
{
  return "<link rel=\"stylesheet\" type=\"text/css\" href=\"af.css\"/>\n";
}

//______________________________________________________________________________
int AFWebMaker::DecodePath(const std::string& path, std::string& period,
                           std::string& esdPass, std::string& aodPass,
                           int& runNumber, std::string& user) const
{
  std::string dir = ::DirName(path);

  std::vector<std::string> parts;

  Tokenize(dir,parts,'/');

  int rv(-1);

  period = "";
  esdPass = "";
  aodPass = "";
  runNumber = -1;

  for ( std::vector<std::string>::size_type i = 0; i < parts.size(); ++i )
  {
    std::string s = parts[i];

    if ( ::BeginsWith(s,"LHC") )
    {
      period = s;
    }

    if ( BeginsWith(s,"ESDs") )
    {
      esdPass = parts[i+1];
    }

    if ( BeginsWith(s,"cpass") || BeginsWith(s,"vpass") )
    {
      esdPass = s;
    }

    if ( BeginsWith(s,"AOD") )
    {
      aodPass = s;
    }

    if ( s.size() == 9 && BeginsWith(s,"000") )
    {
      runNumber = atoi(s.substr(3,6).c_str());
      rv = path.find(s);
    }

    if ( s.size() == 6 && atoi(s.c_str()) > 0  )
    {
      runNumber = atoi(s.c_str());
      rv = path.find(s);
    }

  }

  if ( BeginsWith(dir,"/alice/cern.ch/user") )
  {
    std::vector<std::string> a;
    Tokenize(dir,a,'/');
    user = a[4];
    rv=0; // no further check on esdpass etc for user land...
  }
  else
  {
    if (period.size()==0)
    {
      // must have a period for anything not user land
      rv = -1;
    }

    if ( esdPass.size()== 0 && !strstr(path.c_str(),"/alice/data") && !strstr(path.c_str(),"/raw/"))
    {
      if ( aodPass.size() == 0 && !strstr(path.c_str(),"/alice/sim") )
      {
        // must find the esd pass and/or the aod pass for official data that is not raw data !
        rv = -2;
      }
    }
  }

  if ( rv < 0 )
  {
//    if ( !strstr(path.c_str(),"archive.zip") ) // silence warning for archives
    {
      WARNING() << "rv=" << rv << " for path=" << path << std::endl;
      WARNING() << "period=" << period << " esdpass=" << esdPass << " aodpass=" << aodPass
      << " runnumber=" << runNumber << " user=" << user << std::endl;
    }
  }

  return rv;
}

//_________________________________________________________________________________________________
AFWebMaker::AFFileInfoList& AFWebMaker::FileInfoList()
{
  DEBUG(2) << "FileInfoList " << std::endl;

  if ( fFileInfoList.empty() )
  {
    GetFileInfoListFromMap();
  }
  return fFileInfoList;
}

//_________________________________________________________________________________________________
void AFWebMaker::FillFileInfoMap(const std::string& workerFileName)
{
  DEBUG(2) << "FillFileInfoMap(workerFileName=" << workerFileName << ")" << std::endl;

  std::istringstream sin(workerFileName);

  std::string worker;

  getline(sin,worker,'.');

  if ( worker.empty() )
  {
    std::cerr << "workerFileName " << workerFileName << " is not valid" << std::endl;
    return;
  }

  std::string fullpath(fTopDir);
  fullpath += "/";
  fullpath += workerFileName;

  std::ifstream in(fullpath.c_str());
  std::string line;

  std::vector<std::string> lines;

  while (getline(in,line,'\n'))
  {
    lines.push_back(line);
  }

  if ( fDebugLevel > 2 )
  {
    DEBUG(2) << " read " << lines.size() << " lines from file " << fullpath << std::endl;
  }
  in.close();

  FillFileInfoMap(lines,worker);
}

//_________________________________________________________________________________________________
void AFWebMaker::FillFileInfoMap(const std::vector<std::string>& lines, const std::string& workerName)
{
  DEBUG(2) << "FillFileInfoMap(lines," << workerName << ")" << std::endl;

  AFFileInfoList* fileList = new AFFileInfoList;

  for ( std::vector<std::string>::size_type i = 0; i < lines.size(); ++i )
  {
    AFFileInfo fi(lines[i],fPrefix,workerName);
    fileList->push_back(fi);
  }

  fFileInfoMap[workerName] = fileList;

  if ( fDebugLevel > 3 )
  {
    for ( AFFileInfoList::const_iterator it = fileList->begin(); it != fileList->end(); ++it )
    {
      DEBUG(3) << (*it) << std::endl;
    }
  }
}

//_________________________________________________________________________________________________
AFWebMaker::AFFileInfoMap& AFWebMaker::FileInfoMap()
{
  DEBUG(2) << "FileInfoMap" << std::endl;

  if ( fFileInfoMap.empty() )
  {
    GetFileInfoMap();
    if ( fDebugLevel > 2 )
    {
      for ( AFFileInfoMap::const_iterator it = fFileInfoMap.begin(); it != fFileInfoMap.end(); ++it )
      {
        DEBUG(2) << "worker=" << it->first << std::endl;
      }
    }
  }
  return fFileInfoMap;
}

//______________________________________________________________________________
AFWebMaker::AFFileSize AFWebMaker::GenerateASCIIFileList(const std::string& key, const std::string& value, const AFFileInfoList& list) const
{
  GDEBUG(2) << "GenerateASCIIFileList("<< key << "," << value << ",list)" << std::endl;

  std::string filename(fHostName);

  filename += ".";
  filename += key;
  filename += ".";
  filename += value;
  filename += ".txt";

  std::ofstream out(filename.c_str());

  for ( AFFileInfoList::const_iterator it = list.begin(); it != list.end(); ++it )
  {
    out << (*it) << std::endl;
  }

  out.close();

#if defined(R__SEEK64)
  struct stat64 sbuf;
  if (lstat64(filename.c_str(), &sbuf) == 0) {
    return sbuf.st_size;
  }
#else
    struct stat sbuf;
    if (lstat(filename.c_str(), &sbuf) == 0) {
      return sbuf.st_size;
    }
#endif

  return 0;
}

//______________________________________________________________________________
void AFWebMaker::GenerateDataRepartition()
{
  DEBUG(2) << "GenerateDataRepartition" << std::endl;

  GroupMap();

  std::string table;

  table += "['Server', 'Size'],\n";

  std::string filesDeclarations = "var filesDeclarations = [";

  char buffer[1024];

  for ( AFFileInfoMap::const_iterator it = fGroupMap.begin(); it != fGroupMap.end(); ++it )
  {
    DEBUG(3) << " key " << it->first << std::endl;

    if ( !BeginsWith(it->first,"SERVER") ) continue;

    AFFileInfoList* list = it->second;

    std::vector<std::string> a;

    Tokenize(it->first,a,':');
    if ( a.size() < 2 ) continue;

    std::string key = a[0];
    std::string value = a[1];

    AFFileSize size = SumSize(*list);

    Tokenize(value,a,'.');
    std::string server = a[0];

    sprintf(buffer,"[ '%s', { v:%7.2f, f:'%7.2f GB'} ],\n",server.c_str(),size/byte2GB,size/byte2GB);

    table += buffer;

    AFFileSize fileSize = GenerateASCIIFileList(key,server,*list);

    std::string filename(fHostName);

    filename += ".";
    filename += key;
    filename += ".";
    filename += value;
    filename += ".txt";

    sprintf(buffer,"{ name: '%s', size : %llu, lc : %lu },\n",
            filename.c_str(),fileSize,1ul*list->size());
    filesDeclarations += buffer;

  }
  filesDeclarations += "];\n";

  std::string html;

  std::string css = CSS();
  std::string js;

  js += JSGoogleChart("corechart");

  js += "function drawChart() {\n";

  js += filesDeclarations;

  js += "var data = google.visualization.arrayToDataTable([\n";
  js += table;
  js += "]);\n";
  js += "var options = { \n";
  js += "title: 'Occupied disk (GB) by server',\n";
  js += "hAxis: { textPosition: 'out', minValue: 0 }};\n";
  js += "var chart = new google.visualization.BarChart(document.getElementById('chart_div'));\n";
  js += "chart.draw(data,options);\n";

  js += "function selectHandler() {\n";
  js += "var selectedItem = chart.getSelection()[0];\n";
  js += "if (selectedItem) {\n";
  js += "  var sel = data.getValue(selectedItem.row,0);\n";

  js += "  var filename = '";
  js += fHostName;
  js += ".SERVER.' + data.getValue(selectedItem.row,0) + '.txt';\n";

  js += "showFile(filename);\n";
  js += "}\n";
  js += "}\n";

  js += "google.visualization.events.addListener(chart,'select',selectHandler);\n";


  js += JSListJumper();

  html += HTMLHeader(fHostName,css,js);

  html += "<div><h1>Disk space usage repartition on ";
  html += fHostName;
  html += "</h1></div>\n";

  html += "<div id=\"chart_div\"></div>\n";

  html += "<div id=\"listjumper\">-</div>\n";

  html += HTMLFooter();

  std::ofstream out(FileNameDataRepartition().c_str());
  out << html;
  out.close();
}

//______________________________________________________________________________
void AFWebMaker::GenerateDatasetList()
{
  DEBUG(2) << "GenerateDatasetList" << std::endl;

  GroupMap();

  std::ofstream outfile(FileNameDataSetList().c_str());

  std::string lines;

  lines += HTMLHeader("Data groups",CSS(),"");

  lines += "<table>\n";

  lines += "<tr><th>Group</th><th># of files</th><th>Total size (GB)</th></tr>\n";

  char buffer[1024];

  for ( AFFileInfoMap::const_iterator it = fGroupMap.begin(); it != fGroupMap.end(); ++it )
  {
    AFFileInfoList* list = it->second;
    assert (list!=0);
    AFFileSize size = SumSize(*list);

    sprintf(buffer,"<tr><td>%-50s</td><td>%6lu</td><td>%7.2f</td></tr>\n",it->first.c_str(),1ul*list->size(), size/byte2GB);
    lines += buffer;
  }

  lines += "</table>\n";

  lines += HTMLFooter();

  outfile << lines;

  outfile.close();
}

//______________________________________________________________________________
void AFWebMaker::GeneratePieCharts()
{
  DEBUG(2) << "GeneratePieCharts" << std::endl;

  GroupMap(); // insure we have something to work with

  std::map<std::string,std::string> tables;

  tables["FILETYPE"] += "[ 'FileType', 'Size' ],\n ";
  tables["PERIOD"] += "[ 'Period', 'Size' ],\n";
  tables["DATATYPE"] += "[ 'Data type', 'Size' ],\n";

  tables["USER"] += " ['User', 'Size'],\n";
  tables["ESDPASS"] += " ['Pass', 'Size'],\n";
  tables["AOD"] += " ['AOD', 'Size'],\n";

  std::string filesDeclarations = "var filesDeclarations = [";

  char buffer[1024];

  for ( AFFileInfoMap::const_iterator it = fGroupMap.begin(); it != fGroupMap.end(); ++it )
  {
    AFFileInfoList* list = it->second;

    std::vector<std::string> a;

    Tokenize(it->first,a,':');

    if ( a.size() < 2 ) continue;

    std::string key = a[0];
    std::string value = a[1];

    if ( tables.count(key) )
    {
      AFFileSize size = SumSize(*list);

      sprintf(buffer,"[ '%s', { v:%7.2f, f:'%7.2f GB'} ],\n",value.c_str(),size/byte2GB,size/byte2GB);

      tables[key] += buffer;

      AFFileSize fileSize = GenerateASCIIFileList(key,value,*list);

      std::string filename;

      sprintf(buffer,"%s.%s.%s.txt",fHostName.c_str(),key.c_str(),value.c_str());

      filename = buffer;

      sprintf(buffer,"{ name: '%s', size : %llu, lc : %lu },\n",
              filename.c_str(),fileSize,1ul*list->size());

      filesDeclarations += buffer;
    }
  }

  filesDeclarations += "];\n";

  std::string html;
  std::string css = CSS();
  std::string js = JSGoogleChart();

  js += "function drawChart() {\n";

  js += filesDeclarations;

  for ( std::map<std::string,std::string>::const_iterator it = tables.begin(); it != tables.end(); ++it )
  {
    std::vector<std::string> a;

    Tokenize(it->second,a,'\n');

    if ( a.size() < 2 )
    {
      continue;
    }

    js += "var data";
    js += it->first;
    js += " = google.visualization.arrayToDataTable([\n";
    js += it->second;
    js += "]);\n";
    js += "var options";
    js += it->first;
    js += "= { \n";
    js += "title: 'Disk space by ";
    js += it->first;
    js += "',\npieSliceText: 'label' };\n";
    js += "var chart";
    js += it->first;
    js += " = new google.visualization.PieChart(document.getElementById('piechart_";
    js += it->first;
    js += "'));\n",

    js += "chart";
    js += it->first;
    js += ".draw(data";
    js += it->first;
    js += ",options";
    js += it->first;
    js += ");\n";

    js += "function selectHandler";
    js += it->first;
    js += "() {\n";

    js += "var selectedItem = chart";
    js += it->first;
    js += ".getSelection()[0];\n";
    js += "if (selectedItem) {\n";

    js += "  var sel = data";
    js += it->first;
    js += ".getValue(selectedItem.row,0);\n";

    js += "  var filename = '";
    js += fHostName;
    js += ".";
    js += it->first;
    js += ".";
    js += "' + data";
    js += it->first;
    js += ".getValue(selectedItem.row,0) + '.txt';\n";

    js += "showFile(filename);\n";
    js += "}\n";
    js += "}\n";

    js += "google.visualization.events.addListener(chart";
    js += it->first;
    js += ",'select',selectHandler";
    js += it->first;
    js += ");\n";
  }

  js += JSListJumper();

  html += HTMLHeader("Pie Charts", css,js);

  html += "<div><h1>Disk space usage details on ";
  html += fHostName;
  html += "</h1></div>\n";

  for ( std::map<std::string,std::string>::const_iterator it = tables.begin(); it != tables.end(); ++it )
  {
    html += "<div id=\"piechart_";
    html += it->first;
    html += "\" style=\"width: 1200px; height:500px\"></div>\n";
  }

  html += "<div id=\"listjumper\">-</div>\n";

  html += HTMLFooter();

  std::ofstream out(FileNamePieCharts().c_str());
  out << html;
  out.close();
}

//_________________________________________________________________________________________________
void AFWebMaker::GenerateReports()
{
  DEBUG(2) << "GenerateReports" << std::endl;

  AFFileInfoList& list = FileInfoList();

  if ( list.empty() )
  {
    return;
  }

  GenerateTreeMap();

  GenerateDatasetList();

  GeneratePieCharts();

  GenerateDataRepartition();

  std::ofstream out("index.html");

  std::string html = HTMLHeader(fHostName,CSS(),"");

  html += "<h1>";
  html += fHostName;
  html += "</h1>\n";

  html += "<nav class=\"mainmenu\">\n";

  html += "<a href=\"usermanual.html\">User manual</a>\n";

  html += "<a href=\"";
  html += FileNamePieCharts();
  html += "\">Pie charts of disk usage by file type, data type, pass, etc... </a>\n";

  html += "<a href=\"";
  html += FileNameTreeMap();
  html += "\">Tree map of disk usage</a>\n";

  html += "<a href=\"";
  html +=FileNameDataSetList();
  html += "\">Raw list of dataset groups</a>\n";

  html += "<a href=\"";
  html += FileNameDataRepartition();
  html += "\">Data repartition by server</a>\n";

  html += "</nav>\n";

  time_t now = time(0);
  std::tm* ptm = std::localtime(&now);
  char buffer[32];
  std::strftime(buffer,32,"%a, %d.%m.%Y %H:%M:%S",ptm);

  html += "<p class=\"footer\">Generated on ";
  html += buffer;
  html += "</h2>\n";

  html += HTMLFooter();

  out << html;

  out.close();
}

//______________________________________________________________________________
void AFWebMaker::GenerateTreeMap()
{
  DEBUG(2) << "GenerateTreeMap" << std::endl;

  AFFileInfoMap m;

  AFFileInfoList& fil = FileInfoList();

  // first loop to make a map of the paths hierarchy stopping at the depth of the root file - 2
  for ( AFFileInfoList::const_iterator it = fil.begin(); it != fil.end(); ++it )
  {
    const AFFileInfo& fileInfo = (*it);

    std::vector<std::string> tokens;

    Tokenize(fileInfo.fFullPath,tokens,'/');

    std::string truncatedPath;

    if ( tokens.size() < 3 )
    {
      WARNING() << "this path will not be taken into account (too short) : " << fileInfo.fFullPath << std::endl;
      continue;
    }

    for ( std::vector<std::string>::size_type i = 0; i < tokens.size()-2; ++i )
    {
      truncatedPath += "/";
      truncatedPath += tokens[i];
    }

    AFFileInfoList* list = 0x0;

    if (m.count(truncatedPath)==0)
    {
      list = new AFFileInfoList;
      m[truncatedPath] = list;
    }
    else
    {
      list = m[truncatedPath];
    }

    list->push_back(fileInfo);
  }

  AFFileInfoMap parentMap;

  std::string table = "['Location', 'Parent', '(size)', '(color)'],\n";

  //  // second loop to generate the treemap, starting from the leaves
  for ( AFFileInfoMap::const_iterator it = m.begin(); it != m.end(); ++it )
  {
    std::string path = fPrefix;

    path += "/";
    path += it->first;

    AFFileInfoList* list = it->second;

    std::vector<std::string> a;

    Tokenize(path,a,'/');

    std::string parent;

    for ( std::vector<std::string>::size_type i = 0; i < a.size(); ++i )
    {
      parent += "/";
      parent += a[i];

      std::string node;

      for ( std::vector<std::string>::size_type j = 0; j <= i; ++j )
      {
        node += "/";
        node += a[j];
      }

      AFFileInfoList* alist = 0x0;

      if ( ! parentMap.count(node) )
      {
        alist = new AFFileInfoList;
        parentMap[node] = alist;
      }
      else
      {
        alist = parentMap[node];
      }

      AddList(*list,*alist);
    }
  }


//  AFFileInfoList* list = parentMap["/alice"];

  AFFileInfoList* list = parentMap[fPrefix.c_str()];

  if (!list)
  {
    std::cerr << __FILE__ << ":" << __LINE__ << " Could not find any path starting at "
    << fPrefix << " !!! That's highly unlikely. Check the files with the file stats."
    << std::endl;
    return;
  }

  AFFileSize totalSize = SumSize(*list);

  char lineBuffer[1024];

  for ( AFFileInfoMap::const_iterator it = parentMap.begin(); it != parentMap.end(); ++it )
  {
    std::string str = it->first;

    std::string line;

    std::string shortname = ::BaseName(str.c_str());
    std::string parent = ::DirName(str.c_str());

    AFFileInfoList* list = parentMap[str];

    AFFileSize size = SumSize(*list);

    double color = size*1.0 / totalSize;

    if (parent=="/")
    {
      sprintf(lineBuffer,"[ {v:\'%s\',f:\'%s (%5.1f GB)\'},null,%llu,%5.3f],\n",str.c_str(),shortname.c_str(),size*1.0/byte2GB,size,color);
    }
    else
    {
      sprintf(lineBuffer,"[ {v:\'%s\',f:\'%s (%5.1f GB)\'},\'%s\',%llu,%5.3f],\n",str.c_str(),shortname.c_str(),size*1.0/byte2GB,parent.c_str(),size,color);
    }

    table += lineBuffer;
  }

  std::string html;
  std::string css = CSS();

  std::string js = JSGoogleChart("treemap");

  js += "function drawChart() {\n";

  js += "var data = google.visualization.arrayToDataTable(\n";
  js += "[\n";

  js += table.c_str();

  js += "]);\n";

  js += "var tree = new google.visualization.TreeMap(document.getElementById('chart_div'));\n";

  js += "var options = {\n";
  js += "minColor: '#ffffb2',\n";
  js += "midColor: '#fd8d3c',\n";
  js += "maxColor: '#bd0026',\n";
  js += "showScale: true,\n";
  js += "maxDepth: 1,\n";
  js += "generateTooltip: showSizeTooltip\n";
  js += "};\n";

  js += "tree.draw(data,options);\n";

  js += "function showSizeTooltip(row,size,value) {\n";
  js += "  var s = size/1024/1024/1024;\n";
  js += "  return '<div style=\"background:#fd9; padding:10px; border-style:solid\">' +\n";
  js += "  data.getValue(row, 0) + ' is ' + s.toFixed(1) + ' GB </div>';\n";
  js += "};\n";
  js += "};\n";

  html += HTMLHeader("TreeMap",css,js);

  html += "<div id=\"chart_div\" style=\"width: 1200px; height: 600px;\"></div>\n";

  html += HTMLFooter();

  std::ofstream out(FileNameTreeMap().c_str());
  out << html.c_str();
  out.close();
}

//_________________________________________________________________________________________________
void AFWebMaker::GenerateUserManual(const std::string& mdfile)
{
  GDEBUG(2) << "GenerateUserManual" << std::endl;

  if ( system("which pandoc") )
  {
    std::cout << "Cannot convert .md to .html as you do not seem to have pandoc installed"
    << std::endl;
    return;
  }

  std::string tmpFile("fragment.html");

  std::string cmd("pandoc ");

  cmd += mdfile;
  cmd += " -o ";
  cmd += tmpFile;

  if (system(cmd.c_str())) return;

  std::ifstream in(tmpFile.c_str());

  std::string html;

  std::stringstream buffer;
  buffer << in.rdbuf();

  std::string css = CSS();
  std::string js = JSTOC();

  html += HTMLHeader("User Manual",css,js);

  html += "<div id=\"toc\"></div>\n";

  html += buffer.str();

  html += HTMLFooter(true);

  std::ofstream out("usermanual.html");
  out << html << std::endl;
  out.close();
}

//_________________________________________________________________________________________________
void AFWebMaker::GetFileInfoListFromMap()
{
  DEBUG(2) << "GetFileInfoListFromMap" << std::endl;

  AFFileInfoMap& fim = FileInfoMap();
  AFFileInfoList& fil = fFileInfoList;

  for ( AFFileInfoMap::const_iterator it = fim.begin(); it != fim.end(); ++it )
  {
    const AFFileInfoList* list = it->second;
    for ( AFFileInfoList::const_iterator fit = list->begin(); fit != list->end(); ++fit )
    {
      fil.push_back(AFFileInfo(*fit));
    }
  }

  DEBUG(1) << "Found a grand total of " << fil.size() << " files" << std::endl;
}

//_________________________________________________________________________________________________
void AFWebMaker::GetFileInfoMap()
{
  DEBUG(2) << "GetFileInfoMap " << std::endl;

  std::vector<std::string> workers;

  GetWorkers(workers);

  for ( std::vector<std::string>::const_iterator it = workers.begin(); it != workers.end(); ++it )
  {
    FillFileInfoMap(it->c_str());
  }
}

//______________________________________________________________________________
std::string AFWebMaker::GetFileType(const std::string& path) const
{
  std::string file = ::BaseName(path);

  if ( strstr(file.c_str(),"FILTER_RAWMUON") )
  {
    file = "FILTER_RAWMUON";
  }

  if ( strstr(file.c_str(),"FILTER_ESDMUON") )
  {
    file = "FILTER_ESDMUON";
  }

  if ( strstr(file.c_str(),"FILTER_AODMUONWITHTRACKLETS") )
  {
    file = "FILTER_AODMUONWITHTRACKLETS";
  }

  if ( !strncmp(file.c_str(),"15000",5) )
  {
    file = "RAW 2015";
  }

  if ( !strncmp(file.c_str(),"14000",5) )
  {
    file = "RAW 2014";
  }

  if ( !strncmp(file.c_str(),"13000",5) )
  {
    file = "RAW 2013";
  }

  if ( !strncmp(file.c_str(),"12000",5) )
  {
    file = "RAW 2012";
  }

  if ( !strncmp(file.c_str(),"11000",5) )
  {
    file = "RAW 2011";
  }

  if ( !strncmp(file.c_str(),"10000",5) )
  {
    file = "RAW 2010";
  }

  return file;
}

//_________________________________________________________________________________________________
void AFWebMaker::GetWorkers(std::vector<std::string>& workers) const
{
  DEBUG(2) << "GetWorkers" << std::endl;

  workers.clear();

  DIR* dirp = opendir(fTopDir.c_str());

  struct dirent* de;

  while ( ( de =readdir(dirp) ) )
  {
    DEBUG(2) <<  "GetWorkers on file " << de->d_name << std::endl;

    if ( ! strncmp(de->d_name,fFileListPattern.c_str(),fFileListPattern.size())
        && strstr(de->d_name,".list") )
    {
      if ( fDebugLevel > 0 )
      {
        DEBUG(0) << "Found a file for worker " << de->d_name << std::endl;
      }
      workers.push_back(de->d_name);
    }
  }

  if ( workers.empty() )
  {
    ERROR() << " Could not find any file matching the required pattern [ " << fFileListPattern << "*.list ]"
          << " in directory " << fTopDir << ". Please check !" << std::endl;
  }
  else
  {
      DEBUG(0) << " Found " << workers.size() << " worker files to be dealt with" << std::endl;
      if ( fDebugLevel > 1 )
      {
        for ( std::vector<std::string>::size_type i = 0; i < workers.size(); ++i )
        {
          DEBUG(1) << workers[i] << std::endl;
        }
      }
  }
  closedir(dirp);
}

//______________________________________________________________________________
void AFWebMaker::GroupFileInfoList()
{
  DEBUG(2) << "GroupFileInfoList " << std::endl;

  FileInfoList(); // insure we have something to work with

  DEBUG(2) << " in GroupFileInfoList # of entries in fFileInfoList is " << fFileInfoList.size() << std::endl;

  for ( AFFileInfoList::const_iterator it = fFileInfoList.begin(); it != fFileInfoList.end(); ++it )
  {
    const AFFileInfo& fileInfo = (*it);

    std::string file = GetFileType(fileInfo.fFullPath.c_str());

    DEBUG(2) << "path=" << fileInfo.fFullPath << " filetype=" << file << std::endl;

    // first group by file type
    std::string ft("FILETYPE:");

    ft += file;

    AddFileToGroup(ft,fileInfo);

    std::string server("SERVER:");
    server += fileInfo.fHostName;

    // by server
    AddFileToGroup(server,fileInfo);

    // then broad categories : offical DATA, official SIM, and user land
    if ( fileInfo.BeginsWith("/alice/data") )
    {
      AddFileToGroup("DATATYPE:DATA",fileInfo);
    }

    if ( fileInfo.BeginsWith("/alice/sim") )
    {
      AddFileToGroup("DATATYPE:SIM",fileInfo);
    }

    if ( fileInfo.BeginsWith("/alice/cern.ch/user") )
    {
      AddFileToGroup("DATATYPE:USER",fileInfo);
    }

    // Now group by period / esdPass / aodPass

    std::string period("");
    std::string esdPass("");
    std::string aodPass("");
    int runNumber(-1);
    std::string user("");

    int rv = DecodePath(fileInfo.fFullPath,period,esdPass,aodPass,runNumber,user);

    if (rv<0)
    {
      WARNING() << "Could not find period/esdpass/aodpass/runnumber for path " << fileInfo.fFullPath << std::endl;
      continue;
    }

    if ( user.size() > 0 )
    {
      std::string suser("USER:");
      suser += user;

      AddFileToGroup(suser,fileInfo);

    }

    if ( runNumber > 0 )
    {
      std::ostringstream os;
      os << "RUN:" << runNumber;

      // this is not strictly needed but might help to point to "golden" runs
      AddFileToGroup(os.str().c_str(),fileInfo);
    }

    if ( period.size() > 0 )
    {
      std::string speriod("PERIOD:");
      speriod += period;

      AddFileToGroup(speriod,fileInfo);

      if ( esdPass.size() > 0 )
      {
        std::string sesd("ESDPASS:");
        sesd += period;
        sesd += "_";
        sesd += esdPass;

        AddFileToGroup(sesd,fileInfo);

        if ( aodPass.size() > 0 )
        {
          std::string saod("AOD:");
          saod += period;
          saod += "_";
          saod += esdPass;
          saod += "_";
          saod += aodPass;

          AddFileToGroup(saod,fileInfo);

          std::ostringstream sds;


          sds << "DS:" << period << "_" << esdPass << "_" << aodPass << "_" << runNumber;

          AddFileToGroup(sds.str(),fileInfo);
        }
      }
      else {

        if ( aodPass.size() > 0 )
        {
          std::string saod("AOD:");
          saod += period;
          saod += "_";
          saod += aodPass;

          AddFileToGroup(saod,fileInfo);

          std::ostringstream sds;

          sds << "DS:" << period << "_" << aodPass << "_" << runNumber;

          AddFileToGroup(sds.str(),fileInfo);

        }

      }
    }
  }
}

//______________________________________________________________________________
AFWebMaker::AFFileInfoMap& AFWebMaker::GroupMap()
{
  DEBUG(1) << "GroupMap " << std::endl;

  if ( fGroupMap.empty() )
  {
    GroupFileInfoList();
  }
  return fGroupMap;
}

//______________________________________________________________________________
std::string AFWebMaker::HTMLFooter(bool withJS)
{
  std::string footer;

  if ( withJS )
  {
    //    footer += "<script>\n";
    //    footer += "$(function() { $(\"#toc\").tocify({ context: \"body\", selectors: \"h2,h3,h4\", showEffect: \"fadeIn\", showAndHide: false });});\n";
    //    footer += "</script>\n";
  }

  footer += "</body></html>\n";
  return footer;
}

//______________________________________________________________________________
std::string AFWebMaker::HTMLHeader(const std::string& title, const std::string& css, const std::string& js)
{
  std::string header;

  header += "<!DOCTYPE html>\n";
  header += "<html>\n";

  header += "<head>\n";

  header += "<title>";
  header += title;
  header += "</title>\n";

  header += css;

  if (js.size() > 0 )
  {
    header += js;
    header += "</script>\n";
  }

  header += "</head>\n";

  header += "<body>\n";

  return header;
}


//______________________________________________________________________________
std::string AFWebMaker::JSGoogleChart(const std::string& chartPackage) const
{
  std::string js;

  js += "<script type=\"text/javascript\" src=\"//ajax.googleapis.com/ajax/libs/jquery/1.11.1/jquery.min.js\"></script>\n";
  js += "<script type=\"text/javascript\" src=\"https://www.google.com/jsapi\"></script>\n";
  js += "<script type=\"text/javascript\">\n";
  js += "google.load(\"visualization\", \"1\", {packages:[\"";
  js += chartPackage;
  js += "\"]});\n";
  js += "google.setOnLoadCallback(drawChart);\n";

  return js;
}

//______________________________________________________________________________
std::string AFWebMaker::JSListJumper() const
{
  std::string js;

  js += " var x = null; var y = null;";
  js += "document.addEventListener('mousemove', onMouseUpdate, false);\n";
  js += "document.addEventListener('mouseenter', onMouseUpdate, false);\n";

  js += "function onMouseUpdate(e) { x = e.pageX; y = e.pageY;}\n";

  js += "function findObject(a, name) {for (var i = 0; i < a.length; i++) {if (a[i].name === name) { return  a[i];}}return null;}\n";

  js += "$(\"#listjumper\").click(function() { $(this).css(\"display\",\"none\"); });\n";

  js += "function showFile(filename) {\n";

  js += "var listjumper = $(\"#listjumper\");\n";

  js += "var file = findObject(filesDeclarations,filename);\n";

  js += "var size = file.size/1024.0/1024;\n";

  js += "var msg = '<p>Click the link below to get access to the list of ' + file.lc + ' files in this category</p>';\n";

  js += "msg += '<p><a href=\"' + filename + '\">' + filename + '</a> (' + size.toFixed(2) + ' MB text file)</p>';\n";

  js += "listjumper.html(msg).css( { display: 'block', top: y , left: x } ) }\n";

  js += "} \n";

  return js;
}

//______________________________________________________________________________
std::string AFWebMaker::JSTOC()
{
  std::string js;

  js += "<script type=\"text/javascript\" src=\"//ajax.googleapis.com/ajax/libs/jquery/1.11.1/jquery.min.js\"></script>\n";
  js += "<link rel=\"stylesheet\" href=\"//ajax.googleapis.com/ajax/libs/jqueryui/1.11.1/themes/smoothness/jquery-ui.css\" />\n";
  js += "<script src=\"//ajax.googleapis.com/ajax/libs/jqueryui/1.11.1/jquery-ui.min.js\"></script>\n";
  js += "<script src=\"jquery.tocify.min.js\"></script>\n";

  js += "<script>\n";
  js += "$(function() { $(\"#toc\").tocify({ context: \"body\", selectors: \"h2,h3,h4\", showEffect: \"fadeIn\", showAndHide: false });});\n";

  return js;
}


//______________________________________________________________________________
std::string AFWebMaker::OutputHtmlFileName(const std::string& type) const
{

  std::string name(fHostName);

  name += "." ;
  name += type;
  name += ".html";

  return name;
}

//_________________________________________________________________________________________________
AFWebMaker::AFFileSize AFWebMaker::SumSize(const AFFileInfoList& list) const
{
  AFFileSize thesize(0);

  for ( AFFileInfoList::const_iterator it = list.begin(); it != list.end(); ++it )
  {
    thesize += it->fSize;
  }
  return thesize;
}
