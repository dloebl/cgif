#!/usr/bin/env python3

import hashlib
import re
import sys

# returns 0 if the md5sums are identical
def checkFile(sExpSHA256Hex, sFileName):
  try:
    f = open(sFileName, "rb")
  except OSError:
    print("%s: FAILED to open" % sFileName)
    return 2
  byteData = f.read()
  f.close()
  # compare SHA256 HEX digest
  sRealSHA256Hex = hashlib.sha256(byteData).hexdigest()
  if sRealSHA256Hex == sExpSHA256Hex:
    print("%s: OK" % sFileName)
    return 0
  else:
    print("%s: FAILED" % sFileName)
    return 3

# handle input line
def handleLine(sLine):
  reComment = re.match("^\\s*#", sLine)
  if reComment != None:
    return 0 # line is comment
  # extract SHA256 HEX digest and filename
  mLine = re.fullmatch("^\\s*([0-9a-fA-F]{64})\\s*(.*)\\n$", sLine)
  if mLine == None:
    return 1 # error
  # extract info
  sSHA256Hex = mLine.group(1)
  sFileName = mLine.group(2)
  return checkFile(sSHA256Hex, sFileName)

# ./sha256sum.py <input-checksum-file>
if len(sys.argv) != 2:
  print("Invalid number of arguments.")
  exit(1)
# try to open sha256 input file
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
  print("sha256sum.py: ERROR: At least 1 computed checksum did NOT match or error while parsing input checksums.")
  exit(3)
