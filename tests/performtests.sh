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

cfiles=`ls *.c`
rAll=0
tcc -c -o cgif.o -I.. ../cgif.c
for sTest in $cfiles
do
  printf %s "$sTest: "
  tcc -o a.out -I.. $sTest cgif.o
  idontcare=`./a.out`
  r=$?
  #
  # Check GIF with ImageMagick
  identify out.gif > /dev/null 2> /dev/null
  r=$(($r + $?))
  #
  # Check GIF with gifsicle
  gifsicle --info out.gif -o /dev/null 2> /dev/null
  r=$(($r + $?))
  rm -f out.gif
  rm -f a.out
  rAll=$(($rAll + $r))
  if [ $r != 0 ]
  then
    printf "${RED}FAILED${NC}\n" 
  else
    printf "${GREEN}OK${NC}\n"
  fi
done
rm -f cgif.o
echo "-------------------------------"
printf "result of all tests: "
if [ $rAll != 0 ]
then
  printf "${RED}FAILED${NC}\n"
else
  printf "${GREEN}OK${NC}\n"
fi	
