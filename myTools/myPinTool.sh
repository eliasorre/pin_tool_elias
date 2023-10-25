echo "Running myPinTool"
cd ..
SYMTABLEFILE="${3:-symtable.out}"
./pin -t source/tools/MyPinTool/obj-intel64/MyFirstPinTool.so -o myTools/output/$2 -s myTools/output/$SYMTABLEFILE -adr $3 -- myTools/programs/$1 
echo "Creating graphs"
cd myTools
python3 graph_memory_reads.py $2