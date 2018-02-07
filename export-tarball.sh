#!/bin/sh

# bail on errors
set -e

base=$(basename $PWD)
dest="/proj/edgect/share/dpdk/tarballs"

usage()
{
   echo "$0 [-b basename] [-d dir] [hash|tag]"
   echo "  -b basename: base name for tarball [default: $base]"
   echo "  -d dir: export tarball to dir [default: $dest]"
   exit 1
}

[ ! -d .git ] && echo "you must run $0 for the top level of the git repository" && exit 1

while getopts b:d: opt
do
    case "$opt" in
      b)  base=$OPTARG;;
      d)  dest=$OPTARG;;
      \?) usage;;
    esac
done
shift `expr $OPTIND - 1`

[ ! -d $dest ] && echo "missing dest directory $dest" && exit 1

hash="$1"
if [ -z "$hash" ]; then
   hash=$( git tag --points-at HEAD)
   if [ -z "$hash" ]; then
      echo "No tag for HEAD; using hash"
      hash=$( git log -1| head -1| cut -c 8-24 )
   fi
fi

# verify hash
git log -1 $hash 1>/dev/null 2>/dev/null && rc=0 || rc=1
[ $rc -eq 1 ] && echo "bad hash $hash" && exit 1

echo -n "* Export $base-$hash.tar.gz?"
read ans
case $ans in
  Y|y|YES|yes) ;;
  *) echo "tarball not exported"
     exit 1;;
esac
git archive $hash -o $dest/$base-${hash}.tar.gz --prefix=$base/

echo "* Created $dest/$base-${hash}.tar.gz"
#echo "* Add FASTCLICK_VERSION='git.${hash}' to your edge-vars.inc to use"
