#!/bin/bash
#
#  saf-stage.sh $src $dest
#
# to properly work, this script needs the Alien API be in the
# PATH (to find xrdgsiproxy and alien-token-init commands)
#

saf="$(hostname | grep nansaf)"

if [ -n "$saf" ]; then
	ALIEN_USER=proof
else
	ALIEN_USER=laphecet
	echo "No SAF detected, using ALIEN_USER=$ALIEN_USER"
fi

SWBASEDIR="/cvmfs/alice.cern.ch/x86_64-2.6-gnu-4.1.2/Packages"
DEFAULTALIROOT="v5-06-02"
DEBUG=true

# removes from a $PATH-like variable all the paths containing at least one of the specified files:
# variable name is the first argument, and file names are the remaining arguments
function AliRemovePaths() {

  local RetainPaths="/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin:/usr/local/sbin:/usr/local/lib:$HOME/bin"
  local VARNAME=$1
  shift
  local DIRS=`eval echo \\$$VARNAME`
  local NEWDIRS=""
  local OIFS="$IFS"
  local D F KEEPDIR
  local RemovePathsDebug=0
  local DebugPrompt="${Cc}RemovePaths>${Cz} "
  IFS=:

  [[ $RemovePathsDebug == 1 ]] && echo -e "${DebugPrompt}${Cm}variable:${Cz} $VARNAME"

  for D in $DIRS ; do

    [[ $RemovePathsDebug == 1 ]] && echo -e "${DebugPrompt}  directory $D"

    KEEPDIR=1
    D=$( cd "$D" 2> /dev/null && pwd || echo "$D" )
    if [[ -d "$D" ]] ; then

      # condemn directory if one of the given files is there
      for F in $@ ; do
        if [[ -e "$D/$F" ]] ; then
          [[ $RemovePathsDebug == 1 ]] && echo -e "${DebugPrompt}    remove it: found one of the given files"
          KEEPDIR=0
          break
        elif [ -e "${D}"/tgt_*/"${F}" ] ; then
          [[ $RemovePathsDebug == 1 ]] && echo -e "${DebugPrompt}    remove it: a tgt_ subdirectory contains one of the given files"
          KEEPDIR=0
          break
        fi
      done

      # retain directory if it is in RetainPaths (may revert)
      for K in $RetainPaths ; do
        if [[ "$D" == "$( cd "$K" 2> /dev/null ; pwd )" ]] ; then
          [[ $RemovePathsDebug == 1 ]] && echo -e "${DebugPrompt}    kept: is a system path"
          KEEPDIR=1
          break
        fi
      done

    else
      [[ $RemovePathsDebug == 1 ]] && echo -e "${DebugPrompt}    remove it: cannot access it"
      KEEPDIR=0
    fi
    if [[ $KEEPDIR == 1 ]] ; then
      [[ $RemovePathsDebug == 1 ]] && echo -e "${DebugPrompt}    ${Cg}final decision: keeping${Cz}"
      [[ "$NEWDIRS" == "" ]] && NEWDIRS="$D" || NEWDIRS="${NEWDIRS}:${D}"
    else
      [[ $RemovePathsDebug == 1 ]] && echo -e "${DebugPrompt}    ${Cr}final decision: discarding${Cz}"
    fi

  done

  IFS="$OIFS"

  eval export $VARNAME="$NEWDIRS"
  AliCleanPathList $VARNAME

}

# cleans leading, trailing and double colons from the variable whose name is passed as the only arg
function AliCleanPathList() {
  local VARNAME="$1"
  local STR=`eval echo \\$$VARNAME`
  local PREV_STR
  while [[ "$PREV_STR" != "$STR" ]] ; do
    PREV_STR="$STR"
    STR=`echo "$STR" | sed s/::/:/g`
  done
  STR=${STR#:}
  STR=${STR%:}
  if [[ $STR == '' ]] ; then
    unset $VARNAME
  else
    eval export $VARNAME=\"$STR\"
  fi
}

# insure we get a valid token
function assert_alien_token()
{
  # alien-token-init laphecet

  tryToGetToken=$1

  if [ -n "$DEBUG" ]; then
    echo "assert_alien_token"
  fi

if  [ ! -e "/tmp/gclient_token_$UID" -o ! -e "/tmp/gclient_env_$UID" ]; then
	if [ "$tryToGetToken" -eq 1 ]; then
		if [ -n "$DEBUG" ]; then
			echo "No token | env found. Doing alien-token-init"
		fi
		alien-token-init $ALIEN_USER
		assert_alien_token 0
	else
		echo "No token found. Bailing out."
		exit 1
	fi
else
	# check the token has still some time left
	now=$(date +%s)
	tokenvaliduntil=$(grep -i expiretime /tmp/gclient_token_$UID)
	tokenvaliduntil=$(echo ${tokenvaliduntil// /}) # remove spaces
	tokenvaliduntil=$(echo $tokenvaliduntil | cut -d '=' -f 2)
	# add 30 minutes (should be enough even for the longest filters)
	now=$((now+1800))
	if [ $now -ge $tokenvaliduntil ]; then
			echo "token will expire soon. get another one"
		alien-token-init $ALIEN_USER
		assert_alien_token 0
	fi
fi

if [ -e "/tmp/gclient_env_$UID" ]; then
# source /tmp/gclient_env_$UID
# source a selected part of the /tmp/gclient_env_$UID (i.e. ignoring the LD_LIBRARY_PATH setting)
  eval "$(sed '/LD_LIBRARY_PATH/d' /tmp/gclient_env_$UID)"
else
  echo "File /tmp/gclient_env_$UID not found !!!"
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

# perform a copy with a Root which can do a filtering as well
# New version assuming the ?filter=xxxx&aliphysics=yyyy syntax

function rootfile_transfer_with_filter_query()
{

  if [ -n "$DEBUG" ]; then
    echo "rootfile_transfer_with_filter_query $1 $2"
  fi

n=$(expr index $1 '?')

option=${1:$n}

echo $option

options=(${option//&/ })

filter=""
aliphysics=""

for a in ${options[@]}
do
if [[ $a =~ ^filter= ]]; then
filter=${a/filter=/}
fi
if [[ $a =~ ^aliphysics= ]]; then
aliphysics=${a/aliphysics=/}
fi

done

if [ -z "$filter" ] ; then
echo "Missing filter name"
return 5;
fi

if [ -z "$aliphysics" ] ; then
echo "Missing aliphysics version"
return 6;
fi

   if [ -n "$DEBUG" ]; then
    echo "src=$1"
    echo "dest=$2"
    echo "filter=$filter"
    echo "aliphysics=$aliphysics"
  fi

# check the aliphysics version request actually exists

  if [ ! -d "$SWBASEDIR/AliPhysics/$aliphysics" ]; then
    echo "Requested aliphysics version not found : $SWBASEDIR/AliPhysics/$aliphysics does not exist"
    return 4;
  fi

# fine, the version exists, let's get the Root dependency and the env. correct

  source /cvmfs/alice.cern.ch/etc/login.sh
  source alienv setenv VO_ALICE@AliPhysics::${aliphysics}

  assert_alien_token 1

  echo "after assert_alien_token"
  echo "PATH=$PATH"
  echo "LD_LIBRARY_PATH=$LD_LIBRARY_PATH"

# here be sure to redefine the LD_LIBRARY_PATH and PATH...

  source alienv setenv VO_ALICE@AliPhysics::${aliphysics}

  echo "after source alienv again..."
  echo "PATH=$PATH"
  echo "LD_LIBRARY_PATH=$LD_LIBRARY_PATH"

#  root=$(which root)

  # strip the filtering bit and pieces from the source file name
  # before giving it to the filtering macro
  from=${1/${option}/}
  from=${from/\?}
  
 if [ -n "$DEBUG" ]; then
		VLEVEL=1
 else
	VLEVEL=0
 fi
 
aaf-stage-and-filter --from alien://$from --to $2 --verbose $VLEVEL --filter $filter

 return $?
}

# perform a copy with a Root which can do a filtering as well
function rootfile_transfer_with_filter()
{

  if [ -n "$DEBUG" ]; then
    echo "cpmacro_with_filter_transfer $1 $2"
  fi

  f=${1/*FILTER_/}
  filter=${f/_WITH_ALIPHYSICS_*/}
  aliphysics=${f/*_WITH_ALIPHYSICS_/}
  aliphysics=${aliphysics/.root/}

  if [ -n "$DEBUG" ]; then
    echo "src=$1"
    echo "dest=$2"
    echo "filter=$filter"
    echo "aliphysics=$aliphysics"
  fi

# check the aliphysics version request actually exists

  if [ ! -d "$SWBASEDIR/AliPhysics/$aliphysics" ]; then
    echo "Requested aliphysics version not found : $SWBASEDIR/AliPhysics/$aliphysics does not exist"
    return 4;
  fi

# fine, the version exists, let's get the Root dependency and the env. correct

  source /cvmfs/alice.cern.ch/etc/login.sh
  source alienv setenv VO_ALICE@AliPhysics::${aliphysics}

  assert_alien_token 1

  echo "PATH=$PATH"
  echo "LD_LIBRARY_PATH=$LD_LIBRARY_PATH"

#  root=$(which root)

  # strip the filtering bit and pieces from the source file name
  # before giving it to the filtering macro
  from=${1/.FILTER_/}
  from=${from/$filter/}
  from=${from/_WITH_ALIPHYSICS_/}
  from=${from/$aliphysics/}

 if [ -n "$DEBUG" ]; then
		VLEVEL=1
 else
	VLEVEL=0
 fi
 
aaf-stage-and-filter --from alien://$from --to $2 --verbose $VLEVEL --filter $filter

 return $?
}

# perform a copy using a Root function
function rootfile_transfer()
{
  filter="$(echo $1 | grep FILTER)"
  if [ -n "$filter" ]; then
    rootfile_transfer_with_filter $1 $2
    return $?
  fi

  filter="$(echo $1 | grep 'filter=')"
  if [ -n "$filter" ]; then
    rootfile_transfer_with_filter_query $1 $2
    return $?
  fi

 if [ ! -d "$SWBASEDIR/AliRoot/$DEFAULTALIROOT" ]; then
    echo "Requested aliroot version not found : $SWBASEDIR/AliRoot/$DEFAULTALIROOT does not exist"
    return 5;
  fi

  source /cvmfs/alice.cern.ch/etc/login.sh
  source alienv setenv VO_ALICE@AliRoot::${DEFAULTALIROOT}

  assert_alien_token 1

  root -n -b <<EOF
  TGrid::Connect("alien://");
  Bool_t ok = TFile::Cp("alien://$1","$2");
  if (!ok) gSystem->Exit(6);
  .q
EOF
 RCODE=$?

 return $RCODE
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

 hasanchor="$(echo $src | grep '#')"
hasfilter="$(echo $src | grep 'FILTER_')"

	if [ -n "$hasanchor" ]; then
	if [ -n "$hasfilter" ]; then
		echo "Cannot deal with both an anchor and a filter. Sorry about that." # this should be anyway forbidden by TDataSetManagerAliEn
		return 7
	fi
	# remove the anchor
	src="$(echo $src |  cut -d# -f1)"
	echo "Anchor detected : removing it, i.e. using src = $src"
fi

if [ -z "$isarchive" -a -z "$isroot" ]; then
	echo "Don't know how to deal with that file extension : $src"
	return 10
fi

		rootfile_transfer $src $dest
    return $?

}

###
### main
###

if [ $# -lt 2 ]; then
	echo "Wrong number of arguments : usage is 'saf-stage src dest'"
	exit 100
fi
	
decode_src $1

# clean up alien environment

unset ALIEN_ROOT
unset ALIEN_VERSION
unset GLOBUS_LOCATION

AliRemovePaths LD_LIBRARY_PATH libgapiUI.so
AliRemovePaths PATH alien-token-init alien-perl

export ALIEN_SKIP_GCLIENT_ENV=1

case "$src_proto" in
  "root")
  echo "root protocol not yet supported by this script"
  exit 1
  ;;
  "alien")
    alien_transfer $src_path $2
    exit $?
  ;;
esac

exit 2


