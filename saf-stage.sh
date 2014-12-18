#!/bin/sh
#
#  saf-stage.sh $src $dest
#
# to properly work, this script needs the Alien API be in the
# PATH (to find xrdgsiproxy and alien-token-init commands)
#

#ALIEN_USER=laphecet
ALIEN_USER=proof

SWBASEDIR="/cvmfs/alice.cern.ch/x86_64-2.6-gnu-4.1.2/Packages/AliRoot"
DEFAULTROOT="/cvmfs/alice.cern.ch/x86_64-2.6-gnu-4.1.2/Packages/ROOT/v5-34-13"
DEBUG=true

# insure we get a valid token
function assert_alien_token()
{
  # alien-token-init laphecet

  tryToGetToken=$1

  if [ -n "$DEBUG" ]; then
    echo "assert_alien_token"
  fi

  proxyValidity=$(xrdgsiproxy info 2>&1 | grep "time left" | cut -d " " -f 6)
  if [[ $proxyValidity == "" || $proxyValidity == "0h:0m:0s" ]]; then
   xrdgsiproxy info
    echo "No valid proxy found"
    if [ "$tryToGetToken" -eq 1 ]; then
	echo "Doing alien-token-init"
	alien-token-init $ALIEN_USER
	assert_alien_token 0
    fi
    exit 1
  else
  if [ -n "$DEBUG" ]; then
    echo "Proxy still valid. Using it."
  fi
  fi
}

# decode the src url
function decode_src()
{
  # extract the protocol
  src_proto="$(echo $1 | grep :// | sed -e's,^\(.*://\).*,\1,g')"

  # remove the protocol
  src_url="$(echo ${1/$src_proto/})"
  # extract the user (if any)
  src_user="$(echo $src_url | grep @ | cut -d@ -f1)"
  # extract the host
  src_host="$(echo ${src_url/$src_user@/} | cut -d/ -f1)"
  # extract the path (if any)
  if [ -z "$src_host" ]; then
    src_path="$(echo $src_url | grep / | cut -d/ -f1-)"
  else
    src_path="$(echo $src_url | grep / | cut -d/ -f2-)"
  fi
  src_proto="$(echo ${src_proto/:\/\//})"

  if [ -n "$DEBUG" ]; then
    echo "url: $src_url"
    echo "  proto: $src_proto"
    echo "  user: $src_user"
    echo "  host: $src_host"
    echo "  path: $src_path"
  fi
}

# do a plain xrdcp transfer (xrootd protocol)
function xrdcp_transfer()
{
  echo "xrdcp transfer not yet implemented"
  return 2
}

# perform a copy with a Root which can do a filtering as well
function cpmacro_with_filter_transfer()
{

  if [ -n "$DEBUG" ]; then
    echo "cpmacro_with_filter_transfer $1 $2"
  fi

  f=${1/*_FILTER_/}
  filter=${f/_WITH_ALIROOT_*/}
  aliroot=${f/*_WITH_ALIROOT_/}
  aliroot=${aliroot/.root/}

  if [ -n "$DEBUG" ]; then
    echo "src=$1"
    echo "dest=$2"
    echo "filter=$filter"
    echo "aliroot=$aliroot"
  fi

# check the aliroot version request actually exists

  if [ ! -d "$SWBASEDIR/$aliroot" ]; then
    echo "Requested aliroot version not found : $SWBASEDIR/$aliroot does not exist"
    return 4;
  fi

# fine, the version exists, let's get the Root dependency and the env. correct

  source /cvmfs/alice.cern.ch/etc/login.sh
  source alienv setenv VO_ALICE@AliRoot::${aliroot}
  echo $PATH
  echo $LD_LIBRARY_PAT
  root=$(which root)

  macro=${ALICE_ROOT}/PWG/muon/CpMacroWithFilter.C
  
  if [ ! -e "$macro" ]; then
      echo "$macro not found !"
      return 5;
  fi

  filterMacroFullPath="${ALICE_ROOT}/PWG/muon/FILTER_${filter}.C"
  filterRootLogonFullPath="${ALICE_ROOT}/PWG/muon/FILTER_${filter}_rootlogon.C"
  alirootPath="${ALICE_ROOT}"

  if [ ! -f "$filterMacroFullPath" ]; then
      echo "$filterMacroFullPath not found !"
      return 6;
  fi

  if [ ! -f "$filterRootLogonFullPath" ]; then
      echo "$filterRootLogonFullPath not found !"
      return 7;
  fi

  # strip the filtering bit and pieces from the source file name
  # before giving it to the filtering macro
  from=${1/_FILTER_/}
  from=${from/$filter/}
  from=${from/_WITH_ALIROOT_/}
  from=${from/$aliroot/}

  $root -n -q -b $macro+\(\"alien://$from\",\"$2\",\"FILTER_$filter\",\"$filterMacroFullPath\",\"$filterRootLogonFullPath\",\"$alirootPath\"\)

  return 0
}

# perform a copy using a Root macro
function cpmacro_transfer()
{
  assert_alien_token 1

  source /tmp/gclient_env_$UID

  filter="$(echo $1 | grep FILTER)"
  if [ -n "$filter" ]; then
    cpmacro_with_filter_transfer $1 $2
    return $?
  fi

  source $DEFAULTROOT/bin/thisroot.sh

  root -n -b <<EOF 
TGrid::Connect("alien://");
TFile::Cp("alien://$1","$2");
.q
EOF
  

  return 3
}

# the src url is from alien
function alien_transfer()
{
  local src=$1
  local dest=$2

  isarchive="$(echo $src | grep '\.zip')"
  isroot="$(echo $src | grep '\.root')"

  if [ -n "$DEBUG" ]; then
    echo "src: $src"
    echo "dest: $dest"
    echo "isarchive: $isarchive"
    echo "isroot: $isroot"
  fi

  if [ -n "$isarchive" ]; then
    xrdcp_transfer $1 $2
    return $?
  fi

  if [ -n "$isroot" ]; then
    cpmacro_transfer $1 $2
    return $?
  fi
}

###
### main
###

decode_src $1


case $src_proto in
  "root")
  echo "root protocol not yet supported by this script"
  exit 1
  ;;
  "alien")
  alien_transfer $src_path $2
  ;;
esac

exit $?


