#!/bin/sh

# bail on errors
set -e

dest="/proj/edgect/share/dpdk/tarballs"

while getopts d: opt
do
    case "$opt" in
      d)  dest=$OPTARG;;
      \?) usage;;
    esac
done
shift `expr $OPTIND - 1`

[ ! -d $dest ] && echo "missing dest directory $dest" && exit 1

hash="$1"
[ -z "$hash" ] && hash=$( git log -1| head -1| cut -c 8-24 )

# verify hash
git log -1 $hash 1>/dev/null 2>/dev/null && rc=0 || rc=1
[ $rc -eq 1 ] && echo "bad hash $hash" && exit 1

echo "* Exporting tarball for commit $hash"
git archive $hash -o $dest/edgelab-elements-git.${hash}.tar.gz

echo "* Created $dest/edgelab-elements-git.${hash}.tar.gz"
echo "* Add EDGELABELEMENTS_VERSION='git.${hash}' to your dpdk-version.inc to use"
