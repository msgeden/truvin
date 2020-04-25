//===- Hello.cpp - Example code from "Writing an LLVM Pass" ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements two versions of the LLVM "Hello World" pass described
// in docs/WritingAnLLVMPass.html
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/CFLSteensAliasAnalysis.h"
#include "llvm/Analysis/CFLAndersAliasAnalysis.h"
#include "llvm/Analysis/ScalarEvolutionAliasAnalysis.h"
#include "llvm/Analysis/TypeBasedAliasAnalysis.h"

#include "llvm/Transforms/VI/Alias.h"
#include "llvm/Transforms/VI/CFA.h"
#include "llvm/Transforms/VI/Utils.h"
#include <unordered_set>
#include <unordered_map>

//#include "llvm/Analysis/DSGraph.h"

using namespace llvm;

char PointsToSets::ID = 0;
void PointsToSets::getAnalysisUsage(AnalysisUsage &AU) const {
    //AU.addRequired<AAResultsWrapperPass>();
    AU.addRequired<BasicAAWrapperPass>();
    AU.addRequired<CFLAndersAAWrapperPass>();
    AU.addRequired<CFLSteensAAWrapperPass>();
    AU.addRequired<SCEVAAWrapperPass>();
    AU.addRequired<TypeBasedAAWrapperPass>();
    AU.setPreservesAll();
}
bool PointsToSets::runOnFunction(Function &F) {
    if (F.empty())
        return false;
    outs() << "************************\n";
    outs() << "Alias analyses analyses for function: ";
    outs().write_escaped(F.getName()) << "\n";
    
    MayAliasSet.clear();
    MustAliasSet.clear();
    //TODO:Replace with a flow-sensitive alias analysis
    //AAResults &AA = getAnalysis<AAResultsWrapperPass>().getAAResults();
    //BasicAAResult &AA = getAnalysis<BasicAAWrapperPass>().getResult();
    //CFLAndersAAResult &AA = getAnalysis<CFLAndersAAWrapperPass>().getResult();
    CFLSteensAAResult &AA = getAnalysis<CFLSteensAAWrapperPass>().getResult();
    //SCEVAAResult &AA = getAnalysis<SCEVAAWrapperPass>().getResult();
    //TypeBasedAAResult &AA = getAnalysis<TypeBasedAAWrapperPass>().getResult();
    std::vector<MemoryLocation> MemoryLocations;
    std::unordered_set<Value*> Args;
    outs() << "Args:\n";
    
    for (Function::arg_iterator I = F.arg_begin(); I != F.arg_end(); ++I){
        Value* Arg = I;
        //Type* ArgType=Arg->getType();
        //if (isa<PointerType>(ArgType))
        Args.insert(Arg);
    }
    for (inst_iterator I=inst_begin(F),E=inst_end(F);I!=E;++I){
        Instruction* Inst=&(*I);
        //outs() << "Inst: " << *Inst << "\n";
        if (StoreInst* Store=dyn_cast<StoreInst>(Inst)){
            std::unordered_set<Value*> AliasSet;
            if (Args.find(Store->getValueOperand())!=Args.end()){
                AliasSet.insert(Store->getPointerOperand());
                MayAliasSet.insert(std::make_pair(Store->getValueOperand(), AliasSet));
                MustAliasSet.insert(std::make_pair(Store->getValueOperand(), AliasSet));
            }
        }
        Optional<MemoryLocation> MemLoc=MemoryLocation::getOrNone(Inst);
        if (MemLoc!=None){
            MemoryLocations.push_back(MemLoc.getValue());
            //outs() << "MemLoc:" << *Inst << "\n";
        }
    }
    
    
    for (MemoryLocation Pointer:MemoryLocations){
        Value* PointerVal=const_cast<Value*>(Pointer.Ptr);
        std::unordered_set<Value*> MaySet;
        std::unordered_set<Value*> MustSet;
        for (MemoryLocation To:MemoryLocations){
            Value* ToVal=const_cast<Value*>(To.Ptr);
            if (AA.query(Pointer, To)==MayAlias && Pointer.Ptr!=To.Ptr){
                if (MayAliasSet.find(PointerVal)==MayAliasSet.end()){
                    MaySet.insert(ToVal);
                    MayAliasSet.insert(std::make_pair(PointerVal, MaySet));
                }
                else {
                    MaySet=MayAliasSet.at(PointerVal);
                    MaySet.insert(ToVal);
                    MayAliasSet.at(PointerVal)=MaySet;
                }
            }
            if (AA.query(Pointer, To)==MustAlias && Pointer.Ptr!=To.Ptr){
                if (MustAliasSet.find(PointerVal)==MustAliasSet.end()){
                    MustSet.insert(ToVal);
                    MustAliasSet.insert(std::make_pair(PointerVal, MustSet));
                }
                else {
                    MustSet=MustAliasSet.at(PointerVal);
                    MustSet.insert(ToVal);
                    MustAliasSet.at(PointerVal)=MustSet;
                }
            }
        }
    }
    //Update the set of arguments with the set of first alias found
    for (Value* Arg:Args){
        if (MayAliasSet.find(Arg)!=MayAliasSet.end()){
            std::unordered_set<Value*> ArgMaySet=MayAliasSet.at(Arg);
            Value* MayFirstAlias=*(MayAliasSet.at(Arg).begin());
            if (MayAliasSet.find(MayFirstAlias)!=MayAliasSet.end()){
                std::unordered_set<Value*> AliasMaySet=MayAliasSet.at(MayFirstAlias);
                for (Value* Item:AliasMaySet){
                    ArgMaySet.insert(Item);
                }
            }
            MayAliasSet.at(Arg)=ArgMaySet;
        }
        if (MustAliasSet.find(Arg)!=MustAliasSet.end()){
            std::unordered_set<Value*> ArgMustSet=MustAliasSet.at(Arg);
            Value* MustFirstAlias=*(MustAliasSet.at(Arg).begin());
            if (MustAliasSet.find(MustFirstAlias)!=MustAliasSet.end()){
                std::unordered_set<Value*> AliasMustSet=MustAliasSet.at(MustFirstAlias);
                for (Value* Item:AliasMustSet){
                    ArgMustSet.insert(Item);
                }
            }
            MustAliasSet.at(Arg)=ArgMustSet;
        }
    }
    outs() << "May Alias Groups\n";
    for (std::pair<Value*, std::unordered_set<Value*>> pair:MayAliasSet){
        outs() <<  Utils::identifierStr(pair.first) << "->{ ";
        for (Value* To:pair.second){
            outs() <<  Utils::identifierStr(To) << " ";
        }
        outs() << "}\n";
    }
    outs() << "Must Alias Groups\n";
    for (std::pair<Value*, std::unordered_set<Value*>> pair:MustAliasSet){
        outs() <<  Utils::identifierStr(pair.first) << "->{ ";
        for (Value* To:pair.second){
            outs() <<  Utils::identifierStr(To) << " ";
        }
        outs() << "}\n";
    }
    return false;
}
static RegisterPass<PointsToSets> AVI("points2set", "A Wrapper Pass of CFL based Pointer Analysis");

//template<typename Value*, typename Value*>
struct PointsTo{
    Value* Pointer;
    Value* Location;
    // constructor
    PointsTo(Value* p, Value* l)
    {
        this->Pointer = p;
        this->Location = l;
    }
    
    // operator== is required to compare keys in case of hash collision
    bool operator==(const PointsTo &p) const
    {
        return Pointer == p.Pointer && Location == p.Location;
    }
};
struct hash_fn
{
    std::size_t operator() (const PointsTo &relation) const
    {
        std::size_t h1 = std::hash<Value*>()(relation.Pointer);
        std::size_t h2 = std::hash<Value*>()(relation.Location);
        
        return h1 ^ h2;
    }
};
namespace {
    struct PointsToAnalysis : public FunctionPass {
        static char ID; // Pass identification, replacement for typeid
        PointsToAnalysis() : FunctionPass(ID) {}
        void getAnalysisUsage(AnalysisUsage &AU) const override{
            //AU.addRequired<EQTDDataStructures>();
            //AU.addRequired<SCEVAAWrapperPass>();
            AU.addRequired<CFGAnalysisForDFA>();
            AU.addRequired<PointsToSets>();
            AU.addRequired<BasicAAWrapperPass>();
            AU.addRequired<CFLSteensAAWrapperPass>();
            AU.addRequired<CFLAndersAAWrapperPass>();
            AU.setPreservesAll();
        }
        bool runOnFunction(Function &F) override {
            if (F.empty())
                return false;
            outs() << "************************\n";
            outs() << "Alias analyses for function: ";
            outs().write_escaped(F.getName()) << "\n";
            
            //Calls our pass that extracts instruction-level CFG and instruction list sorted in topological order
            CFGAnalysisForDFA &CFGAnalysesForDFA=getAnalysis<CFGAnalysisForDFA>();
            //Instructions sorted in topological order
            std::vector<Instruction*> TopoSortedInsts=CFGAnalysesForDFA.getTopoSortedInstructions();
            //Predecessor nodes of instructions
            std::unordered_map<Instruction*, std::vector<Instruction*>> PredsOfInstructions=CFGAnalysesForDFA.getPredsOfInstructions();
            //Successor nodes of instructions
            std::unordered_map<Instruction*, std::vector<Instruction*>> SuccsOfInstructions=CFGAnalysesForDFA.getSuccsOfInstructions();
            //Exit instructions
            std::vector<Instruction*> ExitInstructions=CFGAnalysesForDFA.getExitInstructions();
            
            //TODO:Replace with a flow-sensitive alias analysis
            PointsToSets &AA = getAnalysis<PointsToSets>();
            
            //SCEVAAResult &AA = getAnalysis<SCEVAAWrapperPass>().getResult();
            std::unordered_map<Value*,std::unordered_set<Value*>> MayAliasSet=AA.getMayAliasSet();
            std::unordered_map<Value*,std::unordered_set<Value*>> MustAliasSet=AA.getMustAliasSet();
            
            //Domain items: Global variables, global aliases, function arguments, top-Level SSA definitions (i.e. %-type instructions)
            std::vector<PointsTo> Domain;
            std::unordered_map<PointsTo,int, hash_fn> DomainIndices;
            //Address definitions map for kill sets, Key:Pointer operand, Value:List of store instructions for the given target address
            //TODO:Aliases
            std::map<Value*,std::vector<Instruction*>> AddressDefinitions;
            
            //Function arguments
            std::vector<Value*> InputArgs;
            //Arguments can be called by reference
            std::vector<Value*> OutputArgs;
            //Possible return values
            std::vector<Value*> ReturnValues;
            //Possible terminating instructions of the function
            std::vector<Instruction*> ExitInsts;
            //Arg indices
            std::map<Value*,int> ArgIndexes;
            
            
            
            std::unordered_set<Value*> Locations;
            std::unordered_set<Value*> Variables;
            std::vector<MemoryLocation> MemLocations;
            std::unordered_set<Value*> Pointers;
            std::vector<MemoryLocation> MemPointers;
            for (Function::arg_iterator I = F.arg_begin(); I != F.arg_end(); ++I){
                Value* Arg = I;
                Variables.insert(Arg);
                Type* ArgType=Arg->getType();
                //if (Arg->getType()->getTypeID()==llvm::Type::PointerTyID){
                if (isa<PointerType>(ArgType)){
                    Pointers.insert(Arg);
                    //outs() << *Arg << "\n";
                }
                
            }//Adds global arguments to the domain
            for (GlobalVariable &GV:F.getParent()->getGlobalList()){
                if (!GV.isConstant()){
                    Variables.insert(&GV);
                    Locations.insert(&GV);
                    Type* GlobalType=GV.getType();
                    //if (Arg->getType()->getTypeID()==llvm::Type::PointerTyID){
                    if (isa<PointerType>(GlobalType)){
                        Pointers.insert(&GV);
                        //outs() << *Arg << "\n";
                    }
                    //outs() << GV << "\n";
                }
            }//Adds global aliases to the domain
            for (GlobalAlias &GA:F.getParent()->getAliasList()){
                Variables.insert(&GA);
                Locations.insert(&GA);
                Pointers.insert(&GA);
                //outs() << GA << "\n";
            }
            //Adds instructions defining top-level or address-level values and ignores others (e.g. br, ret);
            for (Instruction* Inst:TopoSortedInsts){
                if (Utils::isTopLevelDefinition(Inst)){
                    Variables.insert(Inst);
                    if (AllocaInst* Alloca=dyn_cast<AllocaInst>(Inst)){
                        Locations.insert(Inst);
                        Type* AllocaType=Alloca->getAllocatedType();
                        if (isa<PointerType>(AllocaType))
                            Pointers.insert(Inst);
                        //TODO:malloc group
                    }
                    else if (CallInst* Call=dyn_cast<CallInst>(Inst)){
                        std::string calledFunctionName=Call->getOperand(Call->getNumOperands()-1)->getName().str();
                        //If the call is a taint source, mark the value as tainted
                        if (calledFunctionName=="malloc" || calledFunctionName=="realloc" || calledFunctionName=="calloc"){
                            Locations.insert(Inst);
                            Pointers.insert(Inst);
                        }
                    }
                }
            }
            
            //Create Lattice domain
            int DomainIndex=0;
            
            for (Value* Pointer:Variables){
                for (Value* Variable:Variables){
                    if (Pointer!=Variable){
                        PointsTo Relation={Pointer,Variable};
                        DomainIndices.insert(std::make_pair(Relation, DomainIndex++));
                        Domain.push_back(Relation);
                        outs() << DomainIndex << ":" <<  Utils::identifierStr(Relation.Pointer) << "->" <<  Utils::identifierStr(Relation.Location) << "\n";
                    }
                }
            }
            int DomainSize=Domain.size();
            std::unordered_map<Value*, BitVector> InSets;
            std::unordered_map<Value*, BitVector> OutSets;
            std::unordered_map<Value*, BitVector> InPrimeSets;
            std::unordered_map<Value*, BitVector> OutPrimeSets;
            std::unordered_map<Value*, BitVector> GenSets;
            std::unordered_map<Value*, BitVector> KillSets;
            std::unordered_map<Instruction*, bool> VisitedInstsAtLeastOnce;
            
            bool FirstInstructionForArgs=true;
            //Defines static GEN and KILL sets
            BitVector Touched(DomainSize,false);
            for (Instruction* Inst:TopoSortedInsts){
                VisitedInstsAtLeastOnce.insert(std::make_pair(Inst,false));
                outs() << *Inst << "\n";
                BitVector Gen(DomainSize,false);
                BitVector Kill(DomainSize,false);
                BitVector In(DomainSize,false);
                BitVector Out(DomainSize,false);
                
                if (StoreInst* Store=dyn_cast<StoreInst>(Inst)){
                    Value* Source=Store->getValueOperand();
                    Value* Target=Store->getPointerOperand();
                    if (!isa<Constant>(Source)){
                        PointsTo Relation={Target,Source};
                        Gen.set(DomainIndices.at(Relation));
                    }
                    //for kill for uses of operands
                    for(User* User:Target->users()){
                        if (StoreInst* OtherStore=dyn_cast<StoreInst>(User)){
                            Value* OtherSource=Store->getValueOperand();
                            if (!isa<Constant>(OtherSource)){
                                PointsTo Relation={Target,OtherSource};
                                Kill.set(DomainIndices.at(Relation));
                            }
                        }
                    }
                }
                else if (LoadInst* Load=dyn_cast<LoadInst>(Inst)){
                    Value* Source=Load->getPointerOperand();
                    Value* Target=Load;
                    PointsTo Relation={Source,Target};
                    Gen.set(DomainIndices.at(Relation));
                }
                else if (PtrToIntInst* PtrToInt=dyn_cast<PtrToIntInst>(Inst)){
                    Value* Source=PtrToInt->getPointerOperand();
                    Value* Target=PtrToInt;
                    if (!isa<Constant>(Source)){
                        PointsTo Relation={Source,Target};
                        Gen.set(DomainIndices.at(Relation));
                    }
                }
                else if (IntToPtrInst* IntToPtr=dyn_cast<IntToPtrInst>(Inst)){
                    Value* Source=IntToPtr->getOperand(0);
                    Value* Target=IntToPtr;
                    if (!isa<Constant>(Source)){
                        PointsTo Relation={Source,Target};
                        Gen.set(DomainIndices.at(Relation));
                    }
                }
                else if (GetElementPtrInst* GetElementPtr=dyn_cast<GetElementPtrInst>(Inst)){
                    Value* Target=GetElementPtr;
                    for (unsigned int i=0;i<GetElementPtr->getNumOperands();i++){
                        Value* Source=IntToPtr->getOperand(i);
                        if (!isa<Constant>(Source)){
                            PointsTo Relation={Source,Target};
                            Gen.set(DomainIndices.at(Relation));
                        }
                    }
                }
                else if (BinaryOperator* Binary=dyn_cast<BinaryOperator>(Inst)){
                    Value* Target=Binary;
                    for (unsigned int i=0;i<Binary->getNumOperands();i++){
                        Value* Source=Binary->getOperand(i);
                        if (!isa<Constant>(Source)){
                            PointsTo Relation={Source,Target};
                            Gen.set(DomainIndices.at(Relation));
                        }
                    }
                }
                else if (CastInst* Cast = dyn_cast<CastInst>(Inst)) {
                    Value* Source=Cast->getOperand(0);
                    Value* Target=Cast;
                    if (!isa<Constant>(Source)){
                        PointsTo Relation={Source,Target};
                        Gen.set(DomainIndices.at(Relation));
                    }
                }
                
                FirstInstructionForArgs=false;
                //Map each GEN and KILL set to the Instructions
                if (GenSets.find(Inst)==GenSets.end()){
                    GenSets.insert(std::make_pair(Inst,Gen));
                }
                if (KillSets.find(Inst)==KillSets.end()){
                    KillSets.insert(std::make_pair(Inst,Kill));
                }
                if (InSets.find(Inst)==InSets.end()){
                    InSets.insert(std::make_pair(Inst,In));
                }
                if (OutSets.find(Inst)==OutSets.end()){
                    OutSets.insert(std::make_pair(Inst,Out));
                }
            }//end of gen & kill sets
            
            //Iterative Reaching Definitions via Work List Algorithm
            outs() << "Pops of worklist algorithm:\n";
            outs() << "PopCount\tIn\tOut\tOutPrime\tGen\tKill\tInst\n";
            int PopCount=0;
            std::vector<Instruction*> WorkList;
            std::unordered_map<Instruction*, bool> VisitedInsts;
            WorkList.push_back(TopoSortedInsts.at(0));
            while (!WorkList.empty()){
                PopCount++;
                Instruction* Inst=WorkList.back();
                WorkList.pop_back();
                
                BitVector In=InSets.at(Inst);
                BitVector Out=OutSets.at(Inst);
                BitVector Gen=GenSets.at(Inst);
                BitVector Kill=KillSets.at(Inst);
                BitVector OutPrime=OutSets.at(Inst);
                
                const BitVector Empty(DomainSize,false);
                
                //in[n] = U out[p]
                std::vector<Instruction*> Preds;
                Preds=PredsOfInstructions.at(Inst);
                for (Instruction* Pred:Preds){
                    if (OutSets.find(Pred)!=OutSets.end())
                        In|=OutSets.at(Pred); //MAY Analysis
                    //In&=OutSets.at(Pred); //MUST Analysis
                }
                
                //updateGenKillSets(Inst, DomainIndices, In, Gen, Kill,false, VisitedInstsAtLeastOnce);
                
                //out[n] = gen[n] U (in[n] - kill[n]
                BitVector Temp(DomainSize,false);
                Temp=Kill.flip();
                Kill.flip();
                Temp&=In;
                Temp|=Gen;
                Out=Temp;
                
                InSets.at(Inst)=In;
                OutSets.at(Inst)=Out;
                
                //if out[n] != out'
                //  for each s E succ[n]
                //      add s to worklist
                if (!(Out.operator==(OutPrime)) ||!VisitedInstsAtLeastOnce.at(Inst)){
                    std::vector<Instruction*> Successors=SuccsOfInstructions.at(Inst);
                    for (Instruction* Succ:Successors){
                        WorkList.push_back(Succ);
                    }
                }
                //outs() << PopCount << "\t" << Utils::BitVectorStr(In) << "\t" << Utils::BitVectorStr(Out) << "\t" << Utils::BitVectorStr(OutPrime) << "\t" << Utils::BitVectorStr(Gen) << "\t" << Utils::BitVectorStr(Kill) << "\t" << *Inst << "\n";
                outs() << PopCount << *Inst <<"\n";
                
                VisitedInstsAtLeastOnce.at(Inst)=true;
                
            }
            return false;
        }
    };
}

char PointsToAnalysis::ID = 0;
static RegisterPass<PointsToAnalysis> BVI("fpointstoset", "Flow Sensitive Points-to Analysis Pass");
