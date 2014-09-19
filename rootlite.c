{
  gEnv->SetValue("Proof.DataSetManager",
  "alien cache:/tmp/datasetcache "
  "urltemplate:root://alice-caf.cern.ch/<path> "
  "cacheexpiresecs:86400");
  TProof::Open("");
}
