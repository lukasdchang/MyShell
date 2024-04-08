#!/bin/bash
cd bunchatests
echo Running a buncha tests now:
echo

echo Testing wildcards:
echo All files that end in txt:
ls *.txt
echo All files that end in .sh:
ls *.sh
echo All files that start with the letter o:
ls o*
echo All files that have input?.txt
ls input?.txt
echo 

echo Testing redirection adding the contents of input.txt to output.txt:
echo input.txt and output.txt contents:
cat input.txt
cat output.txt
echo Redirecting...
cat input.txt > output.txt
echo input.txt and output.txt contents after redirection:
cat input.txt
cat output.txt
echo

echo Testing pipelines:
echo No current test exist yet
echo

echo Testing conditionals:
echo Testing 'then' conditional:
true
then echo Success: This message should print because the previous command succeeded.

echo Testing 'else' conditional:
false
else echo Failure: This message should print because the previous command failed.
echo

echo Testing "ls -l":
ls -l 
echo

echo Testing "cd" and "pwd":
pwd
cd directory1
pwd
cd ..
echo


echo Testing "which":
which bash
echo
