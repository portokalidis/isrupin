#include <llvm/Pass.h>
//#include <llvm/Analysis/CallGraphSCCPass.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;

namespace {

class Try1 : public BasicBlockPass {
public:
	static char ID;

	Try1() : BasicBlockPass(ID) {}

	virtual bool runOnBasicBlock(BasicBlock &BB) {
		BasicBlock::iterator e;
		ReturnInst *retInst;
		Value *retVal;
		Constant *constVal;

		errs() << "BB: " << BB << "\n";
		for (BasicBlock::iterator i = BB.begin(), e = BB.end();
				i != e; i++) {
			//errs() << '\t' << *i << "\n";
			retInst = dyn_cast<ReturnInst>(&*i);
			if (retInst) {
				retVal = retInst->getReturnValue();
				if (retVal)  {
					errs() << "\t >> Boom! " << *retVal << "\n";
					constVal = dyn_cast<Constant>(&*retVal);
					if (constVal) {
						errs() << "\t\t CONSTANT = " << constVal->getUniqueInteger() << "\n";
					} else {
					}
				}
			}
		}
		return false;
	}
};

}

char Try1::ID = 0;
static RegisterPass<Try1> X("try2", "First try", false, false);

