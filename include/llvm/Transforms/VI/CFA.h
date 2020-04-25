//===-- Transforms/CFA.h 
#ifndef LLVM_TRANSFORMS_VI_CFA_H
#define LLVM_TRANSFORMS_VI_CFA_H

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instruction.h"
#include <unordered_map>

namespace llvm {
    class CFGAnalysisForDFA : public FunctionPass {
        std::vector<BasicBlock*> RevTopoSortedBBs;
        //Basic blocks sorted in reverse topological order
        std::vector<BasicBlock*> TopoSortedBBs;
        //Predecessors of basic blocks
        std::unordered_map<BasicBlock*, std::vector<BasicBlock*>> PredsOfBBs;
        //Successors of basic blocks
        std::unordered_map<BasicBlock*, std::vector<BasicBlock*>> SuccsOfBBs;
        //Instructions sorted in topological order
        std::vector<Instruction*> TopoSortedInsts;
        //Predecessors of instructions
        std::unordered_map<Instruction*, std::vector<Instruction*>> PredsOfInstructions;
        //Successors of instructions
        std::unordered_map<Instruction*, std::vector<Instruction*>> SuccsOfInstructions;
        //Exit instructions
        std::vector<Instruction*> ExitInstructions;
    public:
        static char ID;
        CFGAnalysisForDFA():FunctionPass(ID){};

        //Get Functions Of Generated Analyses
        std::vector<BasicBlock*> getRevTopoSortedBBs(){
            return RevTopoSortedBBs;
        }
        std::vector<BasicBlock*> getTopoSortedBBs(){
            return TopoSortedBBs;
        }
        std::vector<Instruction*> getTopoSortedInstructions(){
            return TopoSortedInsts;
        }
        std::unordered_map<BasicBlock*, std::vector<BasicBlock*>> getPredsOfBBs(){
            return PredsOfBBs;
        }
        std::unordered_map<BasicBlock*, std::vector<BasicBlock*>> getSuccsOfBBs(){
            return SuccsOfBBs;
        }
        std::unordered_map<Instruction*, std::vector<Instruction*>> getPredsOfInstructions(){
            return PredsOfInstructions;
        }
        std::unordered_map<Instruction*, std::vector<Instruction*>> getSuccsOfInstructions(){
            return SuccsOfInstructions;
        }
        std::vector<Instruction*> getExitInstructions(){
            return ExitInstructions;
        }
        bool runOnFunction(Function &F) override;
    };
} // End llvm namespace
#endif
