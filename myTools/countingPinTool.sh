echo "Running myPinTool"
cd ..
SYMTABLEFILE="${3:-symtable.out}"
./pin -t source/tools/MyPinTool/obj-intel64/MyFirstPinTool.so -o myTools/output/$2 -- /usr/local/bin/python_counting_edition/bin/python3.11 myTools/programs/$1 
