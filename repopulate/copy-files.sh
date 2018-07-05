#!/bin/bash

filelist=$1
user=$2
server=$3

ls -al 

source  /cvmfs/alice.cern.ch/etc/login.sh
eval `alienv printenv VO_ALICE@AliPhysics::vAN-20180702-1`

source user_worker_env.sh

alien-token-init $user

alien-token-info

touch copymacro.C

echo "{" >> copymacro.C
echo "TGrid::Connect(\"alien:\");" >> copymacro.C

for f in $(cat $filelist)
do
  if xrd $server existfile $f | grep "not found" > /dev/null; then
     echo "cout << \"downloading alien://$f\n\";" >> copymacro.C
     echo "TFile::Cp(\"alien://$f\",\"root://$server/$f\");" >> copymacro.C
  else
    echo file $f already there
  fi
done

echo "}" >> copymacro.C

cat copymacro.C

root -l -n -b -q copymacro.C
