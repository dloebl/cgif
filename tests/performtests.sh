#!/bin/sh
#
# Requires:
# - tcc (tiny c compiler)
# - ImageMagick
# - gifsicle
#
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

rAll=0
tcc -c -o cgif.o -I.. ../cgif.c
for sTest in *.c
do
  printf %s "$sTest: "
  basename=`basename $sTest .c`
  tcc -o $basename.out -I.. $sTest cgif.o
  idontcare=`./$basename.out`
  r=$?
  #
  # Check GIF with ImageMagick
  identify $basename.gif > /dev/null 2> /dev/null
  r=$(($r + $?))
  #
  # Check GIF with gifsicle
  gifsicle --no-ignore-errors --info $basename.gif -o /dev/null 2> /dev/null
  r=$(($r + $?))
  rm -f $basename.out
  rAll=$(($rAll + $r))
  if [ $r != 0 ]
  then
    printf "${RED}FAILED${NC}\n" 
  else
    printf "${GREEN}OK${NC}\n"
  fi
done
rm -f cgif.o
#
# check MD5 hashes
md5sum -c --quiet tests.md5
rAll=$(($rAll + $?))
rm *.gif
echo "-------------------------------"
printf "result of all tests: "
if [ $rAll != 0 ]
then
  printf "${RED}FAILED${NC}\n"
  exit 1 # error
else
  printf "${GREEN}OK${NC}\n"
  exit 0 # O.K.
fi	
