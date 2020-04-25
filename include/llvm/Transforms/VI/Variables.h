//===-- Analysis/CFG.h - BasicBlock Analyses --------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This family of functions performs analyses on basic blocks, and instructions
// contained within basic blocks.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_VI_VARIABLES_H
#define LLVM_TRANSFORMS_VI_VARIABLES_H

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instruction.h"
#include <unordered_map>

namespace llvm {
    class VariableNamesAnalysis : public FunctionPass {
        //Map of varibles names and IR identifiers
        std::unordered_map<Value*,std::string> LocalVariables;
    public:
        static char ID;
        VariableNamesAnalysis():FunctionPass(ID){};
        std::unordered_map<Value*,std::string> getLocalVariables(){
            return LocalVariables;
        }
        bool runOnFunction(Function &F) override;
    };
} // End llvm namespace

#endif
