//===- DFIInjector.cpp
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IntrinsicInst.h"
#include <llvm/IR/DebugLoc.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/DIBuilder.h"

#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/Transforms/VI/CFA.h"
#include "llvm/Transforms/VI/FVI.h"
#include "llvm/Transforms/VI/Variables.h"
#include "llvm/Transforms/VI/Utils.h"
#include <unordered_set>
#include <unordered_map>

using namespace llvm;
char IntraShadowCheckInjectorForAll::ID = 0;
void IntraShadowCheckInjectorForAll::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<VariableNamesAnalysis>();
    AU.setPreservesAll();
}
bool  IntraShadowCheckInjectorForAll::doInitialization(Module &M){
    return false;
}
bool IntraShadowCheckInjectorForAll::runOnModule(Module &M){
    std::unordered_map<Value*, int> VariableIds;
    std::unordered_map<Value*, Value*> PointersToVariableNames;
    std::unordered_map<Value*, std::string> LocalVariablesNames;
    
    //enclave identififier variable
    GlobalVariable* GlobalEID;
    
    int VariableId=0;
    LLVMContext &MContext = M.getContext();
    IRBuilder<> StrBuilder(MContext);
    Function* F = M.getFunction("main");
    Function::iterator BB = F->begin();
    BasicBlock::iterator I = BB->begin();
    StrBuilder.SetInsertPoint(&*I);
    //Add variable names as string constants
    Value *PointerForNonVariableAddresses = StrBuilder.CreateGlobalStringPtr("nonvar", ".str.nonvar");
    for (GlobalVariable &GV:M.getGlobalList()){
        if (GV.getName() == "global_eid")
                GlobalEID=&GV;
        if (!GV.isConstant() && GV.getName() != "shadow_variable_cells" && GV.getName() != "shadow_init_vector" && GV.getName() != "shadow_set_success" && GV.getName() != "ocall_table_Enclave" && GV.getName() != "sgx_errlist" && GV.getName() != "global_eid"){
                        Value *PointerToGlobalVariableName = StrBuilder.CreateGlobalStringPtr(Utils::identifierStr(&GV), ".str.vars."+Utils::identifierStr(&GV).substr(1)+".glb");
            PointersToVariableNames.insert(std::make_pair(&GV, PointerToGlobalVariableName));
            if (VariableIds.find(&GV)==VariableIds.end()){
                VariableIds.insert(std::make_pair(&GV, VariableId++));
                //outs() << "Id:" << (VariableId-1) << " Value:" << GV << "\n";
            }
        }
    }
    
    std::vector<Instruction*> MainExits;
    for (Function &F:M){
        if (F.empty() || Utils::isOneOfInstrumentedFunctions(&F))
            continue;
        VariableNamesAnalysis* VNAnalysis;
        VNAnalysis=&getAnalysis<VariableNamesAnalysis>(F);
        LocalVariablesNames=VNAnalysis->getLocalVariables();
        for (std::pair<Value*,std::string> pair:LocalVariablesNames){
            //Adds function name to the local variable name
            Value *PointerToFunctionAddedLocalVariableName = StrBuilder.CreateGlobalStringPtr(pair.second+"@"+F.getName().str(), ".str.vars."+pair.second+"."+F.getName().str());
            PointersToVariableNames.insert(std::make_pair(pair.first, PointerToFunctionAddedLocalVariableName));
        }
        for (BasicBlock &BB:F){
            for (Instruction &Inst:BB){
                if (StoreInst* Store=dyn_cast<StoreInst>(&Inst)){
                    Value* Address=Store->getPointerOperand();
                    if (VariableIds.find(Address)==VariableIds.end()){
                        VariableIds.insert(std::make_pair(Address, VariableId++));
                        //outs() << "Id:" << (VariableId-1) << " Value:" << *Address << " Func:" << F.getName() << "\n";
                    }
                }
                else if (LoadInst* Load=dyn_cast<LoadInst>(&Inst)){
                    Value* Address=Load->getPointerOperand();
                    if (VariableIds.find(Address)==VariableIds.end()){
                        VariableIds.insert(std::make_pair(Address, VariableId++));
                        //outs() << "Id:" << (VariableId-1) << " Value:" << *Address << " Func:" << F.getName() << "\n";
                    }
                }
                if (F.getName()=="main"){//Think about exit functions in non-main functions
                    if (isa<ReturnInst>(&Inst) || isa<UnreachableInst>(&Inst))
                        MainExits.push_back(&Inst);
                }
                //                else{
                //                    if (isa<UnreachableInst>(&Inst))
                //                        MainExits.push_back(&Inst);
                //                }
            }
        }
    }
    
    for (Function &F:M){
        if (F.empty() || Utils::isOneOfInstrumentedFunctions(&F))
            continue;
        if (F.getName()=="main"){
            //Find first instruction of main function
            Function* Main=M.getFunction("main");
            LLVMContext& fcontext = F.getContext();
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
            ConstantInt* Size  = ConstantInt::get(IntegerType::getInt32Ty(fcontext), VariableIds.size());
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
            
        }
        for (BasicBlock &BB:F){
            for (Instruction &Inst:BB){
                if (StoreInst* Store=dyn_cast<StoreInst>(&Inst)){
                    outs() << "Function:" << F.getName() << "\n";
                    outs() << "StoreInst:" << *Store << "\n";
                    Value* Address=Store->getPointerOperand();
                    //VICall->addAttribute(AttributeList::FunctionIndex, Attribute::NoInline);
                    Function* VICall=NULL;
                    ConstantInt* VariableId  = ConstantInt::get(IntegerType::getInt32Ty(MContext), VariableIds.at(Address));
                    outs() << "VariableId: " << *VariableId << "\n";
                    ConstantInt* AccessType  = ConstantInt::get(IntegerType::getInt32Ty(MContext), WRITE_ACCESS);
                    outs() << "AccessType: " << *AccessType << "\n";
                    Value* VariableValue  = Store->getValueOperand();
                    //                    if (F.getName()=="bi_init")
                    //                        outs() << "VariableValue: " << *VariableValue << "\n";
                    //
                    
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
                            //                            if (IT->getBitWidth()!=32){
                            //                                Twine Identifier=Twine(Utils::identifierStr(VariableValue).substr(1)+std::to_string(ConstantID++));
                            //                                CastInstruction=CastInst::CreateIntegerCast(VariableValue, IntegerType::getInt32Ty(MContext),true,Identifier);
                            //                                IsThereCast=true;
                            //                                outs() << "Cast:" << *CastInstruction << "\n";
                            //                            }
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
                    outs() << "VariableValue: " << *VariableValue << "\n";
//                    Value* VarName;
//                    if (PointersToVariableNames.find(Address)!=PointersToVariableNames.end())
//                        VarName=PointersToVariableNames.at(Address);
//                    else
//                        VarName=PointerForNonVariableAddresses;
//                    outs() << "VarName: " << *VarName << "\n";
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
                else if (LoadInst* Load=dyn_cast<LoadInst>(&Inst)){
                    outs() << "Function:" << F.getName() << "\n";
                    outs() << "LoadInst:" << *Load << "\n";
                    Value* Address=Load->getPointerOperand();
                    outs() << "Address:" << *Address << "-" <<  Address << "\n";
                    if (Utils::identifierStr(Load) == "%enclaveid")
                        continue;
                    Function* VICall;
                    //VICall->addAttribute(AttributeList::FunctionIndex, Attribute::NoInline);
                    ConstantInt* VariableId  = ConstantInt::get(IntegerType::getInt32Ty(MContext), VariableIds.at(Address));
                    ConstantInt* AccessType  = ConstantInt::get(IntegerType::getInt32Ty(MContext), READ_ACCESS);
                    Value* VariableValue  = Load;
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
                            //                            if (IT->getBitWidth()!=32){
                            //                                Twine Identifier=Twine(Utils::identifierStr(VariableValue).substr(1)+std::to_string(ConstantID++));
                            //                                CastInstruction=CastInst::CreateIntegerCast(VariableValue, IntegerType::getInt32Ty(MContext),true,Identifier);
                            //                                IsThereCast=true;
                            //                                outs() << "Cast:" << *CastInstruction << "\n";
                            //                            }
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
                    outs() << "VariableValue: " << *VariableValue << "\n";
//                    Value* VarName;
//                    if (PointersToVariableNames.find(Address)!=PointersToVariableNames.end())
//                        VarName=PointersToVariableNames.at(Address);
//                    else
//                        VarName=PointerForNonVariableAddresses;
//                    outs() << "VarName: " << *VarName << "\n";
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
            }
        }
    }
    return true;
}
static RegisterPass<IntraShadowCheckInjectorForAll> N("fvi", "Variable-Integrity Checks Insertions For All Addresses");
