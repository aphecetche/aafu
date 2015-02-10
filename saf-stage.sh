#!/bin/sh
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

# insure we get a valid token
function assert_alien_token()
{
  # alien-token-init laphecet

  tryToGetToken=$1

  if [ -n "$DEBUG" ]; then
    echo "assert_alien_token"
  fi

if  [ ! -e "/tmp/gclient_token_$UID" ]; then
	if [ "$tryToGetToken" -eq 1 ]; then
		if [ -n "$DEBUG" ]; then
			echo "no token found. Doing alien-token-init"
		fi
		alien-token-init $ALIEN_USER
		assert_alien_token 0
	else
		echo "no token found. Bailing out."
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
  assert_alien_token 1


  source /tmp/gclient_env_$UID

  filter="$(echo $1 | grep FILTER)"
  if [ -n "$filter" ]; then
rootfile_transfer_with_filter $1 $2
    return $?
  fi

 if [ ! -d "$SWBASEDIR/AliRoot/$DEFAULTALIROOT" ]; then
    echo "Requested aliroot version not found : $SWBASEDIR/AliRoot/$DEFAULTALIROOT does not exist"
    return 5;
  fi

  source /cvmfs/alice.cern.ch/etc/login.sh
 source alienv setenv VO_ALICE@AliRoot::${DEFAULTALIROOT}

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


