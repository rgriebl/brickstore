#!/bin/zsh

buildType="$1"
targetType="$2"

echo "Fixing iOS archive build"
echo "  current dir: $PWD"
echo "  build tyoe : $buildType"
echo "  target type: $targetType"

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

modules=('bricklink', 'common', 'ldraw', 'modules')
files=('_module_resources_1.build/Objects-normal/x86_64/mocs_compilation.o', \
       '_module_resources_1.build/Objects-normal/x86_64/qrc_qmake_Mobile.o', \
       '_module_qmlcache.build/Objects-normal/x86_64/mocs_compilation.o', \
       '_module_qmlcache.build/Objects-normal/x86_64/mobile_module_qmlcache_loader.o', \
       '_module_resources_2.build/Objects-normal/x86_64/mocs_compilation.o', \
       '_module_resources_2.build/Objects-normal/x86_64/qrc_mobile_module_raw_qml_0.o')

for module in ${modules}; do
  for file in ${files}; do
     common="BrickStore.build/${buildType}-${targetType}/${module}_${file}"
     src="${derivedData}/${common}"
     dst="src/${module}/${common}"

     echo "    $src --> $dst"

     mkdir -p "$(dirname $dst)"
     ln -sf "$PWD/$src" "$dst"
  done
done
