#!/usr/bin/python3

# Copyright (C) 2004-2024 Robert Griebl
# SPDX-License-Identifier: GPL-3.0-only

import os
import re
import sys
import glob
import yaml
import cbor2
import argparse
import requests
import subprocess
import tempfile
from tqdm import tqdm
import shutil


keyre = re.compile('0\\s+!KEYWORDS\\s+')
blre = re.compile('(?i)bricklink\\s+([A-Za-z0-9_.-]+)')
binre = re.compile('(?i)0\\s+bl_item_no\\s+([A-Za-z0-9_.-]+)')

partsmap = { }
officialparts = set()


def download(url, file):
    response = requests.get(url, stream=True)

    if response.status_code != 200:
        sys.exit("Failed to download {}".format(url))
  
    total = int(response.headers.get('content-length', 0))
    blockSize = 1024
    progressBar = tqdm(total=total, unit='iB', unit_scale=True)
    with open(file, 'wb') as fp:
        for data in response.iter_content(blockSize):
            progressBar.update(len(data))
            fp.write(data)
    progressBar.close()


def scanOfficial(dir):
    global partsmap, officalparts, keyre, blre

    # check the regular ldraw parts for a bricklink tag
    for file in glob.iglob(dir + "/parts/*.dat"):
        id = os.path.splitext(os.path.basename(file))[0]

        officialparts.add(id)

        for line in open(file, 'r'):
            keym = re.search(keyre, line)
            if keym:
                kw = line[keym.end():]
                blm = re.search(blre, kw)
                if blm:
                    blid = blm[1]
                    if blid != id:
                        # map the bl part id to the file id
                        partsmap[blid] = id
                        # make sure to not use the file id directly (unless something
                        # else maps to it: e.g. a->b and b->a)
                        if not id in partsmap or partsmap[id] == id:
                            partsmap[id] = None


def scanStudioUnOfficial(dir):
    global partsmap, officalparts, binre

    # check the BL Studio UnOffical parts for a BL_ITEM_NO tagging
    for file in glob.iglob(dir + "/UnOfficial/parts/*.dat"):
        id = os.path.splitext(os.path.basename(file))[0]
        # some parts here have a bl_ prefix in the filename
        id = id.removeprefix('bl_')

        for line in open(file, 'r'):
            binm = re.search(binre, line)
            if binm:
                blid = binm[1]
                if blid != id:
                    # if there is an official part with the same name, we map to that;
                    # otherwise we better don't touch this id
                    if id in officialparts:
                        partsmap[blid] = id
                    else:
                        partsmap[blid] = None
            
                    # make sure to not use the file id directly (unless something
                    # else maps to it: e.g. a->b and b->a)
                    if not id in partsmap or partsmap[id] == id:
                        partsmap[id] = None


def downloadAndUnpack(tempDir):
    ldrawUrl = 'https://www.ldraw.org/library/updates/complete.zip'
    studioUrl = 'https://dzncyaxjqx7p3.cloudfront.net/Studio2.0/Studio+2.0.pkg'

    ldrawFile = "ldraw.zip"
    studioFile = "studio.pkg"
  
    print("Downloading LDraw...")
    download(ldrawUrl, tempDir + '/' + ldrawFile)
    print("Downloading Studio...")
    download(studioUrl, tempDir + '/' + studioFile)

    print("Unpacking LDraw...")
    cp = subprocess.run(['7z', 'x', ldrawFile],
                        cwd=tempDir, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if cp.returncode != 0:
        sys.exit(cp.stdout)
  
    print("Unpacking Studio (stage 1)...")
    cp = subprocess.run(['7z', 'x', studioFile, 'Payload~'],
                        cwd=tempDir, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if cp.returncode != 0:
        sys.exit(cp.stdout)

    studioLDrawDir = 'Applications/Studio 2.0/ldraw'

    print("Unpacking Studio (stage 2)...")
    cp = subprocess.run(['7z', 'x', 'Payload~', './' + studioLDrawDir + '/UnOfficial', '-r'],
                        cwd=tempDir, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if cp.returncode != 0:
        sys.exit(cp.stdout)

    return "ldraw", studioLDrawDir


def main():
    global partsmap
    additionalMaps = []

    parser = argparse.ArgumentParser(description='Create item id mappings between LDraw and BrickLink')
    parser.add_argument('--output', '-o', required=True, metavar='<ldraw.zip>', help="The ouput file, a modified ldraw.zip")
    parser.add_argument('--additional', '-a', action='append', metavar='<mapping.yaml>', help="Integrate an additional, manually curated mapping file")
    parser.add_argument('--temp-base-dir', '-t', required=True, metavar='<dir>', help="A directory, where temporary files are created (needs room for at least 1GB)")

    args = parser.parse_args()

    additionalFiles = []
  
    for additional in args.additional:
        if os.path.isdir(additional):
            additionalFiles.extend(sorted(glob.glob(additional + '/*.yaml')))
        else:
            additionalFiles.append(additional)

    for additionalFile in additionalFiles:
        print("Loading additional mapping file", additionalFile, "...")
        with open(additionalFile, 'r') as fp:
            mapping = yaml.safe_load(fp)
            print("  found", len(mapping), "mappings.")
            additionalMaps.append(mapping)

    with tempfile.TemporaryDirectory(dir=args.temp_base_dir) as tempDir:
        ldrawDir, studioDir = downloadAndUnpack(tempDir)

        print("Scanning ldraw library at", ldrawDir, "...")
        scanOfficial(tempDir + '/' + ldrawDir)
        print("Scanning Studio unofficial parts at", studioDir, "...")
        scanStudioUnOfficial(tempDir + '/' + studioDir)

        print("  found", len(partsmap), "mappings.")

        print("Mergeing mappings...")
        for mapping in additionalMaps:
            partsmap.update(mapping)

        # make the output a bit more readable
        partsmap = { key: partsmap[key] for key in sorted(partsmap) }

        mappingFile = "brickstore-mappings.cbor"
  
        print("Writing mappings to", mappingFile, "...")
        with open(tempDir + '/' +  ldrawDir + '/' + mappingFile, 'wb') as fp:
            cbor2.dump(partsmap, fp)
      
        ldrawZipFile = ldrawDir + '.zip'

        print("Adding mapping file to ldraw.zip...")
        cp = subprocess.run(['7z', 'a', ldrawZipFile, ldrawDir + '/' + mappingFile],
                            cwd=tempDir, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if cp.returncode != 0:
            sys.exit(cp.stdout)

        print("Copying the modified ldraw.zip to", args.output, "...")
        shutil.copy2(tempDir + '/' + ldrawZipFile, args.output)

        print("Finished")

main()
