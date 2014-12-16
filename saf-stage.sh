#!/bin/sh
#
#  saf-stage.sh $src $dest
#

ALIEN_USER=laphecet

SWBASEDIR="/cvmfs/alice.cern.ch/x86_64-2.6-gnu-4.1.2/Packages/AliRoot";

DEBUG=true

# insure we get a valid token
function assert_alien_token()
{
  # alien-token-init laphecet

  if [ -n "$DEBUG" ]; then
    echo "assert_alien_token"
  fi

  proxyValidity=`xrdgsiproxy info 2>&1 | grep "time left" | cut -d " " -f 6`
  if [[ $proxyValidity == "" || $proxyValidity == "0h:0m:0s" ]]; then
    echo "No valid proxy found. Nothing done!"
    exit
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
    echo "filter=$filter"
    echo "aliroot=$aliroot"
  fi

# check the aliroot version request actually exists

  if [ ! -d "$SWBASEDIR/$aliroot" ]; then
    echo "Requested aliroot version not found : $SWBASEDIR/$aliroot does not exist"
    return 4;
  fi

# fine, the version exists, let's get the Root dependency

  source /cvmfs/alice.cern.ch/etc/login.sh
  tmp="$(alienv printenv VO_ALICE@AliRoot::${aliroot}) | tr ';' '\n'"
  echo $tmp
#  echo "ROOTSYS is $ROOTSYS"

  return 5;
}

# perform a copy using a Root macro
function cpmacro_transfer()
{
  assert_alien_token

  filter="$(echo $1 | grep FILTER)"
  if [ -n "$filter" ]; then
    cpmacro_with_filter_transfer $1 $2
    return $?
  fi


  echo "would to a simple TFile::Cp here..."

  return 3
}

# the src url is from alien
function alien_transfer()
{
  local src=$1
  local dest=$2

  archive="$(echo $src | grep '\.zip')"
  root="$(echo $src | grep '\.root')"

  if [ -n "$DEBUG" ]; then
    echo "src: $src"
    echo "dest: $dest"
    echo "archive: $archive"
    echo "root: $root"
  fi

  if [ -n "$archive" ]; then
    xrdcp_transfer $1 $2
    return $?
  fi

  if [ -n "$root" ]; then
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


