#!/bin/bash

#set -x

failcnt=0

function status() 
{
  if [ "$?" == "0" ]; then
    echo -e "\e[32m$1\e[0m"
  else
    echo -e "\e[31m$2\e[0m"
    failcnt=$((failcnt + 1))
  fi
}

[ ! -e VERSION ] && { echo "Please call this script from the root source dir."; exit 2; }

version=$(cat VERSION)

echo -n "Checking release version... "
[ -n "$version" ]
status "$version" "VERSION is empty"

echo -n "Checking if git workdir is clean... "
[ -z "$(git status --porcelain)" ]
status "clean" "dirty - check git status"

echo -n "Fetching git tags... "
git fetch --tags origin >/dev/null 2>/dev/null 
status "done" "failed"

echo -n "Checking git release tag v$version... "
! git tag -l | grep "^v" | cut -c2- | grep -s -q "$version" 
status "ok" "tag already exists"

echo -n "Checking CHANGELOG main entry... "
grep -s -q CHANGELOG.md -e "^## \\[$version\\]"
status "ok" "not found"

echo -n "Checking CHANGELOG main entry date... "
grep -s -q CHANGELOG.md -e "^## \\[$version\\] - $(date +%Y-%m-%d)\$" || fail "not from today!"
status "ok" "not from today"

echo -n "Checking CHANGELOG link entry... "
grep -s -q CHANGELOG.md -e "^\\[$version\\]: https://github.com/rgriebl/brickstore/releases/tag/v"
status "ok" "not found"

echo -n "Checking CHANGELOG link entry tag... "
grep -s -q CHANGELOG.md -e "^\\[$version\\]: https://github.com/rgriebl/brickstore/releases/tag/v$version\$"
status "ok" "links wrong tag"

echo

if [ "$failcnt" != "0" ]; then
  echo "Found $failcnt problem(s)"
  echo
  false ; status "" "RELEASE PROCESS ABORTED"
  echo
  exit 2
fi

echo "No problems found"
echo
status "RELEASE $version IS READY TO GO"
echo

read -p "Tag and push to GitHub now? (y/N) " -n 1 -r
if ! [[ $REPLY =~ ^[Yy]$ ]]; then
  echo
  echo "ABORTED BY USER"
  exit 1
fi

echo
echo

echo -n "Creating a git tag... "
git tag v$version
status "v$version" "failed"

echo -n "Pushing tag to GitHub... "
git push origin v$version
status "ok" "failed"

echo
status "RELEASE DONE"
echo
echo "Check GitHub for build progress: https://github.com/rgriebl/brickstore/actions"
echo
