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

#include <llvm/IR/DebugLoc.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include "llvm/IR/LLVMContext.h"

#include "llvm/Transforms/VI/Variables.h"
#include "llvm/Transforms/VI/Utils.h"

using namespace llvm;

char VariableNamesAnalysis::ID = 0;

bool VariableNamesAnalysis::runOnFunction(Function &F) {
    if (F.empty())
        return false;
    LocalVariables.clear();
    //outs() << "Func:" << F.getName() << "\n";
    for(auto I = inst_begin(F), E = inst_end(F); I != E; ++I) {
        Instruction* Inst=&*I;
        if (DbgDeclareInst* DbgDeclare=dyn_cast<DbgDeclareInst>(Inst)){
            Value* Address=DbgDeclare->getAddress();
            DILocalVariable*  DILocalVariable=DbgDeclare->getVariable();
            LocalVariables.insert(std::make_pair(Address, DILocalVariable->getName()));
            //outs() << "Address:" << Utils::identifierStr(Address)  << " - Name:" << DILocalVariable->getName() << "\n";
        }
        if (DbgValueInst* DbgValue=dyn_cast<DbgValueInst>(Inst)){
            Value* Value=DbgValue->getValue();
            DILocalVariable*  DILocalVariable=DbgValue->getVariable();
            LocalVariables.insert(std::make_pair(Value, DILocalVariable->getName()));
            //outs() << "Value:" << Utils::identifierStr(Value)  << " - Name:" << DILocalVariable->getName() << "\n";
        }
    }
    return false;
}
static RegisterPass<VariableNamesAnalysis> W("variablenames", "A Pass Mapping Variables Names with SSA identifiers from Debug Information");
