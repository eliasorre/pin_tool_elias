#define PIN_DEPRECATED_WARNINGS 0
#include <stdio.h>
#include <map>
#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <vector>
#include <deque>
#include <set>
#include <unordered_map>
#include <inttypes.h>
#include <chrono>
#include "pin.H"

#include "dwarf.h"
#include "libdwarf.h"

using std::string;
using std::map;
using std::ofstream; 
using std::ios;

ofstream OutFile;
ofstream SymTableFile;
ofstream PatternsFile;

// Turn off and on tasks
bool countMemoryReads = false;
bool saveReadAddresses = false;
bool writeJumpAddresses = true;
bool haveRollingBuffer = false;
bool keepLastMOV = true;
bool keepMemoryLoads = true;

// Name of output file
KNOB <string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "first_tool.out", "output file");
KNOB <string> KnobSymtableFile(KNOB_MODE_WRITEONCE, "pintool", "s", "symtable.out", "symtable file");
KNOB <string> KnobPatternsFile(KNOB_MODE_WRITEONCE, "pintool", "patterns", "patterns.out", "patterns file");
KNOB <string> KnobTargetAddress(KNOB_MODE_WRITEONCE, "pintool", "adr", "-1", "Address where the jump happens");
ADDRINT targetAddress; 

map<ADDRINT, int> readAddressMap; 
map<ADDRINT, int> instrMap;
map<ADDRINT, string> mnemonicMap;
map<int, int> loadsSinceLastJmpMap; 

map<int, int> instructionsBetweenJmps;
int granuarilty = 10;

std::set<ADDRINT> jumpAddresses; 
std::set<ADDRINT> tableAddresses; 
std::set<ADDRINT> memoryReadsToR13;

long numberOfJmps = 0;
ADDRINT lastMOV_address = 0;
ADDRINT lastMemoryReadAddress = 0;

bool startCountingWrites = false;
int registerWritesSinceLastJmp = 0;

int startCountingInstructions = true;
int numberOfInstructionsBetweenJmp = 0;

std::vector<std::vector<string>> lastOperationsBeforeJumpVector;
std::deque<string> lastTenInstructions;

ADDRINT mainModuleBase = 0;
ADDRINT mainModuleHigh = 0;


// Callback for loaded images
VOID Image(IMG img, VOID* v) {
    SymTableFile.setf(ios::showbase);
    for( SYM sym= IMG_RegsymHead(img); SYM_Valid(sym); sym = SYM_Next(sym) ) {
        string undFuncName = PIN_UndecorateSymbolName(SYM_Name(sym), UNDECORATION_NAME_ONLY);
        SymTableFile << std::endl;
    }
    SymTableFile.close();

    // Check if it's the main executable
    if (IMG_IsMainExecutable(img)) {
        mainModuleBase = IMG_LowAddress(img);
        mainModuleHigh = IMG_HighAddress(img);
    }
}


// This function increments the times memoryAddr has been read 
VOID incrementMap(ADDRINT readAddress, ADDRINT insAddress) { 
    if (readAddress >= mainModuleBase && readAddress <= mainModuleHigh) {
        readAddressMap[readAddress - mainModuleBase]++; 
    }
    else {
        readAddressMap[readAddress]++; 
    }
    instrMap[insAddress - mainModuleBase]++;
}

void addInstructionToQueue(ADDRINT instructionAddress) {
    lastTenInstructions.push_front(mnemonicMap[instructionAddress]);
    if (lastTenInstructions.size() > 10) {
        lastTenInstructions.pop_back();
    }
}

void commonInstruction(ADDRINT instructionAddress) {
    if (startCountingInstructions) numberOfInstructionsBetweenJmp++;
    if (haveRollingBuffer) addInstructionToQueue(instructionAddress);
}

void updateLastMove(ADDRINT readAddress) { lastMOV_address = readAddress; }
void lastMemoryLoad(ADDRINT memoryReadAddress) { lastMemoryReadAddress = memoryReadAddress; }

void instructionIsSharedJumpPoint(ADDRINT instructionAddress) {
    numberOfJmps++;
    std::cout << "\rCount of jumps: " << numberOfJmps << std::flush;
    if (haveRollingBuffer){
        std::vector<string> recentInstructions(lastTenInstructions.begin(), lastTenInstructions.end());
        lastOperationsBeforeJumpVector.push_back(recentInstructions);
    }
    if (keepLastMOV) tableAddresses.insert(lastMOV_address);
    if (startCountingInstructions) {
        instructionsBetweenJmps[(int) numberOfInstructionsBetweenJmp/granuarilty]++;
        numberOfInstructionsBetweenJmp = 0;
    } 

    startCountingWrites = true;
    loadsSinceLastJmpMap[registerWritesSinceLastJmp]++;
    registerWritesSinceLastJmp = 0;
}

void writeJumpAddress(ADDRINT instructionAddress, ADDRINT regValue) {
    jumpAddresses.insert(regValue);
    instructionIsSharedJumpPoint(instructionAddress);
}

void incrementRegisterWrites() { 
    if(startCountingWrites) registerWritesSinceLastJmp++; 
    if (keepMemoryLoads) memoryReadsToR13.insert(lastMemoryReadAddress);
}

// If instruction is memory read we want to add it to the map
VOID Instruction(INS ins, VOID* v) {
    // Ensure the instruction is from the main module
    if (INS_Address(ins) >= mainModuleBase && INS_Address(ins) <= mainModuleHigh) {  
        if (haveRollingBuffer) mnemonicMap[INS_Address(ins)] = INS_Mnemonic(ins);
        
        // Add the instruction to the rolling buffer
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) commonInstruction,
            IARG_INST_PTR,  
            IARG_END
        );

        if (keepMemoryLoads && INS_IsMemoryRead(ins)) {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) lastMemoryLoad, IARG_MEMORYREAD_EA, IARG_END);
        }
        

        if (keepLastMOV && INS_Mnemonic(ins) == "MOV") {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) updateLastMove, IARG_INST_PTR, IARG_END);
        }


        for (UINT32 i = 0; i < INS_OperandCount(ins); i++) {
            REG reg = INS_OperandReg(ins, i);
            if (INS_OperandWritten(ins, i) && REG_StringShort(reg) == "r13") {
                INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) incrementRegisterWrites, IARG_END); 
            }
        }

        if (INS_Address(ins) - mainModuleBase == targetAddress && writeJumpAddresses) {
            for (UINT32 i = 0; i < INS_OperandCount(ins); i++) {
                REG reg = INS_OperandReg(ins, i);
                if (INS_OperandRead(ins, i)) {
                    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) writeJumpAddress,
                    IARG_INST_PTR,
                    IARG_REG_VALUE, reg, 
                    IARG_END);
                }
            }
        }

        if (INS_IsMemoryRead(ins) && countMemoryReads) {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)incrementMap, 
                IARG_MEMORYREAD_EA, 
                IARG_INST_PTR, 
                IARG_END
            );
        }
    }
}


std::unordered_map<std::string, int> CountPatterns(const std::vector<std::vector<string>>& patterns) {
    std::unordered_map<std::string, int> counts;
    for (const auto& pattern : patterns) {
        std::string key;
        for (const string &insMnemonic : pattern) {
            key+= insMnemonic + ",";
        } 
        counts[key]++;
    }
    return counts;
}

void WriteResultsToFile(const std::unordered_map<std::string, int>& counts, const std::string& filename) {
    PatternsFile.setf(ios::showbase);    
    if (keepLastMOV) {
        PatternsFile << "Addresses read from table" << std::endl;
        for (ADDRINT tableAddress : tableAddresses) {
            PatternsFile << std::hex << tableAddress - mainModuleBase << std::endl;
        }
    }
    PatternsFile << std::endl;
    PatternsFile << "Addresses jumped to:" << std::endl;
    for (ADDRINT jumpAddress : jumpAddresses) {
        PatternsFile << std::hex << jumpAddress - mainModuleBase << std::endl; 
    }
    PatternsFile << std::endl;
    PatternsFile << "Addresses read from into R13:" << std::endl;
    for (ADDRINT memoryReads : memoryReadsToR13) {
        PatternsFile << std::hex << memoryReads << std::endl; 
    }

    PatternsFile << std::endl;
    for (const auto& [pattern, count] : counts) {
        PatternsFile << pattern << ": "<< std::dec << count << "\n";
    }
    PatternsFile << std::endl << "Writes between jumps: writes, count" << std::endl;
    for (const auto& [writes, count] : loadsSinceLastJmpMap) {
        PatternsFile << std::dec << writes << ": " << std::dec << count << "\n";
    }
    
    PatternsFile << std::endl << "Instructions between jumps: writes, count" << std::endl;
    for (const auto& [writes, count] : instructionsBetweenJmps) {
        PatternsFile << std::dec << writes << ": " << std::dec << count << "\n";
    }
    PatternsFile.close();
}

void WriteMemoryReadsToFile() {
    OutFile.setf(ios::showbase);
    OutFile << std::endl; 
    std::cout << "Generating the memory load address counts" << std::endl;
    OutFile << "Memory loads - Address: Count:" << std::endl;
    for (const auto& pair : readAddressMap) {
        OutFile << pair.first << " " << pair.second << std::endl; 
    }
    OutFile << std::endl; 
    std::cout << "Generating the load instruction adress counts" << std::endl;
    OutFile << "Instructions - Address: Count:" << std::endl;
    for (const auto& pair : instrMap) {
        OutFile << pair.first << " " << pair.second << std::endl; 
    }
    OutFile << std::endl; 
    OutFile.close();
}

VOID Fini(INT32 code, VOID* v) {        
    if (countMemoryReads) WriteMemoryReadsToFile();
    
    // Generate the instruction patterns
    if (writeJumpAddresses) {
        std::cout << "Generating the instruction patterns" << std::endl;
        auto patternCounts = CountPatterns(lastOperationsBeforeJumpVector);
        WriteResultsToFile(patternCounts, "output/patterns_count.txt");
    }
}

// Print Help Message
INT32 Usage() {
    std::cerr << "This tool counts the number of memory loads in a map" << std::endl;
    std::cerr << std::endl << KNOB_BASE::StringKnobSummary() << std::endl;
    return -1;
}

int main(int argc, char* argv[]) {
    // Initialize pin
    if (PIN_Init(argc, argv)) return Usage();

    OutFile.open(KnobOutputFile.Value().c_str());
    SymTableFile.open(KnobSymtableFile.Value().c_str());
    PatternsFile.open(KnobPatternsFile.Value().c_str());

    // Find the targeted address
    unsigned long long result;
    errno = 0;
    result = strtoull(KnobTargetAddress.Value().c_str(), nullptr, 16);
    if (result == 0) {
        std::printf("Str was not a number");
        return 1;
    } else if (result == ULLONG_MAX && errno) {
        std::printf("the value of str does not fit in unsigned long long");
    }
    targetAddress = static_cast<ADDRINT>(result);
    std::printf("Target address: %" PRIx64 "\n", targetAddress);
    
    // Fix base adress to calculate the correct addresses
    IMG_AddInstrumentFunction(Image, 0);

    std::printf("Starting PinTool! \n");

    PIN_InitSymbols();
    // Register Instruction to be called instrument instructions
    INS_AddInstrumentFunction(Instruction, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}