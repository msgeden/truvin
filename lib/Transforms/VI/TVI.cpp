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

#include "llvm/ADT/SCCIterator.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IntrinsicInst.h"
#include <llvm/IR/DebugLoc.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Transforms/VI/TVI.h"
#include "llvm/Transforms/VI/CFA.h"
#include "llvm/Transforms/VI/Alias.h"
#include "llvm/Transforms/VI/Utils.h"

#include <unordered_set>
#include <unordered_map>
#include <cstdlib>
#include <iostream>
using namespace llvm;
//File pointers
std::ofstream fcbenchmarks;
std::ofstream fcfunctions;
std::ofstream fcrules;
std::ofstream fcinsts;
//Set of functions returning clean values
std::unordered_set<std::string> CleanFunctions;
//Set of functions returning clean values according to the given rule
std::unordered_map<std::string,std::vector<PropagationRule>> CleanPropagatingFunctions;
//Set of control instructions returning clean values according to the given rule
std::unordered_set<Instruction*> CleanControlInstructions;
std::unordered_set<Instruction*> AllCControlInstructions;
std::unordered_set<Instruction*> CleanLoopControlInstructions;
std::unordered_set<Instruction*> AllCLoopControlInstructions;

std::unordered_set<Instruction*> CleanLoadInstructions;
std::unordered_set<Instruction*> AllCLoadInstructions;
std::unordered_set<Instruction*> CleanStoreInstructions;
std::unordered_set<Instruction*> AllCStoreInstructions;


//Must be reaching clean values (IN) set for each instruction
std::unordered_map<Instruction*, BitVector> GlobalReachingInCleanData;
std::unordered_map<Instruction*, BitVector> GlobalReachingOutCleanData;
char FunctionProgrammerVariables::ID = 0;
void FunctionProgrammerVariables::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
}
bool FunctionProgrammerVariables::runOnFunction(Function &F) {
    if (F.empty())
        return false;
    outs() << "************************\n";
    outs() << "Variables defined by the programmer for function:";
    outs().write_escaped(F.getName()) << "\n";
    
    std::unordered_set<Value*> Variables;
    std::unordered_set<Value*> ProgrammerVariables;
    std::unordered_set<Value*> NonProgrammerVariables;
    std::unordered_set<Value*> ConstantVariables;
    std::unordered_set<Value*> NonConstantVariables;
    //    std::unordered_set<const Value*> MemoryLocations;
    //    for (Function::arg_iterator I = F.arg_begin(); I != F.arg_end(); ++I){
    //        Value* Arg = I;
    //        outs() << "ARG:" << *Arg << "\n";
    //    }//Adds global arguments to the domain
    //    for (GlobalVariable &GV:F.getParent()->getGlobalList()){
    //        if (!GV.isConstant()){
    //            outs() << "GV:" << GV << "\n";
    //        }
    //    }//Adds global aliases to the domain
    //    for (GlobalAlias &GA:F.getParent()->getAliasList()){
    //        outs() << "GA:" << GA << "\n";
    //    }
    for (auto I = inst_begin(F); I!=inst_end(F);++I){
        Instruction* Inst=&*I;
        Value* Pointer;
        Value* Value;
        //        Optional<MemoryLocation> MemLoc=MemoryLocation::getOrNone(Inst);
        //        if (MemLoc!=None){
        //            const llvm::Value* MemValue=MemLoc.getValue().Ptr;
        //            //outs() << "Mem:" << *MemValue << "\n";
        //            MemoryLocations.insert(MemValue);
        //        }
        if (StoreInst* Store=dyn_cast<StoreInst>(Inst)){
            Value=Store->getValueOperand();
            Pointer=Store->getPointerOperand();
            Variables.insert(Pointer);
            if (!(isa<GlobalValue>(Pointer) || isa<GlobalAlias>(Pointer))){
                if (isa<Constant>(Value))
                {
                    //outs() << "Store:" << *Inst << "\n";
                    ConstantVariables.insert(Pointer);
                }
                else
                    NonConstantVariables.insert(Pointer);
            }
        }
        //outs() << *Inst << "\n";
    }
    //outs() << "Programmer-Defined Variables:\n";
    for (Value* ConstantVariable:ConstantVariables){
        if (NonConstantVariables.find(ConstantVariable)==NonConstantVariables.end()){
            if (ProgrammerVariables.find(ConstantVariable)==ProgrammerVariables.end()){
                ProgrammerVariables.insert(ConstantVariable);
                //outs() << *ConstantVariable << "\n";
            }
        }
    }
    //outs() << "Other Variables:\n";
    for (Value* Variable:Variables){
        if (ProgrammerVariables.find(Variable)==ProgrammerVariables.end()){
            if (NonProgrammerVariables.find(Variable)==NonProgrammerVariables.end()){
                NonProgrammerVariables.insert(Variable);
                //outs() << *Variable << "\n";
            }
        }
    }
    float Ratio=((float)ProgrammerVariables.size()/(float)Variables.size())*100;
    outs() << "# of programmer-defined variables:"<< ProgrammerVariables.size() << "\n";
    outs() << "# of other variables:"<< NonProgrammerVariables.size() << "\n";
    outs() << "# of all variables:"<< Variables.size() << "\n";
    outs() << "ratio of programmer variables:"<< Utils::round((double)Ratio) << "\n";
    return false;
}
static RegisterPass<FunctionProgrammerVariables> A("fprogvars", "Function-wise flow-insensitive of extraction of programmer-defined variables");

char ModuleProgrammerVariables::ID = 0;
void ModuleProgrammerVariables::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
}
bool ModuleProgrammerVariables::runOnModule(Module &M) {
    outs() << "************************\n";
    outs() << "Variables defined by the programmer for module:";
    outs().write_escaped(M.getName()) << "\n";
    
    std::unordered_set<Value*> Variables;
    std::unordered_set<Value*> ConstantVariables;
    std::unordered_set<Value*> NonConstantVariables;
    std::unordered_set<const Value*> MemoryLocations;
    for (Function &F:M){
        if (F.empty())
            continue;
        for (auto I = inst_begin(F); I!=inst_end(F);++I){
            Instruction* Inst=&*I;
            Value* Pointer;
            Value* Value;
            //            Optional<MemoryLocation> MemLoc=MemoryLocation::getOrNone(Inst);
            //            if (MemLoc!=None){
            //                const llvm::Value* MemValue=MemLoc.getValue().Ptr;
            //                //outs() << "Mem:" << *MemValue << "\n";
            //                MemoryLocations.insert(MemValue);
            //            }
            if (StoreInst* Store=dyn_cast<StoreInst>(Inst)){
                Value=Store->getValueOperand();
                Pointer=Store->getPointerOperand();
                Variables.insert(Pointer);
                if (isa<Constant>(Value))
                {
                    //outs() << "Store:" << *Inst << "\n";
                    ConstantVariables.insert(Pointer);
                }
                else
                    NonConstantVariables.insert(Pointer);
            }
            //outs() << *Inst << "\n";
        }
    }
    //outs() << "Programmer-Defined Variables:\n";
    for (Value* ConstantVariable:ConstantVariables){
        if (NonConstantVariables.find(ConstantVariable)==NonConstantVariables.end()){
            if (ProgrammerVariables.find(ConstantVariable)==ProgrammerVariables.end()){
                ProgrammerVariables.insert(ConstantVariable);
                //outs() << *ConstantVariable << "\n";
            }
        }
    }
    //outs() << "Other Variables:\n";
    for (Value* Variable:Variables){
        if (ProgrammerVariables.find(Variable)==ProgrammerVariables.end()){
            if (NonProgrammerVariables.find(Variable)==NonProgrammerVariables.end()){
                NonProgrammerVariables.insert(Variable);
                //outs() << *Variable << "\n";
            }
        }
    }
    float Ratio=((float)ProgrammerVariables.size()/(float)Variables.size())*100;
    outs() << "# of programmer-defined variables:"<< ProgrammerVariables.size() << "\n";
    outs() << "# of other variables:"<< NonProgrammerVariables.size() << "\n";
    outs() << "# of all variables:"<< Variables.size() << "\n";
    outs() << "ratio of programmer variables:"<< Utils::round((double)Ratio) << "\n";
    return false;
}
static RegisterPass<ModuleProgrammerVariables> E("mprogvars", "Module-wise flow-insensitive of extraction of programmer-defined variables");


/////////////
/////////////
/////////////   Clean Data Propagation Modeller
/////////////
void updateCleanSetsViaMust(Instruction* Inst, std::unordered_map<Value*,int> DomainIndices, BitVector &In, BitVector &Gen, BitVector &Kill, BitVector &Touched){
    //    //LLVM inherits Instruction from Value class. For most instructions, target value and the instruction is the same
    Value* Source;
    Value* Source2;
    Value* Target;
    
    //Call instructions
    if (CallInst* Call = dyn_cast<CallInst>(Inst)) {
        Target=Call;
        std::string calledFunctionName=Call->getOperand(Call->getNumOperands()-1)->getName().str();
        if (DomainIndices.find(Target)!=DomainIndices.end()){
            if (CleanFunctions.find(calledFunctionName)!= CleanFunctions.end()){
                Gen.set(DomainIndices.at(Target));
                Touched.set(DomainIndices.at(Target));
            }
            else if (CleanPropagatingFunctions.find(calledFunctionName) != CleanPropagatingFunctions.end()){
                bool AreAllArgsAreClean=(Call->getNumArgOperands()>0);
                bool ArgsTouched=false;
                for (unsigned int i=0;i<Call->getNumArgOperands();i++){
                    Value* Arg=Call->getArgOperand(i);
                    if (DomainIndices.find(Arg)!=DomainIndices.end()){
                        if (In[DomainIndices.at(Arg)] && Touched[DomainIndices.at(Arg)]){
                            AreAllArgsAreClean&=true;
                            ArgsTouched=true;
                        }
                        else if (!In[DomainIndices.at(Arg)] && Touched[DomainIndices.at(Arg)]){
                            AreAllArgsAreClean&=false;
                            ArgsTouched=true;
                        }
                    }
                }
                if (AreAllArgsAreClean && ArgsTouched){
                    Gen.set(DomainIndices.at(Target));
                    Touched.set(DomainIndices.at(Target));
                }
                else if (!AreAllArgsAreClean && ArgsTouched){
                    Kill.set(DomainIndices.at(Target));
                    Touched.set(DomainIndices.at(Target));
                }
            }
            else{
                Kill.set(DomainIndices.at(Target));
                Touched.set(DomainIndices.at(Target));
            }
        }
    }//end of Call
    //Store instructions
    if (StoreInst* Store=dyn_cast<StoreInst>(Inst)){
        Target=Store->getPointerOperand();
        Source=Store->getValueOperand();
        if (DomainIndices.find(Target)!=DomainIndices.end()){
            if (isa<Constant>(Source)){
                Gen.set(DomainIndices.at(Target));
                Touched.set(DomainIndices.at(Target));
            }
            else{
                if (DomainIndices.find(Source)!=DomainIndices.end()){
                    if (In[DomainIndices.at(Source)] && Touched[DomainIndices.at(Source)]){
                        Gen.set(DomainIndices.at(Target));
                        Touched.set(DomainIndices.at(Target));
                    }
                    else if (!In[DomainIndices.at(Source)] && Touched[DomainIndices.at(Source)]){
                        Kill.set(DomainIndices.at(Target));
                        Touched.set(DomainIndices.at(Target));
                    }
                }
            }
        }
    }//end of Store
    //Load instructions
    else if (LoadInst* Load=dyn_cast<LoadInst>(Inst)){
        Source=Load->getPointerOperand();
        Target=Load;
        //TODO:Think about Function args
        if (DomainIndices.find(Target)!=DomainIndices.end()){
            if (DomainIndices.find(Source)!=DomainIndices.end()){
                if (In[DomainIndices.at(Source)] && Touched[DomainIndices.at(Source)]){
                    Gen.set(DomainIndices.at(Target));
                    Touched.set(DomainIndices.at(Target));
                }
                else if (!In[DomainIndices.at(Source)] && Touched[DomainIndices.at(Source)]){
                    Kill.set(DomainIndices.at(Target));
                    Touched.set(DomainIndices.at(Target));
                }
            }
        }
    }//end of Load
    //Cmp instruction
    else if (CmpInst* Cmp = dyn_cast<CmpInst>(Inst)) {
        Source=Cmp->getOperand(0);
        Source2=Cmp->getOperand(1);
        Target=Cmp;
        bool AtLeastOneCleanOperand=false;
        if (DomainIndices.find(Target)!=DomainIndices.end()){
            
            if (isa<Constant>(Source) || isa<Constant>(Source2)){
                AtLeastOneCleanOperand=true;
            }
            if (DomainIndices.find(Source)!=DomainIndices.end()){
                if (In[DomainIndices.at(Source)] && Touched[DomainIndices.at(Source)]){
                    AtLeastOneCleanOperand=true;
                }
            }
            if (DomainIndices.find(Source2)!=DomainIndices.end()){
                if (In[DomainIndices.at(Source2)] && Touched[DomainIndices.at(Source2)]){
                    AtLeastOneCleanOperand=true;
                }
            }
            if (AtLeastOneCleanOperand){
                Gen.set(DomainIndices.at(Target));
                Touched.set(DomainIndices.at(Target));
            }
            else{
                Kill.set(DomainIndices.at(Target));
                Touched.set(DomainIndices.at(Target));
            }
        }
    }//End of Cmp
    
    //    //Cmp instruction
    //    else if (CmpInst* Cmp = dyn_cast<CmpInst>(Inst)) {
    //        Source=Cmp->getOperand(0);
    //        Source2=Cmp->getOperand(1);
    //        Target=Cmp;
    //        if (DomainIndices.find(Target)!=DomainIndices.end()){
    //            if (isa<Constant>(Source) && isa<Constant>(Source2)){
    //                Gen.set(DomainIndices.at(Target));
    //                Touched.set(DomainIndices.at(Target));
    //            }
    //            else if (isa<Constant>(Source) && !isa<Constant>(Source2)){
    //                if (DomainIndices.find(Source2)!=DomainIndices.end()){
    //                    if (In[DomainIndices.at(Source2)] && Touched[DomainIndices.at(Source2)]){
    //                        Gen.set(DomainIndices.at(Target));
    //                        Touched.set(DomainIndices.at(Target));
    //                    }
    //                    else if (!In[DomainIndices.at(Source2)] && Touched[DomainIndices.at(Source2)]){
    //                        Kill.set(DomainIndices.at(Target));
    //                        Touched.set(DomainIndices.at(Target));
    //                    }
    //                }
    //            }
    //            else if (!isa<Constant>(Source) && isa<Constant>(Source2)){
    //                if (DomainIndices.find(Source)!=DomainIndices.end()){
    //                    if (In[DomainIndices.at(Source)] && Touched[DomainIndices.at(Source)]){
    //                        Gen.set(DomainIndices.at(Target));
    //                        Touched.set(DomainIndices.at(Target));
    //                    }
    //                    else if (!In[DomainIndices.at(Source)] && Touched[DomainIndices.at(Source)]){
    //                        Kill.set(DomainIndices.at(Target));
    //                        Touched.set(DomainIndices.at(Target));
    //                    }
    //                }
    //            }
    //            else {
    //                if (DomainIndices.find(Source)!=DomainIndices.end() && DomainIndices.find(Source2)!=DomainIndices.end()){
    //                    if (In[DomainIndices.at(Source)] && In[DomainIndices.at(Source2)] && Touched[DomainIndices.at(Source)] && Touched[DomainIndices.at(Source2)]){
    //                        Gen.set(DomainIndices.at(Target));
    //                        Touched.set(DomainIndices.at(Target));
    //                    }
    //                    else if ((!In[DomainIndices.at(Source)] && In[DomainIndices.at(Source2)]) || (Touched[DomainIndices.at(Source)] && Touched[DomainIndices.at(Source2)])){
    //                        Kill.set(DomainIndices.at(Target));
    //                        Touched.set(DomainIndices.at(Target));
    //                    }
    //                }
    //            }
    //        }
    //    }//End of Cmp
    //Binary instructions
    else if (BinaryOperator* Binary = dyn_cast<BinaryOperator>(Inst)) {
        Source=Binary->getOperand(0);
        Source2=Binary->getOperand(1);
        Target=Binary;
        if (DomainIndices.find(Target)!=DomainIndices.end()){
            if (isa<Constant>(Source) && isa<Constant>(Source2)){
                Gen.set(DomainIndices.at(Target));
                Touched.set(DomainIndices.at(Target));
            }
            else if (isa<Constant>(Source) && !isa<Constant>(Source2)){
                if (DomainIndices.find(Source2)!=DomainIndices.end()){
                    if (In[DomainIndices.at(Source2)] && Touched[DomainIndices.at(Source2)]){
                        Gen.set(DomainIndices.at(Target));
                        Touched.set(DomainIndices.at(Target));
                    }
                    else if (!In[DomainIndices.at(Source2)] && Touched[DomainIndices.at(Source2)]){
                        Kill.set(DomainIndices.at(Target));
                        Touched.set(DomainIndices.at(Target));
                    }
                }
            }
            else if (!isa<Constant>(Source) && isa<Constant>(Source2)){
                if (DomainIndices.find(Source)!=DomainIndices.end()){
                    if (In[DomainIndices.at(Source)] && Touched[DomainIndices.at(Source)]){
                        Gen.set(DomainIndices.at(Target));
                        Touched.set(DomainIndices.at(Target));
                    }
                    else if (!In[DomainIndices.at(Source)] && Touched[DomainIndices.at(Source)]){
                        Kill.set(DomainIndices.at(Target));
                        Touched.set(DomainIndices.at(Target));
                    }
                }
            }
            else {
                if (DomainIndices.find(Source)!=DomainIndices.end() && DomainIndices.find(Source2)!=DomainIndices.end()){
                    if (In[DomainIndices.at(Source)] && In[DomainIndices.at(Source2)] && Touched[DomainIndices.at(Source)] && Touched[DomainIndices.at(Source2)]){
                        Gen.set(DomainIndices.at(Target));
                        Touched.set(DomainIndices.at(Target));
                    }
                    else if ((!In[DomainIndices.at(Source)] && In[DomainIndices.at(Source2)]) || (Touched[DomainIndices.at(Source)] && Touched[DomainIndices.at(Source2)])){
                        Kill.set(DomainIndices.at(Target));
                        Touched.set(DomainIndices.at(Target));
                    }
                }
            }
        }
    }//end of Binary
    //    //Casting instructions
    //    else if (CastInst* Cast = dyn_cast<CastInst>(Inst)) {
    //        Source=Cast->getOperand(0);
    //        Target=Cast;
    //        if (isa<Constant>(Source)){
    //            Gen.set(DomainIndices.at(Target));
    //        }
    //        else{
    //            if (DomainIndices.find(Source)!=DomainIndices.end()){
    //                if (In[DomainIndices.at(Source)])
    //                    Gen.set(DomainIndices.at(Target));
    //            }
    //        }
    //    }//end of Casting
    //Unary instructions
    else if (UnaryInstruction* Unary = dyn_cast<CastInst>(Inst)) {
        Source=Unary->getOperand(0);
        Target=Unary;
        if (DomainIndices.find(Target)!=DomainIndices.end()){
            if (isa<Constant>(Source)){
                Gen.set(DomainIndices.at(Target));
                Touched.set(DomainIndices.at(Target));
            }
            else{
                if (DomainIndices.find(Source)!=DomainIndices.end()){
                    if (In[DomainIndices.at(Source)] && Touched[DomainIndices.at(Source)]){
                        Gen.set(DomainIndices.at(Target));
                        Touched.set(DomainIndices.at(Target));
                    }
                    else if (!In[DomainIndices.at(Source)] && Touched[DomainIndices.at(Source)]){
                        Kill.set(DomainIndices.at(Target));
                        Touched.set(DomainIndices.at(Target));
                    }
                }
            }
        }
    }//end of Casting
    //GetElementPtrInst instructions
    else if (GetElementPtrInst* GetElementPtr = dyn_cast<GetElementPtrInst>(Inst)) {
        Target=GetElementPtr;
        Source=GetElementPtr->getPointerOperand();
        if (DomainIndices.find(Target)!=DomainIndices.end()){
            if (DomainIndices.find(Source)!=DomainIndices.end()){
                if (In[DomainIndices.at(Source)] && Touched[DomainIndices.at(Source)]){
                    Gen.set(DomainIndices.at(Target));
                    Touched.set(DomainIndices.at(Target));
                }
                else if (!In[DomainIndices.at(Source)] && Touched[DomainIndices.at(Source)]){
                    Kill.set(DomainIndices.at(Target));
                    Touched.set(DomainIndices.at(Target));
                }
            }
        }
    }//end of GetElementPtrInst
    //Phi instructions
    else if (PHINode* PHI = dyn_cast<PHINode>(Inst)) {
        Target=PHI;
        bool Clean=true;
        bool TouchInst=false;
        if (DomainIndices.find(Target)!=DomainIndices.end()){
            for (unsigned int i=0;i<PHI->getNumOperands();i++){
                Source=PHI->getOperand(i);
                if (!isa<Constant>(Source)){
                    if (DomainIndices.find(Source)!=DomainIndices.end()){
                        if (In[DomainIndices.at(Source)] && Touched[DomainIndices.at(Source)]){
                            Clean&=true;
                            TouchInst=true;
                        }
                        else if (!In[DomainIndices.at(Source)] && Touched[DomainIndices.at(Source)]){
                            Clean&=false;
                            TouchInst=true;
                        }
                    }
                    
                }
            }
            if (Clean && TouchInst){
                Gen.set(DomainIndices.at(Target));
                Touched.set(DomainIndices.at(Target));
            }
            else if (!Clean && TouchInst){
                Kill.set(DomainIndices.at(Target));
                Touched.set(DomainIndices.at(Target));
            }
        }
    }//end of PHI Instruction
}
int CTotalCount=0;

char CleanDataPropagationModeller::ID = 0;
void CleanDataPropagationModeller::getAnalysisUsage(AnalysisUsage &AU) const {
    
    AU.addRequired<CFGAnalysisForDFA>();
    //AU.addRequired<VariableNamesAnalysis>();
    AU.addRequired<LoopInfoWrapperPass>();
    
    AU.setPreservesAll();
}

bool CleanDataPropagationModeller::runOnFunction(Function &F)  {
    if (F.empty())
        return false;
    
    std::clock_t begin;
    begin=std::clock();
    std::clock_t now;
    
    outs() << "************************\n";
    outs () << "Clean data propagation analyses for function:" << F.getName().str() << "\n";
    DomainIndices.clear();
    BitVectorItems.clear();
    
    
    LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
    
    
   // LocalVariables=getAnalysis<VariableNamesAnalysis>().getLocalVariables();
    
    //Calls our pass that extracts instruction-level CFG and instruction list sorted in topological order
    CFGAnalysisForDFA &CFGAnalysesForDFA=getAnalysis<CFGAnalysisForDFA>();
    //Preds of Basic Blocks
    PredsofBBs=CFGAnalysesForDFA.getPredsOfBBs();
    //Instructions sorted in topological order
    TopoSortedInsts=CFGAnalysesForDFA.getTopoSortedInstructions();
    //std::vector<Instruction*> TopoSortedInsts=CFGAnalysesForDFA.getTopoSortedInstructions();
    //Predecessor nodes of instructions
    std::unordered_map<Instruction*, std::vector<Instruction*>> PredsOfInstructions=CFGAnalysesForDFA.getPredsOfInstructions();
    //Successor nodes of instructions
    std::unordered_map<Instruction*, std::vector<Instruction*>> SuccsOfInstructions=CFGAnalysesForDFA.getSuccsOfInstructions();
    //Exit instructions
    std::vector<Instruction*> ExitInstructions=CFGAnalysesForDFA.getExitInstructions();
    //TODO:Replace with a flow-sensitive alias analysis
    PointsToSets &AA = getAnalysis<PointsToSets>();
    std::unordered_map<Value*,std::unordered_set<Value*>> MayAliasSet;
    std::unordered_map<Value*,std::unordered_set<Value*>> MustAliasSet;
    
    MayAliasSet=AA.getMayAliasSet();
    MustAliasSet=AA.getMustAliasSet();
    
    //Look for aliases of output arguments
    std::unordered_map<Value*,std::unordered_set<Value*>> MayArgAliasSet;
    std::unordered_map<Value*,std::unordered_set<Value*>> MustArgAliasSet;

    //Domain items: Global variables, global aliases, function arguments, top-Level SSA definitions (i.e. %-type instructions)
    std::vector<Value*> Domain;
    //Address definitions map for kill sets, Key:Pointer operand, Value:List of store instructions for the given target address
    //TODO:Aliases
    std::unordered_map<Value*,std::vector<Instruction*>> AddressDefinitions;
    
    //Function arguments
    std::vector<Value*> InputArgs;
    //Arg indices
    std::unordered_map<Value*,int> ArgIndexes;
    //Function arguments
    std::unordered_set<Value*> ArgSet;
    //Arguments can be called by reference
    std::vector<Value*> OutputArgs;
    //Possible return values
    std::vector<Value*> ReturnValues;
    //Possible terminating instructions of the function
    std::vector<Instruction*> ExitInsts;
    
    
    //TODO:Variadic functions
    //Identify functions arguments (call by reference) which can be function output
    int ArgIndex=0;
    if (!F.isVarArg()){
        for (Function::arg_iterator I = F.arg_begin(),E=F.arg_end(); I != E; ++I){
            Argument* Arg = I;
            ArgIndexes.insert(std::make_pair(Arg,ArgIndex++));
            //outs() << "Input Arg:" << *Arg << "\n";
            InputArgs.push_back(Arg);
            ArgSet.insert(Arg);
            //If the argument is called by reference
            Type* ArgType=Arg->getType();
            if (isa<PointerType>(ArgType)){
                OutputArgs.push_back(Arg);
                //                if (MayAliasSet.find(Arg)!=MayAliasSet.end())
                //                    MayArgAliasSet.insert(std::make_pair(Arg, MayAliasSet.at(Arg)));
                //                if (MustAliasSet.find(Arg)!=MustAliasSet.end())
                //                    MustArgAliasSet.insert(std::make_pair(Arg, MustAliasSet.at(Arg)));
                //outs() << "Output Arg:" << *Arg << "\n";
            }
        }
    }//Identify return values of the function
    if (!F.doesNotReturn() &&F.getReturnType()->getTypeID() != llvm::Type::VoidTyID){
        inst_iterator I;
        for (Instruction* Inst:ExitInstructions){
            if (ReturnInst* Return=dyn_cast<ReturnInst>(Inst)){
                Value* ReturnValue=Return->getReturnValue();
                if (!isa<Constant>(ReturnValue))
                    ReturnValues.push_back(ReturnValue);
                //outs() << "Return:" << *ReturnValue << "\n";
            }
        }
    }
    
    outs() << "Domain items:\n";
    outs() << "Index\tItem\n";
    int DomainIndex=0;
    //Adds function arguments to the domain
    for (Function::arg_iterator I = F.arg_begin(); I != F.arg_end(); ++I){
        Value* Arg = I;
        Domain.push_back(Arg);
        DomainIndices.insert(std::make_pair(Arg, DomainIndex++));
        BitVectorItems.insert(std::make_pair(DomainIndex,Arg));
        outs() << DomainIndex << "ARG:" << *Arg << "\n";
    }//Adds global arguments to the domain
    for (GlobalVariable &GV:F.getParent()->getGlobalList()){
        if (!GV.isConstant() && GV.getName() != "variable_cells" && GV.getName() != "init_vector" && GV.getName() != "shadow_set_success"){
            
            Domain.push_back(&GV);
            DomainIndices.insert(std::make_pair(&GV, DomainIndex++));
            BitVectorItems.insert(std::make_pair(DomainIndex,&GV));
            outs() << DomainIndex << "GV:" << GV << "\n";
        }
    }//Adds global aliases to the domain
    for (GlobalAlias &GA:F.getParent()->getAliasList()){
        Domain.push_back(&GA);
        DomainIndices.insert(std::make_pair(&GA, DomainIndex++));
        BitVectorItems.insert(std::make_pair(DomainIndex,&GA));
        outs() << DomainIndex << "GA:" << GA << "\n";
    }
    //Adds instructions defining top-level or address-level values and ignores others (e.g. br, ret);
    for (Instruction* Inst:TopoSortedInsts){
        if (Utils::isTopLevelDefinition(Inst)){
            Domain.push_back(Inst);
            DomainIndices.insert(std::make_pair(Inst, DomainIndex++));
            BitVectorItems.insert(std::make_pair(DomainIndex,Inst));
            outs() << DomainIndex << "I:" << *Inst << "\n";
        }
        if (StoreInst* Store=dyn_cast<StoreInst>(Inst)){
            std::unordered_set<Value*> AliasSet;
            if (ArgSet.find(Store->getValueOperand())!=ArgSet.end()){
                AliasSet.insert(Store->getPointerOperand());
                MayArgAliasSet.insert(std::make_pair(Store->getValueOperand(), AliasSet));
                MustArgAliasSet.insert(std::make_pair(Store->getValueOperand(), AliasSet));
            }
            
        }
        if (isa<IndirectBrInst>(Inst)){
            outs() << "There is indirect branch for function "<< F.getName() << "\n";
            return false;
        }
    }
    int DomainSize=Domain.size();
    //Generate propagation on based on each argument
    
    std::unordered_map<Value*, BitVector> InSets;
    std::unordered_map<Value*, BitVector> OutSets;
    std::unordered_map<Value*, BitVector> InPrimeSets;
    std::unordered_map<Value*, BitVector> OutPrimeSets;
    std::unordered_map<Value*, BitVector> GenSets;
    std::unordered_map<Value*, BitVector> KillSets;
    std::unordered_map<Value*, BitVector> PreTouchedSets;
    std::unordered_map<Value*, BitVector> PostTouchedSets;
    std::unordered_map<Instruction*, bool> VisitedInstsAtLeastOnce;
    
    //Defines static GEN and KILL sets
    BitVector Touched(DomainSize,false);
    for (Instruction* Inst:TopoSortedInsts){
        VisitedInstsAtLeastOnce.insert(std::make_pair(Inst,false));
        outs() << *Inst << "-" << Inst << "\n";
        BitVector Gen(DomainSize,false);
        BitVector Kill(DomainSize,false);
        BitVector In(DomainSize,true);
        BitVector Out(DomainSize,true);
        
        if (StoreInst* Store=dyn_cast<StoreInst>(Inst)){
            Value* Target=Store->getPointerOperand();
            Value* Source=Store->getValueOperand();
            Value* AddressOperand=Store->getPointerOperand();
            std::vector<Instruction*> DefinitionList;
            if (AddressDefinitions.find(AddressOperand)!=AddressDefinitions.end()){
                DefinitionList = AddressDefinitions.at(AddressOperand);
                DefinitionList.push_back(Store);
                AddressDefinitions.at(AddressOperand)=DefinitionList;
            }
            else{
                DefinitionList.push_back(Store);
                AddressDefinitions.insert(std::make_pair(AddressOperand,DefinitionList));
            }
            if (DomainIndices.find(Target)!=DomainIndices.end()){
                if (isa<Constant>(Source)){
                    Gen.set(DomainIndices.at(Target));
                }
            }
        }
        
        //Map each GEN and KILL set to the Instructions
        if (GenSets.find(Inst)==GenSets.end()){
            GenSets.insert(std::make_pair(Inst,Gen));
        }
        if (KillSets.find(Inst)==KillSets.end()){
            KillSets.insert(std::make_pair(Inst,Kill));
        }
        if (PreTouchedSets.find(Inst)==PreTouchedSets.end()){
            PreTouchedSets.insert(std::make_pair(Inst,Touched));
        }
        if (PostTouchedSets.find(Inst)==PostTouchedSets.end()){
            PostTouchedSets.insert(std::make_pair(Inst,Touched));
        }
        if (InSets.find(Inst)==InSets.end()){
            InSets.insert(std::make_pair(Inst,In));
        }
        if (OutSets.find(Inst)==OutSets.end()){
            OutSets.insert(std::make_pair(Inst,Out));
        }
        if (InPrimeSets.find(Inst)==InPrimeSets.end()){
            InPrimeSets.insert(std::make_pair(Inst,In));
        }
        if (OutPrimeSets.find(Inst)==OutPrimeSets.end()){
            OutPrimeSets.insert(std::make_pair(Inst,Out));
        }
        
    }
    
    //Iterative Programmer-Defined Values Propagation via Work List Algorithm
    outs() << "Pops of worklist algorithm:\n";
    outs() << "PopCount\tPreTouched\tPostTouched\tIn\tOut\tOutPrime\tGen\tKill\tInst\n";
    int PopIterationCount=0;
    std::vector<Instruction*> WorkList;
    WorkList.push_back(TopoSortedInsts.at(0));
    while (!WorkList.empty()){
        PopIterationCount++;
        Instruction* Inst=WorkList.back();
        WorkList.pop_back();
        //outs() << PopCount << " ";
        BitVector GenStatic=GenSets.at(Inst);
        BitVector KillStatic=KillSets.at(Inst);
        BitVector Gen=GenSets.at(Inst);
        BitVector Kill=KillSets.at(Inst);
        BitVector In=InSets.at(Inst);
        BitVector Out=OutSets.at(Inst);
        
        BitVector OutPrime=OutSets.at(Inst);
        
        const BitVector Empty(DomainSize,false);
        
        //in[n] = U out[p]
        std::vector<Instruction*> Preds;
        Preds=PredsOfInstructions.at(Inst);
        for (Instruction* Pred:Preds){
            //There can be non-entry instructions not in the list since not having any predecessors but with successors
            if (OutSets.find(Pred)!=OutSets.end())
                In&=(OutSets.at(Pred)); //MUST Analysis
        }
        PreTouchedSets.at(Inst)=Touched;
        updateCleanSetsViaMust(Inst, DomainIndices, In, Gen, Kill, Touched);
        
        //out[n] = gen[n] U (in[n] - kill[n]
        BitVector Temp(DomainSize,false);
        Temp=Kill.flip();
        Kill.flip();
        Temp&=In;
        Temp|=Gen;
        Out=Temp;
        
        PostTouchedSets.at(Inst)=Touched;
        InSets.at(Inst)=In;
        OutSets.at(Inst)=Out;
        
        //if out[n] != out'
        //  for each s E succ[n]
        //      add s to worklist
        //if (!(Out.operator==(OutPrime)) || (Gen.operator==(Empty) && Kill.operator==(Empty))||!VisitedInstsAtLeastOnce.at(Inst)){
        if (!(Out.operator==(OutPrime)) || !VisitedInstsAtLeastOnce.at(Inst)){
            //if (!(Out.operator==(OutPrime))){
            std::vector<Instruction*> Successors=SuccsOfInstructions.at(Inst);
            for (Instruction* Succ:Successors){
                WorkList.push_back(Succ);
            }
        }
        //outs() << PopIterationCount << "\t" << Utils::bitVectorStr(PreTouchedSets.at(Inst)) << "\t" << Utils::bitVectorStr(PostTouchedSets.at(Inst)) << "\t" << Utils::bitVectorStr(In) << "\t" << Utils::bitVectorStr(Out) << "\t" << Utils::bitVectorStr(OutPrime) << "\t" << Utils::bitVectorStr(Gen) << "\t" << Utils::bitVectorStr(Kill) << "\t" << *Inst << "\n";
        VisitedInstsAtLeastOnce.at(Inst)=true;
        
    }
    CTotalCount+=PopIterationCount;
    //    int PopIterationCount=0;
    //    bool Converges=false;
    //    while (!Converges){
    //        PopIterationCount++;
    //        for (Instruction* Inst:TopoSortedInsts){
    //            BitVector In=InSets.at(Inst);
    //            BitVector Out=OutSets.at(Inst);
    //            BitVector Gen=GenSets.at(Inst);
    //            BitVector Kill=KillSets.at(Inst);
    //            BitVector Temp(DomainSize,false);
    //            BitVector InPrime(DomainSize,false);
    //            BitVector OutPrime(DomainSize,false);
    //            InPrime=In;
    //            OutPrime=Out;
    //            InPrimeSets.at(Inst)=InPrime;
    //            OutPrimeSets.at(Inst)=OutPrime;
    //
    //            //in[n] = U out[p]
    //            std::vector<Instruction*> Preds;
    //            Preds=PredsOfInstructions.at(Inst);
    //            for (Instruction* Pred:Preds){
    //                if (OutSets.find(Pred)!=OutSets.end())
    //                    In&=OutSets.at(Pred);
    //            }
    //            PreTouchedSets.at(Inst)=Touched;
    //
    //            BitVector RealIn(DomainSize,true);
    //            RealIn&=Touched;
    //            RealIn&=In;
    //
    //            updateSets(Inst, DomainIndices, In, Gen, Kill, Touched);
    //
    //            //out[n] = gen[n] U (in[n] - kill[n]
    //            Temp=Kill.flip();
    //            Kill.flip();
    //            Temp&=In;
    //            Temp|=Gen;
    //            Out=Temp;
    //
    //            PostTouchedSets.at(Inst)=Touched;
    //
    //            BitVector RealOut(DomainSize,true);
    //            RealOut&=Touched;
    //            RealOut&=Out;
    //
    //            InSets.at(Inst)=In;
    //            OutSets.at(Inst)=Out;
    //
    //
    //            //outs() << PopIterationCount << "\t" << Utils::bitVectorStr(Gen) << "\t" << Utils::bitVectorStr(Kill) << "\t" << Utils::bitVectorStr(RealIn) << "\t" << Utils::bitVectorStr(RealOut)  << "\t" << Utils::bitVectorStr(In) << "\t" << Utils::bitVectorStr(Out) << "\t" << Utils::bitVectorStr(InPrime) << "\t" << Utils::bitVectorStr(OutPrime) << "\t" << *Inst << "\n";
    //
    //        }
    //        //Check convergence
    //        Converges=(Utils::compareBitVectors(InSets,InPrimeSets) && Utils::compareBitVectors(OutSets,OutPrimeSets) && PopIterationCount>1);
    //    }
    //    TotalCount+=PopIterationCount;
    //Prints IN and OUT sets for each node (Instruction)
    //outs() << "IN and OUT sets for each node via Worklist:\n";
    //outs() << "In\tOut\tNode\n";
    for (Instruction* Inst:TopoSortedInsts){
        BitVector In=InSets.at(Inst);
        In&=PreTouchedSets.at(Inst);
        InSets.at(Inst)=In;//Eliminates the domain items set but never encountered during iterations
        BitVector Out=OutSets.at(Inst);
        Out&=PostTouchedSets.at(Inst);
        OutSets.at(Inst)=Out;
        GlobalReachingInCleanData.insert(std::make_pair(Inst, InSets.at(Inst)));
        GlobalReachingOutCleanData.insert(std::make_pair(Inst, OutSets.at(Inst)));
        ReachingInCleanData.insert(std::make_pair(Inst, InSets.at(Inst)));
        ReachingOutCleanData.insert(std::make_pair(Inst, OutSets.at(Inst)));
        //outs() << Utils::BitVectorStr(MayInSets.at(Inst)) << "\t" << Utils::BitVectorStr(MayOutSets.at(Inst)) << "\t" << *Inst << "\n";
    }
    
    //Print digest of clean operands before and after each instruction
    std::ofstream fcprop=Utils::openFile(Utils::getHomePath()+CleanOutputFolder+F.getName().str() + ".txt");
    for (Instruction* Inst:TopoSortedInsts){
        BitVector PreTouched=PreTouchedSets.at(Inst);
        BitVector PostTouched=PostTouchedSets.at(Inst);
        BitVector InSet=InSets.at(Inst);
        BitVector OutSet=OutSets.at(Inst);
        outs() << *Inst << " /// Pre:[";
        fcprop << Utils::valueStr(Inst) << " /// Pre:[";
        for (unsigned int i=0;i<Inst->getNumOperands();i++){
            Value* Operand=Inst->getOperand(i);
            if (DomainIndices.find(Operand)!=DomainIndices.end())
                if (InSet[DomainIndices.at(Operand)] && PreTouched[DomainIndices.at(Operand)]){
                    outs() << " " << Utils::identifierStr(Operand) << " ";
                    fcprop << " " << Utils::identifierStr(Operand) << " ";
                }
        }
        outs() <<  "],";
        fcprop <<  "],";
        outs() << "Post:[";
        fcprop << "Post:[";
        if (DomainIndices.find(Inst)!=DomainIndices.end())
            if (OutSet[DomainIndices.at(Inst)] && PostTouched[DomainIndices.at(Inst)]){
                outs() << " " << Utils::identifierStr(Inst) << " ";
                fcprop << " " << Utils::identifierStr(Inst) << " ";
            }
        for (unsigned int i=0;i<Inst->getNumOperands();i++){
            Value* Operand=Inst->getOperand(i);
            if (DomainIndices.find(Operand)!=DomainIndices.end())
                if (OutSet[DomainIndices.at(Operand)] && PostTouched[DomainIndices.at(Operand)]){
                    outs() << " " << Utils::identifierStr(Operand) << " ";
                    fcprop << " " << Utils::identifierStr(Operand) << " ";
                }
        }
        outs() <<  "]\n";
        fcprop <<  "]\n";
    }
    Utils::closeFile(&fcprop);
    
    //Update tainting and propagating functions based on return values
    bool MustReturn=(ReturnValues.size()>0);
    bool MustOutput=(OutputArgs.size()>0);
    
    for (Instruction* Inst:ExitInstructions){
        BitVector Out=OutSets.at(Inst);
        BitVector PostTouched=PostTouchedSets.at(Inst);
        //If the function returns a tainted value regardless of its arguments, add the function to the tainting functions set
        for (Value* Return:ReturnValues){
            if (DomainIndices.find(Return)!=DomainIndices.end()){
                if (Out[DomainIndices.at(Return)] && PostTouched[DomainIndices.at(Return)])
                    MustReturn&=true;
                else
                    MustReturn&=false;
            }
        }
        for (Value* Output:OutputArgs){
            if (DomainIndices.find(Output)!=DomainIndices.end()){
                if (Out[DomainIndices.at(Output)] && PostTouched[DomainIndices.at(Output)])
                    MustOutput&=true;
                else
                    MustOutput&=false;
            }
        }
        
    }
    if (MustReturn){
        CleanFunctions.insert(F.getName());
    }
    if (MustOutput){
        CleanFunctions.insert(F.getName());
    }
    int TotalLoadCount=0;
    int ProgrammerDefinedLoadCount=0;
    int TotalStoreCount=0;
    int ProgrammerDefinedStoreCount=0;
    int TotalControlCount=0;
    int ProgrammerDefinedControlCount=0;
    
    //Checks loop induction variales are trusted or not
    int TotalLoopControlCount=0;
    int ProgrammerDefinedLoopControlCount=0;
    for (BasicBlock &BB:F){
        //Loop* Loop=LI.getLoopFor(&BB);
        if (LI.isLoopHeader(&BB)){
            outs() << "Loop Header:" << BB << "\n";
            for (Instruction &Inst:BB){
                if (BranchInst* Branch=dyn_cast<BranchInst>(&Inst)){
                    if (Branch->isConditional()){
                        Value* Condition=Branch->getCondition();
                        if (Instruction* ConditionInst = dyn_cast<Instruction>(Condition)){
                            if (AllCLoopControlInstructions.find(ConditionInst)==AllCLoopControlInstructions.end()){
                                AllCLoopControlInstructions.insert(ConditionInst);
                                outs() << "Loop Condition:" << *Condition << "\n";
                                TotalLoopControlCount++;
                            
                                if (DomainIndices.find(ConditionInst)!=DomainIndices.end() && GlobalReachingOutCleanData.at(ConditionInst)[DomainIndices.at(ConditionInst)]){
                                    CleanLoopControlInstructions.insert(ConditionInst);
                                    outs() << "Trusted Loop Condition:" << *Condition << "\n";
                                    ProgrammerDefinedLoopControlCount++;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    outs() << "Function" << F.getName() << "\n";
    
    std::unordered_map<BasicBlock*, std::vector<BranchInst*>> LoopConditions;
    for (Instruction* Inst:TopoSortedInsts){
        BitVector InSet=GlobalReachingInCleanData.at(Inst);
        BitVector OutSet=GlobalReachingOutCleanData.at(Inst);
        if (LoadInst* Load = dyn_cast<LoadInst>(Inst)) {
            Value* Source=Load->getPointerOperand();
            AllCLoadInstructions.insert(Inst);
            TotalLoadCount++;
            if (DomainIndices.find(Source)!=DomainIndices.end() && InSet[DomainIndices.at(Source)]){
                CleanLoadInstructions.insert(Inst);
                ProgrammerDefinedLoadCount++;
            }
        }
        if (StoreInst* Store = dyn_cast<StoreInst>(Inst)) {
            Value* Target=Store->getPointerOperand();
            AllCStoreInstructions.insert(Inst);
            TotalStoreCount++;
            if (DomainIndices.find(Target)!=DomainIndices.end() && OutSet[DomainIndices.at(Target)]){
                CleanStoreInstructions.insert(Inst);
                ProgrammerDefinedStoreCount++;
            }
        }
        //Check whether the condition variables of branch instruction is programmer-defined or not
        if (BranchInst* Branch = dyn_cast<BranchInst>(Inst)) {
            if (Branch->isConditional()){
                Value* Condition=Branch->getCondition();
                if (Instruction* ConditionInst = dyn_cast<Instruction>(Condition)){
                    bool CleanCriticalVariableExists=true;
                    for (unsigned int i=0;i<ConditionInst->getNumOperands();i++){
                        if (DomainIndices.find(ConditionInst->getOperand(i))!=DomainIndices.end()){
                            CleanCriticalVariableExists&=InSet[DomainIndices.at(ConditionInst->getOperand(i))];
                            
                        }
                    }
                    AllCControlInstructions.insert(ConditionInst);
                    TotalControlCount++;
                    if (CleanCriticalVariableExists && CleanControlInstructions.find(ConditionInst)==CleanControlInstructions.end()){
                        CleanControlInstructions.insert(ConditionInst);
                        ProgrammerDefinedControlCount++;
                    }
                }
            }
        }
        //Check whether the condition variables of branch instruction is programmer-defined or not
        else if (IndirectBrInst* IndirectBr = dyn_cast<IndirectBrInst>(Inst)) {
            Value* Address=IndirectBr->getAddress();
            if (Instruction* AddressInst = dyn_cast<Instruction>(Address)){
                bool CleanCriticalVariableExists=false;
                if (DomainIndices.find(AddressInst)!=DomainIndices.end()){
                    CleanCriticalVariableExists=InSet[DomainIndices.at(AddressInst)];
                }
                AllCControlInstructions.insert(AddressInst);
                TotalControlCount++;
                if (CleanCriticalVariableExists && CleanControlInstructions.find(AddressInst)==CleanControlInstructions.end()){
                    CleanControlInstructions.insert(AddressInst);
                    ProgrammerDefinedControlCount++;
                }
            }
        }
        //Check whether the condition variables of switch instruction is tainted or not
        else if (SwitchInst* Switch = dyn_cast<SwitchInst>(Inst)) {
            Value* Condition=Switch->getCondition();
            if(!isa<Constant>(Condition)){
                if (Instruction* ConditionInst = dyn_cast<Instruction>(Condition)){
                    bool CleanCriticalVariableExists=false;
                    if (DomainIndices.find(ConditionInst)!=DomainIndices.end()){
                        CleanCriticalVariableExists=InSet[DomainIndices.at(ConditionInst)];
                    }
                    AllCControlInstructions.insert(ConditionInst);
                    TotalControlCount++;
                    if (CleanCriticalVariableExists && CleanControlInstructions.find(ConditionInst)==CleanControlInstructions.end()){
                        CleanControlInstructions.insert(ConditionInst);
                        ProgrammerDefinedControlCount++;
                    }
                }
            }
        }
    }
    now=std::clock();
    double elapsed_secs = ((now-begin) / (double)CLOCKS_PER_SEC);
    outs() << F.getName().str() << "\t" << PopIterationCount << "\t" << Utils::round(elapsed_secs) << "\t" << TotalLoadCount << "\t" << ProgrammerDefinedLoadCount << "\t" << TotalControlCount << "\t" << ProgrammerDefinedControlCount << "\t" << TotalLoopControlCount << "\t" << ProgrammerDefinedLoopControlCount << "\n";
    
    fcbenchmarks << F.getName().str() << "\t" << PopIterationCount << "\t" << Utils::round(elapsed_secs) << "\t" << TotalLoadCount << "\t" << ProgrammerDefinedLoadCount << "\t" << TotalControlCount << "\t" << ProgrammerDefinedControlCount << "\t" << TotalLoopControlCount << "\t" << ProgrammerDefinedLoopControlCount << "\n";
    
    return false;
}
static RegisterPass<CleanDataPropagationModeller> F("cfunctionmodeller", "Clean Data Propagation Modeller of Functions");



char ModuleCleanDataChecker::ID = 0;
void ModuleCleanDataChecker::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<CallGraphWrapperPass>();
    AU.addRequired<CleanDataPropagationModeller>();
    AU.setPreservesAll();
}
bool ModuleCleanDataChecker::runOnModule(Module &M) {
    std::clock_t begin;
    begin=std::clock();
    std::clock_t now;
    std::string ModuleName=M.getName().str().substr((M.getName().str().find_last_of('/')+1));
    ModuleName=ModuleName.substr(0,ModuleName.find_last_of('.'));
    fcbenchmarks=Utils::openFile(Utils::getTimeStampedFilePath("cbenchmarks",CleanOutputFolder,ModuleName,"tsv"));
    //fcbenchmarks=Utils::openFile("cbenchmarks.tsv");
    fcbenchmarks << "Function\tPopIteration-Count\tAnalysis-Time\tTotal-Load\tClean-Load\tTotal-Store\tClean-Store\tTotal-Control\tClean-Control\tTotal-LoopControl\tClean-LoopControl\n";
    fcfunctions=Utils::openFile(Utils::getTimeStampedFilePath("cfunctions",CleanOutputFolder,ModuleName,"txt"));
    //fcfunctions=Utils::openFile("cfunctions.txt");
    fcrules=Utils::openFile(Utils::getTimeStampedFilePath("crules",CleanOutputFolder,ModuleName,"txt"));
    //fcrules=Utils::openFile("crules.txt");
    fcinsts=Utils::openFile(Utils::getTimeStampedFilePath("cinsts",CleanOutputFolder,ModuleName,"txt"));
    //fcinsts=Utils::openFile("cinsts.txt");
    outs() << "Module: " << M.getName() << "\n";
    GlobalReachingInCleanData.clear();
    std::unordered_map<Instruction*,std::vector<BitVector>> MayReachingTaints;
    std::map<Value*, std::string> AllLocalVariables;
    CallGraph& CG = getAnalysis<CallGraphWrapperPass>().getCallGraph();
    std::vector<Function*> Calls;
    //CG.dump();
    scc_iterator<CallGraph*> CGSCCI = scc_begin(&CG);
    
    
    
    std::unordered_map<Value*, Value*> PointersToVariableNames;
    
    //enclave identififier variable
    GlobalVariable* GlobalEID;
    std::vector<Instruction*> MainExits;
    
    LLVMContext &GlobalContext = M.getContext();
    IRBuilder<> StrBuilder(GlobalContext);
    Function* F = M.getFunction("main");
    Function::iterator BB = F->begin();
    BasicBlock::iterator I = BB->begin();
    StrBuilder.SetInsertPoint(&*I);
    
    //Instrumentation for global variable names
    //Add a generic non variable string for addresses that do not belong a variable
    Value *PointerForNonVariableAddresses = StrBuilder.CreateGlobalStringPtr("nonvar", ".str.nonvar");
    //Add global variable names as string constants from debug information
    for (GlobalVariable &GV:M.getGlobalList()){
        if (GV.getName() == "global_eid")
            GlobalEID=&GV;
        if (!GV.isConstant() && GV.getName() != "shadow_variable_cells" && GV.getName() != "shadow_init_vector" && GV.getName() != "shadow_set_success" && GV.getName() != "ocall_table_Enclave" && GV.getName() != "sgx_errlist" && GV.getName() != "global_eid"){
            Value *PointerToGlobalVariableName = StrBuilder.CreateGlobalStringPtr(Utils::identifierStr(&GV), ".str.vars."+Utils::identifierStr(&GV).substr(1)+".glb");
            PointersToVariableNames.insert(std::make_pair(&GV, PointerToGlobalVariableName));
            //outs() << "Value:" << GV << " " << &GV << "- GV:" <<  *PointerToGlobalVariableName << "\n";
        }
    }
    
    //To enumerate addresses dealing with clean/trusted data
    int VariableIdIndex=0;
    
    while (!CGSCCI.isAtEnd()){
        const std::vector<CallGraphNode*>& BottomUpNodes = *CGSCCI;
        for (CallGraphNode* Node:BottomUpNodes){
            if (Node->getFunction()!=NULL){
                Function *F = Node->getFunction();
                if (!F->empty() && !Utils::isOneOfInstrumentedFunctions(F)){
                    //if (F->getName()=="lm_init"){ //For convergence issue (gen=empty kill=empty controls are removed)
                    //if (F->getName()=="validate_timespec"){ //outset issue for the instruction which is a pred of some and does not have succ.
                    outs() << "Function: " << F->getName() << "\n";
                    
                    Calls.push_back(F);
                   
                    
                    CleanDataPropagationModeller* Modeller=&getAnalysis<CleanDataPropagationModeller>(*F);
                 
                    //VariableNamesAnalysis* VariableNames;
                    
                    std::unordered_map<Value*, int> DomainIndices=Modeller->getDomainIndices();
                    std::unordered_map<Instruction*, BitVector> ReachingInCleanData=Modeller->getReachingInCleanData();
                    std::unordered_map<Instruction*, BitVector> ReachingOutCleanData=Modeller->getReachingOutCleanData();
                    std::vector<Instruction*> TopoSortedInsts=Modeller->getTopoSortedInsts();
                    
                    
                    //Instrumentation for local variable names
                    //Add local variable names as string constants from debug information
                    std::unordered_map<Value*, std::string> LocalVariablesNames=Modeller->getLocalVariables();
                    //std::unordered_map<Value*, std::string> LocalVariablesNames;
                    for (std::pair<Value*,std::string> pair:LocalVariablesNames){
                        Value *PointerToFunctionAddedLocalVariableName = StrBuilder.CreateGlobalStringPtr(pair.second+"@"+F->getName().str(), ".str.vars."+pair.second+"."+F->getName().str());
                        PointersToVariableNames.insert(std::make_pair(pair.first, PointerToFunctionAddedLocalVariableName));
                        //outs() << "Value:" << *pair.first << " - V:" <<  *PointerToFunctionAddedLocalVariableName << "\n";
                    }
                    
                    //Create lists of store/load instructions operating with only trusted/clean data to appropriate data structures
                    for (Instruction* Inst:TopoSortedInsts){
                        if (ReachingInCleanData.find(Inst)!=ReachingInCleanData.end() && ReachingOutCleanData.find(Inst)!=ReachingOutCleanData.end()){
                            BitVector In=ReachingInCleanData.at(Inst);
                            BitVector Out=ReachingOutCleanData.at(Inst);
                            //If the load instruction loads clean data which should be part of IN set of that instruction
                            if (LoadInst* Load=dyn_cast<LoadInst>(Inst)){
                                Value* Address=Load->getPointerOperand();
                                if (DomainIndices.find(Address)!=DomainIndices.end()){
                                    if (In[DomainIndices.at(Address)]){
                                        if (VariableIds.find(Address)==VariableIds.end())
                                            VariableIds.insert(std::make_pair(Address, VariableIdIndex++));
                                        CleanLoads.insert(Load);
                                        std::vector<Instruction*> Instructions;
                                        if (CleanMemoryOps.find(Address)==CleanMemoryOps.end()){
                                            Instructions.push_back(Load);
                                            CleanMemoryOps.insert(std::make_pair(Address, Instructions));
                                        }
                                        else{
                                            Instructions=CleanMemoryOps.at(Address);
                                            Instructions.push_back(Load);
                                            CleanMemoryOps.at(Address)=Instructions;
                                        }
                                    }
                                }
                            }
                            //If the store instruction stores clean data which should be part of the OUT set of that instruction
                            else if (StoreInst* Store=dyn_cast<StoreInst>(Inst)){
                                Value* Address=Store->getPointerOperand();
                                if (DomainIndices.find(Address)!=DomainIndices.end()){
                                    if (Out[DomainIndices.at(Address)]){
                                        if (VariableIds.find(Address)==VariableIds.end())
                                            VariableIds.insert(std::make_pair(Address, VariableIdIndex++));
                                        CleanStores.insert(Store);
                                        std::vector<Instruction*> Instructions;
                                        if (CleanMemoryOps.find(Address)==CleanMemoryOps.end()){
                                            Instructions.push_back(Load);
                                            CleanMemoryOps.insert(std::make_pair(Address, Instructions));
                                        }
                                        else{
                                            Instructions=CleanMemoryOps.at(Address);
                                            Instructions.push_back(Load);
                                            CleanMemoryOps.at(Address)=Instructions;
                                        }
                                    }
                                }
                            }
                            if (F->getName()=="main"){//Think about exit functions in non-main functions
                                if (isa<ReturnInst>(Inst) || isa<UnreachableInst>(Inst))
                                    MainExits.push_back(Inst);
                            }
                            //                else{
                            //                    if (isa<UnreachableInst>(Inst))
                            //                        MainExits.push_back(Inst);
                            //                }
                        }
                    }
                }
            }
        }
        ++CGSCCI;
    }
    
    //Outputs the functions that must return clean/trusted data
    if (CleanFunctions.size()>0){
        outs() << "***************** " << CleanFunctions.size() << " Clean Functions **************\n";
        for (std::string FunctionName:CleanFunctions){
            outs() << FunctionName << "\n";
            fcfunctions << FunctionName << "\n";
        }
    }
    //Outputs the functions that must propagate clean/trusted data based on the given argument rule
    if (CleanPropagatingFunctions.size()>0){
        outs() << "***************** " << CleanPropagatingFunctions.size() << " Rule Based Propagating Functions **************\n";
        fcrules << "***************** " << CleanPropagatingFunctions.size() << " Rule Based Propagating Functions **************\n";
        for (std::pair<std::string,std::vector<PropagationRule>> pair:CleanPropagatingFunctions){
            for (PropagationRule Rule:pair.second){
                outs() << pair.first << " IsVariadic:" << Rule.VariadicArgs << " VariadicIndex:" << Rule.VariadicStartIndex << " Return:" << Utils::returnDescription(Rule.Return) << " Source Args:{" ;
                fcrules<< pair.first << " IsVariadic:" << Rule.VariadicArgs << " VariadicIndex:" << Rule.VariadicStartIndex << " Return:" << Utils::returnDescription(Rule.Return) << " Source Args:{" ;
                
                int SourceArgsCount=Rule.SourceArgs.size();
                for (int i=0;i<SourceArgsCount-1;i++)
                    outs() << Rule.SourceArgs[i] << ",";
                if (SourceArgsCount>0)
                    outs() << Rule.SourceArgs[SourceArgsCount-1];
                int TaintableArgsCount=Rule.TaintableArgs.size();
                outs() << "} Taintable Args:{";
                fcrules << "} Taintable Args:{";
                
                for (int i=0;i<TaintableArgsCount-1;i++){
                    outs() << Rule.TaintableArgs[i] << ",";
                    fcrules << Rule.TaintableArgs[i] << ",";
                }
                if (TaintableArgsCount>0){
                    outs() << Rule.TaintableArgs[TaintableArgsCount-1];
                    fcrules << Rule.TaintableArgs[TaintableArgsCount-1];
                }
                outs() << "}\n";
                fcrules << "}\n";
            }
        }
    }
    
    //Output the control (e.g. CMP) instructions that checks with trusted/clean data
    outs() << "*******************************Control instructions that have to operate with programmer-defined values\n";
    for (Instruction* Inst:CleanControlInstructions){
        const DebugLoc &DebugInfo = Inst->getDebugLoc();
        if (DebugInfo.get()!=NULL){
            std::string directory = DebugInfo->getDirectory();
            std::string filePath = DebugInfo->getFilename();
            int line = DebugInfo->getLine();
            int column = DebugInfo->getColumn();
            outs() << Inst->getParent()->getParent()->getName() << ":" << *Inst << "\n@file:" << filePath << " @line:" << line << " @column:" <<  column  << " \n";
            fcinsts << Inst->getParent()->getParent()->getName().str() << ":" << Utils::valueStr(Inst) << "\n@file:" << filePath << " @line:" << line << " @column:" <<  column  << " \n";
        }
        else{
            outs() << Inst->getParent()->getParent()->getName()<< ":" << *Inst << "\n";
            fcinsts << Inst->getParent()->getParent()->getName().str() << ":" << Utils::valueStr(Inst) << "\n";
        }
    }
    //Output all control (e.g. CMP) instructions
    outs() << "*******************************All control instructions\n";
    for (Instruction* Inst:AllCControlInstructions){
        const DebugLoc &DebugInfo = Inst->getDebugLoc();
        if (DebugInfo.get()!=NULL){
            std::string directory = DebugInfo->getDirectory();
            std::string filePath = DebugInfo->getFilename();
            int line = DebugInfo->getLine();
            int column = DebugInfo->getColumn();
            outs() << Inst->getParent()->getParent()->getName() << ":" << *Inst << "\n@file:" << filePath << " @line:" << line << " @column:" <<  column  << " \n";
        }
        else
            outs() << Inst->getParent()->getParent()->getName()<< ":" << *Inst << "\n";
    }
    float ControlRatio=((float)CleanControlInstructions.size()/(float)AllCControlInstructions.size())*100;
    outs() << "# of control instructions checking clean values:"<< CleanControlInstructions.size() << "\n";
    outs() << "# of all control instructions:"<< AllCControlInstructions.size() << "\n";
    outs() << "ratio of programmer variables:"<< Utils::round((double)ControlRatio) << "\n";
    
    float LoadRatio=((float)CleanLoadInstructions.size()/(float)AllCLoadInstructions.size())*100;
    outs() << "# of load instructions reading clean values:"<< CleanLoadInstructions.size() << "\n";
    outs() << "# of all load instructions:"<< AllCLoadInstructions.size() << "\n";
    outs() << "ratio:"<< Utils::round((double)LoadRatio) << "\n";
    
    float StoreRatio=((float)CleanStoreInstructions.size()/(float)AllCStoreInstructions.size())*100;
    outs() << "# of store instructions reading clean values:"<< CleanStoreInstructions.size() << "\n";
    outs() << "# of all store instructions:"<< AllCStoreInstructions.size() << "\n";
    outs() << "ratio:"<< Utils::round((double)StoreRatio) << "\n";
    
    now=std::clock();
    //outs() << "\nnow: " << now;
    double elapsed_secs = ((now-begin) / (double)CLOCKS_PER_SEC);
    
    fcbenchmarks << M.getName().str().substr((M.getName().str().find_last_of('/')+1)) << "\t" << CTotalCount << "\t" << Utils::round(elapsed_secs) << "\t" << AllCLoadInstructions.size() << "\t" << CleanLoadInstructions.size() << "\t" << AllCStoreInstructions.size() << "\t" << CleanStoreInstructions.size() << "\t" << AllCControlInstructions.size() << "\t" << CleanControlInstructions.size() << "\t" << AllCLoopControlInstructions.size() << "\t" << CleanLoopControlInstructions.size() << "\n";
    
    Utils::closeFile(&fcfunctions);
    Utils::closeFile(&fcrules);
    Utils::closeFile(&fcinsts);
    Utils::closeFile(&fcbenchmarks);
    
    
    
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////////////////////Instrumentation start here/////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    Function* Main=M.getFunction("main");
    LLVMContext& fcontext = F->getContext();
    Instruction* FirstMainInst=&(Main->getEntryBlock().front());
    const DebugLoc& FirstDB=FirstMainInst->getDebugLoc();
    //outs() << "FirstMainInst:" << *FirstMainInst << "\n";
    
    //Instrument initialize_enclave
    std::vector<Value*> IEArgsVector;
    ArrayRef<Value*> IEArgs(IEArgsVector);
    Function* IECall=M.getFunction("initialize_enclave");
    Instruction *IEInstruction = CallInst::Create(IECall,IEArgs,"ie");
    //outs() << "IE Call:" << *IEInstruction << "\n";
    FirstMainInst->getParent()->getInstList().insert(FirstMainInst->getIterator(), IEInstruction);
    
    
    //Instrument a load instruction to read global_eid value
    LoadInst* GlobalEIDInstruction = new LoadInst(GlobalEID, "enclaveid");
    IEInstruction->getParent()->getInstList().insertAfter(IEInstruction->getIterator(), GlobalEIDInstruction);
    
    
    std::vector<Value*> PFArgsVector;
    PFArgsVector.push_back(GlobalEIDInstruction);
    ArrayRef<Value*> PFArgs(PFArgsVector);
    Function* PFCall=M.getFunction("printf_helloworld");
    Instruction *PFInstruction = CallInst::Create(PFCall,PFArgs,"pf");
    //outs() << "PF Call:" << *PFInstruction << "\n";
    GlobalEIDInstruction->getParent()->getInstList().insertAfter(GlobalEIDInstruction->getIterator(), PFInstruction);
    
    
    Function* SSMCall=M.getFunction("set_shadow_memory");
    ConstantInt* Size  = ConstantInt::get(IntegerType::getInt32Ty(fcontext), VariableIdIndex);
    std::vector<Value*> SSMArgsVector;
    SSMArgsVector.push_back(GlobalEIDInstruction);
    SSMArgsVector.push_back(Size);
    ArrayRef<Value*> SSMArgs(SSMArgsVector);
    Instruction *SSMInstruction = CallInst::Create(SSMCall,SSMArgs,"ssm");
    //outs() << "SSM Call:" << *SSMInstruction << "\n";
    PFInstruction->getParent()->getInstList().insertAfter(PFInstruction->getIterator(), SSMInstruction);
    //GlobalEIDInstruction->getParent()->getInstList().insertAfter(GlobalEIDInstruction->getIterator(), SSMInstruction);
    //outs() << "BB after insertion:" << *(PFInstruction->getParent()) << "\n" ;
    
    
    //Instrument destroy_enclave function before the program exits
    std::vector<Value*> DEArgsVector;
    //DEArgsVector.push_back(GlobalEIDInstruction);
    ArrayRef<Value*> DEArgs(DEArgsVector);
    Function* DECall=M.getFunction("destroy_enclave");
    int exitCount=0;
    for (Instruction* Exit:MainExits){
        Instruction *DEInstruction = CallInst::Create(DECall,DEArgs,"de"+std::to_string(exitCount++));
        //outs() << "DE Call:" << std::to_string(exitCount++) << *DEInstruction << "\n";
        if (isa<UnreachableInst>(Exit))
            Exit->getParent()->getInstList().insert(Exit->getIterator().operator--(), DEInstruction);
        else
            Exit->getParent()->getInstList().insert(Exit->getIterator(), DEInstruction);
        //outs() << "BB after insertion:" << *(Exit->getParent()) << "\n" ;
        
    }
    
    //    Function* Main=M.getFunction("main");
    //    LLVMContext& fcontext = F->getContext();
    //    Instruction* FirstMainInst=&(Main->getEntryBlock().front());
    //    outs() << "FirstMainInst:" << *FirstMainInst << "\n";
    //    Function* SSMCall=M.getFunction("set_shadow_memory");
    //    ConstantInt* Size  = ConstantInt::get(IntegerType::getInt32Ty(fcontext), VariableIds.size());
    //    std::vector<Value*> ArgsVector;
    //    ArgsVector.push_back(Size);
    //    ArrayRef<Value*> Args(ArgsVector);
    //    Instruction *SSMInstruction = CallInst::Create(SSMCall,Args);
    //    outs() << "SSM Call:" << *SSMInstruction << "\n";
    //
    //    FirstMainInst->getParent()->getInstList().insert(FirstMainInst->getIterator(), SSMInstruction);
    //    outs() << "BB after insertion:" << *(FirstMainInst->getParent()) << "\n" ;
    
    LLVMContext& context = M.getContext();
    
    for (std::pair<Value*, Value*> pair:PointersToVariableNames)
        outs() << "Var:" << *pair.first  << "-<" << pair.first << ">" <<  "Str:"<<  *pair.second << "\n";
    for (LoadInst* Load:CleanLoads){
        
        outs() << "LoadInst:" << *Load << "\n";
        Value* Address=Load->getPointerOperand();
        outs() << "Address:" << *Address << "-" <<  Address << "\n";
        Function* VICall;
        if (Utils::identifierStr(Load) == "%enclaveid")
            continue;
        ConstantInt* VariableId  = ConstantInt::get(IntegerType::getInt32Ty(context), VariableIds.at(Address));
        //ConstantInt* AccessType  = ConstantInt::get(IntegerType::getInt32Ty(context), READ_ACCESS);
        Value* VariableValue  = Load;
        //Value* VarName;
        outs() << "VariableId" << *VariableId << "\n";
        outs() << "VariableValue" << *VariableValue << "\n";
           
        if (PointerType *PT = dyn_cast<PointerType>(VariableValue->getType())){
            if (IntegerType* IT=dyn_cast<IntegerType>(PT->getPointerElementType())){
                if (IT->getBitWidth()==1)
                    VICall=M.getFunction("vi_call_use_p1");
                else if (IT->getBitWidth()==8)
                    VICall=M.getFunction("vi_call_use_p8");
                else if (IT->getBitWidth()==16)
                    VICall=M.getFunction("vi_call_use_p16");
                else if (IT->getBitWidth()==32)
                    VICall=M.getFunction("vi_call_use_p32");
                else if (IT->getBitWidth()==64)
                    VICall=M.getFunction("vi_call_use_p64");
                else
                    continue;
            }
            else
                continue;
        }
        else{
            if (IntegerType* IT=dyn_cast<IntegerType>(VariableValue->getType())){
                if (IT->getSignBit()==1){
                    if (IT->getBitWidth()==1)
                        VICall=M.getFunction("vi_call_use_1");
                    else if (IT->getBitWidth()==8)
                        VICall=M.getFunction("vi_call_use_8");
                    else if (IT->getBitWidth()==16)
                        VICall=M.getFunction("vi_call_use_16");
                    else if (IT->getBitWidth()==32)
                        VICall=M.getFunction("vi_call_use_32");
                    else if (IT->getBitWidth()==64)
                        VICall=M.getFunction("vi_call_use_64");
                    else
                        continue;
                }
                else{
                    if (IT->getBitWidth()==1)
                        VICall=M.getFunction("vi_call_use_u1");
                    else if (IT->getBitWidth()==8)
                        VICall=M.getFunction("vi_call_use_u8");
                    else if (IT->getBitWidth()==16)
                        VICall=M.getFunction("vi_call_use_u16");
                    else if (IT->getBitWidth()==32)
                        VICall=M.getFunction("vi_call_use_u32");
                    else if (IT->getBitWidth()==64)
                        VICall=M.getFunction("vi_call_use_u64");
                    else
                        continue;
                }
            }
            else
                continue;
        }
        
//        if (PointersToVariableNames.find(Address)!=PointersToVariableNames.end())
//            VarName=PointersToVariableNames.at(Address);
//        else
//            VarName=PointerForNonVariableAddresses;
//        outs() << "VarName" << *VarName << "\n";
      
        std::vector<Value*> ArgsVector;
        ArgsVector.push_back(VariableId);
        //ArgsVector.push_back(AccessType);
        ArgsVector.push_back(VariableValue);
        //ArgsVector.push_back(VarName);
        ArrayRef<Value*> Args(ArgsVector);
        Instruction *VIInstruction = CallInst::Create(VICall,Args);
        outs() << "VI Call:" << *VIInstruction << "\n";
        Load->getParent()->getInstList().insertAfter(Load->getIterator(), VIInstruction);
        //outs() << "BB after insertion:" << *(Load->getParent()) << "\n" ;
    }
    for (StoreInst* Store:CleanStores){
        outs() << "StoreInst:" << *Store << "\n";
        Value* Address=Store->getPointerOperand();
        Function* VICall;
        ConstantInt* VariableId  = ConstantInt::get(IntegerType::getInt32Ty(context), VariableIds.at(Address));
        //ConstantInt* AccessType  = ConstantInt::get(IntegerType::getInt32Ty(context), WRITE_ACCESS);
        Value* VariableValue  = Store->getValueOperand();
//        Value* VarName;
        
        if (PointerType *PT = dyn_cast<PointerType>(VariableValue->getType())){
            if (IntegerType* IT=dyn_cast<IntegerType>(PT->getPointerElementType())){
                if (IT->getBitWidth()==1)
                    VICall=M.getFunction("vi_call_def_p1");
                else if (IT->getBitWidth()==8)
                    VICall=M.getFunction("vi_call_def_p8");
                else if (IT->getBitWidth()==16)
                    VICall=M.getFunction("vi_call_def_p16");
                else if (IT->getBitWidth()==32)
                    VICall=M.getFunction("vi_call_def_p32");
                else if (IT->getBitWidth()==64)
                    VICall=M.getFunction("vi_call_def_p64");
                else
                    continue;
            }
            else
                continue;
        }
        else{
            if (IntegerType* IT=dyn_cast<IntegerType>(VariableValue->getType())){
                if (IT->getSignBit()==1){
                    if (IT->getBitWidth()==1)
                        VICall=M.getFunction("vi_call_def_1");
                    else if (IT->getBitWidth()==8)
                        VICall=M.getFunction("vi_call_def_8");
                    else if (IT->getBitWidth()==16)
                        VICall=M.getFunction("vi_call_def_16");
                    else if (IT->getBitWidth()==32)
                        VICall=M.getFunction("vi_call_def_32");
                    else if (IT->getBitWidth()==64)
                        VICall=M.getFunction("vi_call_def_64");
                    else
                        continue;
                }
                else{
                    if (IT->getBitWidth()==1)
                        VICall=M.getFunction("vi_call_def_u1");
                    else if (IT->getBitWidth()==8)
                        VICall=M.getFunction("vi_call_def_u8");
                    else if (IT->getBitWidth()==16)
                        VICall=M.getFunction("vi_call_def_u16");
                    else if (IT->getBitWidth()==32)
                        VICall=M.getFunction("vi_call_def_u32");
                    else if (IT->getBitWidth()==64)
                        VICall=M.getFunction("vi_call_def_u64");
                    else
                        continue;
                }
            }
            else
                continue;
        }
        
//        if (PointersToVariableNames.find(Address)!=PointersToVariableNames.end())
//            VarName=PointersToVariableNames.at(Address);
//        else
//            VarName=PointerForNonVariableAddresses;
        
        std::vector<Value*> ArgsVector;
        ArgsVector.push_back(VariableId);
        //ArgsVector.push_back(AccessType);
        ArgsVector.push_back(VariableValue);
        //ArgsVector.push_back(VarName);
        ArrayRef<Value*> Args(ArgsVector);
        Instruction *VIInstruction = CallInst::Create(VICall,Args);
        outs() << "VI Call:" << *VIInstruction << "\n";
        Store->getParent()->getInstList().insert(Store->getIterator(), VIInstruction);
        //outs() << "BB after insertion:" << *(Store->getParent()) << "\n" ;
    }
    
    return true;
    
}

static RegisterPass<ModuleCleanDataChecker> S("tvi", "Variable-Integrity Checks Insertions For Trusted Variables");
