#!/bin/sh

maintainer=$(grep ^Maintainer: debian/control | sed -e 's,^.*:[ ]*,,g')
package=$(grep ^Package: debian/control | head -n1 | sed -e 's,^.*:[ ]*,,g')
version=${1:-`cat VERSION`}
dist=$(lsb_release -c -s)

cat <<EOF
$package ($version) $dist; urgency=low

  * Current Release

 -- $maintainer  `date -R`

EOF
