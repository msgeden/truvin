//===-- Utils.h
#ifndef LLVM_TRANSFORMS_VI_UTILS_H
#define LLVM_TRANSFORMS_VI_UTILS_H

#include "llvm/ADT/BitVector.h"
#include "llvm/Support/raw_ostream.h"
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <unordered_map>

#define LLVM_IDENTIFIER_INDEX 2
#define LLVM_IDENTIFIER_SYMBOL "%"
#define LLVM_GLOBAL_SYMBOL "@"
#define LLVM_EQUAL_SYMBOL "="
#define MAX_ARGS 6
#define READ_ACCESS 0
#define WRITE_ACCESS 1


static const char* HomePathEnv="HOME";
static const char* CleanOutputFolder="/LLVM/clean/";
static const char* TaintOutputFolder="/LLVM/taint/";
namespace llvm {
    enum Degree {
        Clean=0,
        May=1,
        Must=2,
    };
    struct PropagationRule{
        bool VariadicArgs;
        int VariadicStartIndex;
        SmallVector<int,MAX_ARGS> SourceArgs; //OR
        SmallVector<int,MAX_ARGS> TaintableArgs; //AND
        Degree Return;
    };
    struct SinkRule{
        bool VariadicArgs;
        int VariadicStartIndex;
        SmallVector<int,MAX_ARGS> CriticalArgs;
    };
    class Utils{
    public:
        static std::string getTimeStampedFilePath(std::string FileName, std::string OutputFolder, std::string ObjectName, std::string Extension);
        static std::string getHomePath();
        static   std::string bitVectorStr(BitVector BitVector);
        static   std::string valueStr(Value* Value);
        static   std::string identifierStr(Value* Value);
        static   Value* findPreceedingMemoryOfVariable(Value* Value);
        static   bool isTopLevelDefinition(Value* Value);
        static   bool isAddressLevelDefinition(Value* Value);
        static   bool compareBitVectors(std::unordered_map<Value*, BitVector> New,std::unordered_map<Value*, BitVector> Prime);
        static   bool isOneOfInstrumentedFunctions(Function* F);
        
        static std::string returnDescription(Degree Degree);
        static std::ofstream openFile(std::string filePath);
        static void closeFile(std::ofstream* file);
        static std::string round(const double number);
    };
    
} // End llvm namespace

#endif
