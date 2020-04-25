//===- CFA.cpp---------------===//
#include "llvm/ADT/SCCIterator.h"
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/Analysis/CFG.h"
#include "llvm/Transforms/VI/CFA.h"


using namespace llvm;

char CFGAnalysisForDFA::ID = 0;

bool CFGAnalysisForDFA::runOnFunction(Function &F) {
    if (F.empty())
        return false;
    
    RevTopoSortedBBs.clear();
    PredsOfBBs.clear();
    SuccsOfBBs.clear();
    TopoSortedInsts.clear();
    PredsOfInstructions.clear();
    SuccsOfInstructions.clear();
    ExitInstructions.clear();
    
    //outs() << "CFG Analyses for Function:";
    //outs().write_escaped(F.getName()) << "\n";
    //outs() << "Predecessor Lists:\n";
    for (scc_iterator<Function *> SCCI = scc_begin(&F), SCCIE = scc_end(&F); SCCI != SCCIE; ++SCCI) {
        // Obtain the vector of BBs in this SCC and print it out.
        const std::vector<BasicBlock *> &SCCBBs = *SCCI;
        //outs() << "  SCC: ";
        for (auto BBI = SCCBBs.begin(), BBIE = SCCBBs.end();BBI != BBIE; ++BBI) {
            RevTopoSortedBBs.push_back(*BBI);
            BasicBlock* BB=*BBI;
            //outs() << "BB:" << BB << "-" << *BB << "\n";
            std::vector<BasicBlock*> Preds;
            for (auto PI=pred_begin(BB), PIE=pred_end(BB);PI!=PIE;++PI){
                BasicBlock* PBB=*PI;
                Preds.push_back(PBB);
                //outs() << "Pred BBs:" << PBB <<"-" << *PBB << "\n";
            }
            PredsOfBBs.insert(std::make_pair(BB,Preds));
        }
        //outs() << "\n";
    }
    
    //Generates predecessors of instructions from BB-wise information
    for (std::pair<BasicBlock*, std::vector<BasicBlock*>> pair:PredsOfBBs){
        Instruction* EntryInstOfBB=&(pair.first->front());
        Instruction* PrevInst=EntryInstOfBB;
        Instruction* CurrentInst=EntryInstOfBB;
        //outs() << "Entry Inst:" << *EntryInstOfBB << "\n";
        for (Instruction &Inst:*pair.first){
            std::vector<Instruction*> PredList;
            CurrentInst=&Inst;
            //outs() << "Inst:" << CurrentInst << "-"<< *CurrentInst << "\n";
            if (CurrentInst==EntryInstOfBB){
                for (BasicBlock* Pred:pair.second){
                    //outs() << "Preds:" << Pred->getTerminator() << "-" << *Pred->getTerminator() << "\n";
                    PredList.push_back(Pred->getTerminator());
                }
            }
            else{
                //outs() << "Pred:" << PrevInst << "-" << *PrevInst << "\n";
                PredList.push_back(PrevInst);
            }
            PredsOfInstructions.insert(std::make_pair(CurrentInst, PredList));
            PrevInst=&Inst;
        }
    }
    
    //outs() << "Successor Lists:\n";
    //Generates successor list of instructions and BBs
    for (BasicBlock &BB:F){
        std::vector<BasicBlock*> SuccBBList;
        for (auto I=BB.begin();I!=BB.end();++I){
            Instruction* CurrentInst=&*I;
            //outs() << "Inst:" << CurrentInst << "-" << *CurrentInst << "\n";
            //outs() << "BB:" << &BB << "-" << BB << "\n";
            std::vector<Instruction*> SuccInstList;
            if (CurrentInst==BB.getTerminator()) {
                for (unsigned int i=0;i<BB.getTerminator()->getNumSuccessors();i++){
                    BasicBlock* SuccBB=BB.getTerminator()->getSuccessor(i);
                    SuccBBList.push_back(SuccBB);
                    //outs() << "Succs:" << SuccBB << "-" << *SuccBB  << "\n";
                    SuccInstList.push_back(&(SuccBB->front()));
                }
            }
            else {
                Instruction* NextInst=&*(std::next(I,1));
                SuccInstList.push_back(NextInst);
            }
            SuccsOfInstructions.insert(std::make_pair(CurrentInst, SuccInstList));
        }
        SuccsOfBBs.insert(std::make_pair(&BB, SuccBBList));
    }
    
    //Issue:Misses the instructions with a pred instruction
    //Current Inst:  %16 = load %struct.timespec*, %struct.timespec** %3, align 8, !dbg !1076-0x10c45b078
    //Preds:  br label %15, !dbg !1075-0x10c45b028
    //Preds:  br label %15, !dbg !1075-0x10c45afd8
    //outs() << "Topological Ordered Instructions:\n";
    //Generates instructions list sorted in topological order
    for (auto RI=RevTopoSortedBBs.rbegin();RI!=RevTopoSortedBBs.rend();++RI){
        BasicBlock* BB=*RI;
        TopoSortedBBs.push_back(BB);
        //outs() << BB << "-" << *BB << "\n";
        for (auto I=BB->begin();I!=BB->end();++I){
            Instruction* Inst=&*I;
            TopoSortedInsts.push_back(Inst);
            if (isa<ReturnInst>(Inst) || isa<UnreachableInst>(Inst))
                ExitInstructions.push_back(Inst);
            //outs() << Inst << "-" << *Inst <<"\n";
        }
    }
    return false;
}
static RegisterPass<CFGAnalysisForDFA> D("cfa", "Control Flow Graph Analyses for Iterative Data Flow Frameworks");
