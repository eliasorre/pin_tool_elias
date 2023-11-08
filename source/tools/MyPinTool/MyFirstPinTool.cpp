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

// Name of output file
KNOB <string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "first_tool.out", "output file");
KNOB <string> KnobSymtableFile(KNOB_MODE_WRITEONCE, "pintool", "s", "symtable.out", "symtable file");
KNOB <string> KnobPatternsFile(KNOB_MODE_WRITEONCE, "pintool", "patterns", "patterns.out", "patterns file");
KNOB <string> KnobTargetAddress(KNOB_MODE_WRITEONCE, "pintool", "adr", "-1", "Address where the jump happens");
ADDRINT targetAddress; 

map<ADDRINT, int> readAddressMap; 
map<ADDRINT, int> instrMap;
map<ADDRINT, string> mnemonicMap;
std::set<ADDRINT> jumpAddresses; 
std::set<ADDRINT> tableAddresses; 
long numberOfJmps = 0;
ADDRINT lastMOV_address = 0;

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
    if (mnemonicMap[insAddress] == "MOV") {
        lastMOV_address = readAddress;
    }
    instrMap[insAddress - mainModuleBase]++;
}

void instructionIsSharedJumpPoint(ADDRINT instructionAddress) {
    numberOfJmps++;
    std::cout << "\rCount of jumps: " << numberOfJmps << std::flush;
    std::vector<string> recentInstructions(lastTenInstructions.begin(), lastTenInstructions.end());
    lastOperationsBeforeJumpVector.push_back(recentInstructions);
    tableAddresses.insert(lastMOV_address);
}

void addInstructionToQueue(ADDRINT instructionAddress) {
    lastTenInstructions.push_front(mnemonicMap[instructionAddress]);
    if (lastTenInstructions.size() > 10) {
        lastTenInstructions.pop_back();
    }
    if (instructionAddress - mainModuleBase == targetAddress) {
        instructionIsSharedJumpPoint(instructionAddress);
    }
}

void writeJumpAddress(ADDRINT regValue) {
    jumpAddresses.insert(regValue);
}

// If instruction is memory read we want to add it to the map
VOID Instruction(INS ins, VOID* v) {
    // Ensure the instruction is from the main module
    if (INS_Address(ins) >= mainModuleBase && INS_Address(ins) <= mainModuleHigh) {  
        mnemonicMap[INS_Address(ins)] = INS_Mnemonic(ins);
        // Add the instruction to the rolling buffer
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)addInstructionToQueue,
            IARG_INST_PTR,  
            IARG_END
        );

        if (INS_Address(ins) - mainModuleBase == targetAddress) {
            for (UINT32 i = 0; i < INS_OperandCount(ins); i++) {
                REG reg = INS_OperandReg(ins, i);
                if (INS_OperandRead(ins, i)) {
                    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) writeJumpAddress,
                    IARG_REG_VALUE, reg, 
                    IARG_END);
                }
            }
        }

        if (INS_IsMemoryRead(ins)) {
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
    PatternsFile << "Addresses read from table" << std::endl;
    for (ADDRINT tableAddress : tableAddresses) {
        PatternsFile << std::hex << tableAddress - mainModuleBase << std::endl;
    }
    PatternsFile << std::endl;
    PatternsFile << "Addresses jumped to:" << std::endl;
    for (ADDRINT jumpAddress : jumpAddresses) {
        PatternsFile << std::hex << jumpAddress - mainModuleBase << std::endl; 
    }
    PatternsFile << std::endl;
    for (const auto& [pattern, count] : counts) {
        PatternsFile << pattern << ": " << count << "\n";
    }
    PatternsFile.close();
}

VOID Fini(INT32 code, VOID* v) {        
    OutFile.setf(ios::showbase);
    OutFile << std::endl; 
    std::printf("Generating the memory load address counts");
    OutFile << "Memory loads - Address: Count:" << std::endl;
    for (const auto& pair : readAddressMap) {
        OutFile << pair.first << " " << pair.second << std::endl; 
    }
    OutFile << std::endl; 
    std::printf("Generating the load instruction adress counts");
    OutFile << "Instructions - Address: Count:" << std::endl;
    for (const auto& pair : instrMap) {
        OutFile << pair.first << " " << pair.second << std::endl; 
    }
    OutFile << std::endl; 
    OutFile.close();
    
    // Generate the instruction patterns
    std::printf("Generating the instruction patterns");
    auto patternCounts = CountPatterns(lastOperationsBeforeJumpVector);
    WriteResultsToFile(patternCounts, "output/patterns_count.txt");
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