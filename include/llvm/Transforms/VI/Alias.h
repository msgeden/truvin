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

#ifndef LLVM_TRANSFORMS_VI_ALIAS_H
#define LLVM_TRANSFORMS_VI_ALIAS_H

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instruction.h"
#include <unordered_map>
#include <unordered_set>

namespace llvm {
    class PointsToSets : public FunctionPass {
        //Points-To Sets of May Alias
        std::unordered_map<Value*,std::unordered_set<Value*>> MayAliasSet;
        //Points-To Sets of Must Alias
        std::unordered_map<Value*,std::unordered_set<Value*>> MustAliasSet;
        
    public:
        static char ID;
        PointsToSets():FunctionPass(ID){};
        
        //Get Functions Of Generated Analyses
        std::unordered_map<Value*,std::unordered_set<Value*>> getMayAliasSet(){
            return MayAliasSet;
        }
        std::unordered_map<Value*,std::unordered_set<Value*>> getMustAliasSet(){
            return MustAliasSet;
        }
        
        void getAnalysisUsage(AnalysisUsage &AU) const override;
        bool runOnFunction(Function &F) override;
    };
} // End llvm namespace

#endif
