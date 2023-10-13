echo "Running myPinTool"
cd ..
SYMTABLEFILE="${3:-symtable.out}"
./pin -t source/tools/MyPinTool/obj-intel64/MyFirstPinTool.so -o myTools/output/$2 -s myTools/output/$SYMTABLEFILE -- myTools/programs/$1 
echo "Creating graphs"
cd myTools
python graph_memory_reads.py $2