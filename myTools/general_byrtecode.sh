echo "Running myPinTool"
cd ..
./pin -t source/tools/MyPinTool/obj-intel64/generalBytecodeLoads.so -o myTools/output/$2 -- myTools/programs/$1 
