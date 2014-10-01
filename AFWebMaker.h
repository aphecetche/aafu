#ifndef AFWEBMAKER_H
#define AFWEBMAKER_H

#include <string>
#include <ctime>
#include <map>
#include <list>
#include <vector>

class AFWebMaker
{
public:
  
  typedef unsigned long long int AFFileSize;

  class AFFileInfo
  {
  public:
    AFFileInfo(): fSize(0), fTime(), fFullPath("") {}
    AFFileInfo(const std::string& lsline, const std::string& prefix, const std::string& hostname);
    
    friend std::ostream& operator<<(std::ostream& os, const AFFileInfo& fileinfo);
    
    std::string Basename() const;
    
    bool BeginsWith(const std::string& str) const;

  public:
    AFFileSize fSize;
    time_t fTime;
    std::string fFullPath;
    std::string fHostName;
  };
  
public:
  
  typedef std::list<AFFileInfo> AFFileInfoList;
  typedef std::map<std::string, AFFileInfoList*> AFFileInfoMap;
  
  AFWebMaker(const std::string& topdir, const std::string& fileListPattern, const std::string& prefix);
  ~AFWebMaker();
  
  void FillFileInfoMap(const std::vector<std::string>& lines, const std::string& workerName);
  
  void GenerateReports();
  
  void GenerateUserManual(const std::string& mdfile);
  
private:
  
  void AddFileToGroup(const std::string& file, const AFWebMaker::AFFileInfo& fileInfo);

  std::string CSS() const;

  int DecodePath(const std::string& path, std::string& period,
                 std::string& esdPass, std::string& aodPass,
                 int& runNumber, std::string& user) const;

  std::string FileNamePieCharts() const { return OutputHtmlFileName("piecharts"); }
  std::string FileNameTreeMap() const { return OutputHtmlFileName("treemap"); }
  std::string FileNameDataSetList() const { return OutputHtmlFileName("datasetlist"); }
  std::string FileNameDataRepartition() const { return OutputHtmlFileName("datarepartition"); }
  
  void FillFileInfoMap(const std::string& worker="");

  AFFileInfoList& FileInfoList();
  
  AFFileInfoMap& FileInfoMap();

  AFFileInfoMap& GroupMap();

  AFFileSize GenerateASCIIFileList(const std::string& key, const std::string& value, const AFFileInfoList& list) const;
  
  void GenerateDataRepartition();
  
  void GenerateDatasetList();
  
  void GeneratePieCharts();
  
  void GenerateTreeMap();
  
  void GetFileInfoMap();
  
  void GetFileInfoListFromMap();
  
  std::string GetFileType(const std::string& path) const;

  void GetWorkers(std::vector<std::string>& workers) const;

  void GroupFileInfoList();

  std::string HTMLHeader(const std::string& title, const std::string& css, const std::string& js) const;

  std::string HTMLFooter(bool withJS=false) const;

  std::string JSGoogleChart(const std::string& chartPackage="corechart") const;
  
  std::string JSListJumper() const;

  std::string JSTOC() const;
  
  std::string OutputHtmlFileName(const std::string& type) const;

  AFFileSize SumSize(const AFFileInfoList& list) const;
  
private:
  std::string fTopDir; // top dir where to find one ASCII file per machine containing the list of files
  std::string fFileListPattern; // all files in fTopDir starting with fFileListPattern will be used
  std::string fPrefix; // prefix to be removed in the filenames (e.g. /data)
  std::string fHostName;
  AFFileInfoMap fFileInfoMap;
  AFFileInfoList fFileInfoList;
  AFFileInfoMap fGroupMap;
};

#endif
