#!/usr/bin/env python3

import hashlib
import re
import sys

# returns 0 if the md5sums are identical
def checkFile(sExpMD5Hex, sFileName):
  try:
    f = open(sFileName, "rb")
  except OSError:
    print("%s: FAILED to open" % sFileName)
    return 2
  byteData = f.read()
  f.close()
  # compare MD5 HEX digest
  sRealMD5Hex = hashlib.md5(byteData).hexdigest()
  if sRealMD5Hex == sExpMD5Hex:
    print("%s: OK" % sFileName)
    return 0
  else:
    print("%s: FAILED" % sFileName)
    return 3

# handle input line in tests.md5
def handleLine(sLine):
  reComment = re.match("^\\s*#", sLine)
  if reComment != None:
    return 0 # line is comment
  # extract MD5 HEX digest and filename
  mLine = re.fullmatch("^\\s*([0-9a-f]{32})\\s*(.*)\\n$", sLine)
  if mLine == None:
    return 1 # error
  # extract info
  sMD5Hex   = mLine.group(1)
  sFileName = mLine.group(2)
  return checkFile(sMD5Hex, sFileName)


# ./md5sum.py <input-checksum-file>
if len(sys.argv) != 2:
  print("Invalid number of arguments.")
  exit(1)
# try to open md5 input file
try:
  f = open(sys.argv[1], "rt")
except OSError:
  print("Failed to open <input-checksum-file>")
  exit(2)
r = 0
sNextLine = f.readline()
while sNextLine != "":
  r |= handleLine(sNextLine)
  sNextLine = f.readline()
f.close()
if r:
  print("md5sum.py: ERROR: At least 1 computed checksum did NOT match or error while parsing input checksums.")
  exit(3)
