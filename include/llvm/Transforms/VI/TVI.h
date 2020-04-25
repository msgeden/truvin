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

#ifndef LLVM_TRANSFORMS_VI_TVI_H
#define LLVM_TRANSFORMS_VI_TVI_H

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Transforms/VI/Utils.h"
#include <unordered_map>
#include <unordered_set>
#include <set>

namespace llvm {
    class FunctionProgrammerVariables : public FunctionPass {
    public:
        static char ID;
        FunctionProgrammerVariables():FunctionPass(ID){};
        //Get Functions Of Generated Analyses
        void getAnalysisUsage(AnalysisUsage &AU) const override;
        bool runOnFunction(Function &F) override;
    };
    class ModuleProgrammerVariables : public ModulePass {
        //Programmer-defined variables
        std::unordered_set<Value*> ProgrammerVariables;
        //Other variables
        std::unordered_set<Value*> NonProgrammerVariables;
    public:
        static char ID;
        ModuleProgrammerVariables():ModulePass(ID){};
        //Get Functions Of Generated Analyses
        std::unordered_set<Value*> getProgrammerVariables(){
            return ProgrammerVariables;
        }
        std::unordered_set<Value*> getNonProgrammerVariables(){
            return NonProgrammerVariables;
        }
        void getAnalysisUsage(AnalysisUsage &AU) const override;
        bool runOnModule(Module &M) override;
    };
    

    class CleanDataPropagationModeller : public FunctionPass {
        std::unordered_map<Value*,int> DomainIndices;
        //Domain items of indexed bitvectors
        std::unordered_map<int,Value*> BitVectorItems;
        
        std::unordered_map<Instruction*, BitVector> ReachingInCleanData;
        
        std::unordered_map<Instruction*, BitVector> ReachingOutCleanData;
        
        std::vector<Instruction*> TopoSortedInsts;
        
        std::unordered_map<BasicBlock*, std::vector<BasicBlock*>> PredsofBBs;
        
        std::unordered_map<Value*,std::string> LocalVariables;
    public:
        static char ID;
        CleanDataPropagationModeller():FunctionPass(ID){};
        std::unordered_map<Value*,int> getDomainIndices(){
            return DomainIndices;
        }
        std::unordered_map<int, Value*> getBitVectorItems(){
            return BitVectorItems;
        }
        std::unordered_map<Instruction*, BitVector> getReachingInCleanData(){
            return ReachingInCleanData;
        }
        std::unordered_map<Instruction*, BitVector> getReachingOutCleanData(){
            return ReachingOutCleanData;
        }
        std::vector<Instruction*> getTopoSortedInsts(){
            return TopoSortedInsts;
        }
        std::unordered_map<BasicBlock*, std::vector<BasicBlock*>> getPredsOfBBs(){
            return PredsofBBs;
        }
        std::unordered_map<Value*,std::string> getLocalVariables(){
            return LocalVariables;
        }
        void getAnalysisUsage(AnalysisUsage &AU) const override;
        //bool doInitialization(Module &M) override;
        bool runOnFunction(Function &F) override;
    };
    
    class ModuleCleanDataChecker : public ModulePass {
        //Memory instructions reading clean data
        std::unordered_set<LoadInst*> CleanLoads;
        //Memory instructions writing clean data
        std::unordered_set<StoreInst*> CleanStores;
        //Memory instructions writing/reading clean data
        std::unordered_map<Value*, std::vector<Instruction*>> CleanMemoryOps;
        //Memory instructions writing/reading clean data
        std::map<Value*, int> VariableIds;
    public:
        static char ID; // Pass identification, replacement for typeid
        ModuleCleanDataChecker() : ModulePass(ID) {}
        std::unordered_set<LoadInst*> getCleanLoads(){
            return CleanLoads;
        }
        std::unordered_set<StoreInst*> getCleanStores(){
            return CleanStores;
        }
        std::unordered_map<Value*, std::vector<Instruction*>> getCleanMemoryOps(){
            return CleanMemoryOps;
        }
        std::map<Value*, int> getVariableIds(){
            return VariableIds;
        }
        void getAnalysisUsage(AnalysisUsage &AU) const override;
        bool runOnModule(Module &M) override;
    };
} // End llvm namespace

#endif
