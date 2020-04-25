//===- Utils.cpp
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"

#include "llvm/Transforms/VI/Utils.h"

#include <string>
#include <sstream>
#include <chrono>
#include <ctime>
#include <time.h>

using namespace llvm;
std::string Utils::getTimeStampedFilePath(std::string FileName, std::string OutputFolder, std::string ObjectName, std::string Extension){
    time_t now = time(0);
    // convert now to string form
    char* dt = ctime(&now);
    std::string Date(dt);
    //return (getHomePath()+OutputFolder+FileName+"_"+ObjectName+"_"+Date+"."+Extension);
    return (getHomePath()+OutputFolder+FileName+"_"+ObjectName+"."+Extension);
}
std::string Utils::getHomePath(){
    std::string Path(getenv(HomePathEnv));
    return Path;
}
std::string Utils::bitVectorStr(BitVector BitVector){
    std::stringstream ss;
    for (unsigned int i=0;i<BitVector.size();i++)
        ss << BitVector[i];
    return ss.str();
}
bool Utils::compareBitVectors(std::unordered_map<Value *, BitVector> New, std::unordered_map<Value *, BitVector> Prime){
    bool Equality=true;
    for (std::pair<Value*, BitVector> pair:New){
        Equality&=(pair.second.operator==(Prime.at(pair.first)));
    }
    return Equality;
}
std::string Utils::valueStr(Value* Value){
    std::string ValueStr;
    raw_string_ostream RawStringStream(ValueStr);
    Value->print(RawStringStream);
    return ValueStr;
}

std::string Utils::identifierStr(Value* Value){
    std::string RetStr="";
    std::string ValueStr;
    raw_string_ostream RawStringStream(ValueStr);
    Value->print(RawStringStream);
    
    if (isa<Instruction>(Value)){
        std::size_t IdentifierFound = ValueStr.find(LLVM_IDENTIFIER_SYMBOL);
        if (IdentifierFound!=std::string::npos && IdentifierFound==LLVM_IDENTIFIER_INDEX){
            std::size_t EqualFound = ValueStr.find(LLVM_EQUAL_SYMBOL);
            if (EqualFound!=std::string::npos){
                RetStr=ValueStr.substr(LLVM_IDENTIFIER_INDEX,EqualFound-(LLVM_IDENTIFIER_INDEX+1));
            }
        }
    }
    else if (isa<Argument>(Value)){
        std::size_t IdentifierFound = ValueStr.find(LLVM_IDENTIFIER_SYMBOL);
        if (IdentifierFound!=std::string::npos){
            RetStr=ValueStr.substr(IdentifierFound,ValueStr.size());
        }
    }
    else if (isa<GlobalVariable>(Value)||isa<GlobalAlias>(Value)){
        std::size_t IdentifierFound = ValueStr.find(LLVM_GLOBAL_SYMBOL);
        if (IdentifierFound!=std::string::npos){
            std::size_t EqualFound = ValueStr.find(LLVM_EQUAL_SYMBOL);
            if (EqualFound!=std::string::npos){
                RetStr=ValueStr.substr(0,EqualFound-1);
            }
        }
    }
    else if (isa<Constant>(Value)){
        RetStr="%C";
    }
    return RetStr;
}
bool Utils::isTopLevelDefinition(Value* Value){
    if (isa<Instruction>(Value)){
        std::string InstStr;
        raw_string_ostream RawStringStream(InstStr);
        Value->print(RawStringStream);
        std::size_t IdentifierFound = InstStr.find(LLVM_IDENTIFIER_SYMBOL);
        if (IdentifierFound!=std::string::npos && IdentifierFound==LLVM_IDENTIFIER_INDEX)
            return true;
    }
    return false;
}
bool Utils::isAddressLevelDefinition(Value* Value){
    if (isa<StoreInst>(Value)){
        return true;
    }
    return false;
}
bool Utils::isOneOfInstrumentedFunctions(Function* F){
    if (F->getName().startswith("vi_call") || F->getName().contains("sgx_") || F->getName().startswith("ocall_") || F->getName().startswith("ecall_") || F->getName().startswith("upfs_") ||  F->getName().contains("enclave") ||  F->getName().startswith("Enclave_") || 
        F->getName()=="printf_helloworld" || F->getName()=="printf_on_terminal" || F->getName()=="printf_on_file" || F->getName()=="set_shadow_memory" || F->getName()=="free_shadow_memory" || F->getName()=="destroy_enclave" || F->getName()=="initialize_enclave" || F->getName()=="print_error_message"){
        return true;
    }
    else
        return false;
}
std::string Utils::returnDescription(Degree Degree){
    if (Degree==Clean)
        return "Clean";
    else if (Degree==May)
        return "May";
    else if (Degree==Must)
        return "Must";
    else
        return "";
}
std::ofstream Utils::openFile(std::string FilePath){
    std::ofstream File;
    File.open(FilePath);
    return File;
}
void Utils::closeFile(std::ofstream* File){
    File->close();
}
std::string Utils::round(const double number){
    std::stringstream ss;
    ss << std::fixed;
    ss.precision(2); // set # places after decimal
    ss << number;
    return ss.str();
}
