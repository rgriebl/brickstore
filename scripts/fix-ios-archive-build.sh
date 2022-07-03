#!/bin/zsh

cd /derivedData/ArchiveIntermediates/BrickStore/IntermediateBuildFilesPath
for i in $(find */*/*_module_{resources,qmlcache}*(N) -name "*.o"); do
  j=$(echo $i | sed -e 's,.*/([^_]*)_module_.*,\1,')
  dst="../../../../src/$j/$i"
  mkdir -p $(dirname $dst)
  ln -sf $PWD/$i $dst
done
