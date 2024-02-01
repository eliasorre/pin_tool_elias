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

using std::string;
using std::map;
using std::ofstream; 
using std::ios;

ofstream OutFile;

// Name of output file
KNOB <string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "first_tool.out", "output file");

// Addresses used for 
ADDRINT mainModuleBase = 0;
ADDRINT mainModuleHigh = 0;

// Callback for loaded images - to find the base and high of the program, and thus calculate offsets
VOID Image(IMG img, VOID* v) {
    if (IMG_IsMainExecutable(img)) {
        mainModuleBase = IMG_LowAddress(img);
        mainModuleHigh = IMG_HighAddress(img);
    }
}

struct PossibleTableLoadIns {
    ADDRINT PC;
    ADDRINT loaded_address;
};

struct TableLoadIns {
    constexpr bool operator()(const TableLoadIns& lhs, const TableLoadIns& rhs) 
    { return std::make_pair(lhs.LOAD_ADDR, lhs.JMP_ADDR) > std::make_pair(rhs.LOAD_ADDR, rhs.JMP_ADDR) ; }

    ADDRINT LOAD_ADDR;
    ADDRINT JMP_ADDR;
};

constexpr bool operator< ( TableLoadIns const& a, TableLoadIns const& b)
    { return std::make_pair(a.LOAD_ADDR, a.JMP_ADDR) < std::make_pair(b.LOAD_ADDR, b.JMP_ADDR); }

std::map<TableLoadIns, INT> tableLoadInstructions;
std::set<ADDRINT> indirectJumps;

int tableLoadIndex = 0;
constexpr int SIZE_OF_POSSIBLETABLELOADS = 50;
std::vector<PossibleTableLoadIns> possibleTableLoadInsVector;


VOID memoryLoad(ADDRINT PC, ADDRINT readAddr, UINT32 readSize) {
    PossibleTableLoadIns loadIns;
    if (readSize != 8) return;
    UINT64 readValue;
    // Safely read the memory content
    if (PIN_SafeCopy(&readValue, reinterpret_cast<void*>(readAddr), readSize) == readSize) {
        // Print the memory read value in hex
        loadIns.PC = PC - mainModuleBase;
        loadIns.loaded_address =  static_cast<ADDRINT>(readValue) - mainModuleBase;
        if (possibleTableLoadInsVector.size() < SIZE_OF_POSSIBLETABLELOADS) {
            possibleTableLoadInsVector.push_back(loadIns);
        } else {
            possibleTableLoadInsVector[tableLoadIndex] = loadIns;
        }
        tableLoadIndex++;
        if (tableLoadIndex == SIZE_OF_POSSIBLETABLELOADS) tableLoadIndex = 0;
    } else {
        std::cout << "Failed to read memory at address " << std::hex << readAddr << std::endl;
    }
}

VOID indirectJump(ADDRINT insAddress, ADDRINT targetAddr) {
    auto loadIns = std::find_if(possibleTableLoadInsVector.begin(), possibleTableLoadInsVector.end(),
                                [targetAddr] (const PossibleTableLoadIns possibleTableLoad) {
                                    return possibleTableLoad.loaded_address == targetAddr - mainModuleBase;
                                });

    if (loadIns != possibleTableLoadInsVector.end()) {
        TableLoadIns tableLoadIns;
        tableLoadIns.LOAD_ADDR = loadIns->PC;
        tableLoadIns.JMP_ADDR = insAddress - mainModuleBase;
        tableLoadInstructions[tableLoadIns]++;
        possibleTableLoadInsVector.clear();
    }
    indirectJumps.insert(insAddress - mainModuleBase); 
}


VOID Instruction(INS ins, VOID* v) {
    // Ensure the instruction is from the main module
    if (INS_Address(ins) >= mainModuleBase && INS_Address(ins) <= mainModuleHigh) {          
        if (INS_IsMemoryRead(ins)) {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) memoryLoad,
                IARG_INST_PTR,
                IARG_MEMORYREAD_EA,
                IARG_UINT32, INS_MemoryReadSize(ins),
                IARG_END
            );
        }
        else if (INS_IsIndirectBranchOrCall(ins)){
            if (!INS_IsCall(ins))INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)indirectJump, IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_END);
        }
    }
}


VOID Fini(INT32 code, VOID* v) {        
    std::cout << "Time to generate results!" << std::endl;
    OutFile.setf(ios::showbase);
    for (const auto [tableLoadAddr, times]  : tableLoadInstructions) {
        OutFile << std::hex << "LoadAddr: " << tableLoadAddr.LOAD_ADDR << " JmpAddr: " << tableLoadAddr.JMP_ADDR << " Times: " << std::dec << times << "\n";
    }
    OutFile << "\n\n\n\n";
    for (ADDRINT indirectJump : indirectJumps) {
        OutFile << std::hex << "JmpInsAddr: " << indirectJump << "\n";
    }
    OutFile.close();
}

// Print Help Message
INT32 Usage() {
    std::cerr << "This tool is used to label memory loads which are connected to bytecode loads (for CPython 3.11)" << std::endl;
    std::cerr << std::endl << KNOB_BASE::StringKnobSummary() << std::endl;
    return -1;
}

int main(int argc, char* argv[]) {
    if (PIN_Init(argc, argv)) return Usage();
    
    OutFile.open(KnobOutputFile.Value().c_str());

    // Fix base adress to enable calculation of offset
    IMG_AddInstrumentFunction(Image, 0);

    std::printf("Starting PinTool! \n");
    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);
    PIN_StartProgram();

    return 0;
}