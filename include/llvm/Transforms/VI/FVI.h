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

#ifndef LLVM_TRANSFORMS_VI_FVI_H
#define LLVM_TRANSFORMS_VI_FVI_H

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instruction.h"
#include <unordered_map>
#include <unordered_set>
#include <set>

namespace llvm {
    class IntraShadowCheckInjectorForAll : public ModulePass {
    public:
        static char ID;
        IntraShadowCheckInjectorForAll():ModulePass(ID){};
        void getAnalysisUsage(AnalysisUsage &AU) const override;
        bool  doInitialization(Module &M) override;
        bool runOnModule(Module &M) override;
    };
} // End llvm namespace
#endif
