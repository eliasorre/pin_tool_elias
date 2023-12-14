echo "Running myPinTool"
cd ..
SYMTABLEFILE="${3:-symtable.out}"
./pin -t source/tools/MyPinTool/obj-intel64/MyFirstPinTool.so -o myTools/output/$2 -- python3.11 myTools/programs/$1 
