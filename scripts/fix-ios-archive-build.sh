#!/bin/zsh

buildType="$1"
targetType="$2"
arch="$3"

echo "Fixing iOS archive build"
echo "  current dir : $PWD"
echo "  build type  : $buildType"
echo "  target type : $targetType"
echo "  architecture: $arch"

derivedData="derivedData/ArchiveIntermediates/BrickStore/IntermediateBuildFilesPath"

echo "  derived dir: $derivedData"
echo
echo "  created links:"

#cd $derivedData
#for i in $(find */*/*_module_{resources,qmlcache}*(N) -name "*.o"); do
#  j=$(echo $i | sed -e 's,.*/([^_]*)_module_.*,\1,')
#  dst="../../../../src/$j/$i"

#  echo "    $PWD/$i --> $dst"

#  mkdir -p $(dirname $dst)
#  ln -sf $PWD/$i $dst
#done

buildModules=('BrickLink/bricklink' 'BrickStore/common' 'LDraw/ldraw' 'Mobile/mobile')
files=('@bm@_module_resources_1.build/Objects-normal/@arch@/mocs_compilation.o' \
       '@bm@_module_resources_1.build/Objects-normal/@arch@/qrc_qmake_@BM@.o' \
       '@bm@_module_qmlcache.build/Objects-normal/@arch@/mocs_compilation.o' \
       '@bm@_module_qmlcache.build/Objects-normal/@arch@/@bm@_module_qmlcache_loader.o' \
       '@bm@_module_resources_2.build/Objects-normal/@arch@/mocs_compilation.o' \
       '@bm@_module_resources_2.build/Objects-normal/@arch@/qrc_@bm@_module_raw_qml_0.o')

for buildModule in ${buildModules}; do
  BM=$(echo $buildModule | cut -d / -f 1)
  bm=$(echo $buildModule | cut -d / -f 2)

  for file in ${files}; do
     f=$(echo $file | sed -e "s,@BM@,${BM},g" -e "s,@bm@,${bm},g" -e "s,@arch@,${arch},g")

     common="BrickStore.build/${buildType}-${targetType}/${f}"
     src="${derivedData}/${common}"
     dst="src/${bm}/${common}"

     echo "    $src --> $dst"

     mkdir -p "$(dirname $dst)"
     ln -sf "$PWD/$src" "$dst"
  done
done
