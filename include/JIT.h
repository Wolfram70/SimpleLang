#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/ExecutorProcessControl.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/Mangling.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"

namespace llvm {
namespace orc {
class SimpleJIT {
 private:
  std::unique_ptr<ExecutionSession> execS;
  DataLayout DL;
  MangleAndInterner MAI;
  RTDyldObjectLinkingLayer objectLayer;
  IRCompileLayer compileLayer;
  JITDylib &mainJD;

 public:
  SimpleJIT(std::unique_ptr<ExecutionSession> execS,
            JITTargetMachineBuilder JTMB, DataLayout DL)
      : execS(std::move(execS)),
        DL(std::move(DL)),
        MAI(*this->execS, this->DL),
        objectLayer(*this->execS,
                    []() { return std::make_unique<SectionMemoryManager>(); }),
        compileLayer(*this->execS, objectLayer,
                     std::make_unique<ConcurrentIRCompiler>(std::move(JTMB))),
        mainJD(this->execS->createBareJITDylib("<main>")) {
    mainJD.addGenerator(
        cantFail(DynamicLibrarySearchGenerator::GetForCurrentProcess(
            DL.getGlobalPrefix())));
    if (JTMB.getTargetTriple().isOSBinFormatCOFF()) {
      objectLayer.setOverrideObjectFlagsWithResponsibilityFlags(true);
      objectLayer.setAutoClaimResponsibilityForObjectSymbols(true);
    }
  }

  ~SimpleJIT() {
    if (auto err = execS->endSession()) {
      execS->reportError(std::move(err));
    }
  }

  static Expected<std::unique_ptr<SimpleJIT>> create() {
    auto EPC = SelfExecutorProcessControl::Create();
    if (!EPC) {
      return EPC.takeError();
    }

    auto execS = std::make_unique<ExecutionSession>(std::move(*EPC));

    JITTargetMachineBuilder JTMB(
        execS->getExecutorProcessControl().getTargetTriple());

    auto DL = JTMB.getDefaultDataLayoutForTarget();
    if (!DL) {
      return DL.takeError();
    }

    return std::make_unique<SimpleJIT>(std::move(execS), std::move(JTMB),
                                       std::move(*DL));
  }

  const DataLayout &getDataLayout() const { return DL; }

  JITDylib &getMainJITDylib() { return mainJD; }

  Error addModule(ThreadSafeModule TSM, ResourceTrackerSP RT = nullptr) {
    if (!RT) {
      RT = mainJD.getDefaultResourceTracker();
    }

    return compileLayer.add(RT, std::move(TSM));
  }

  Expected<JITEvaluatedSymbol> lookup(StringRef name) {
    return execS->lookup({&mainJD}, MAI(name.str()));
  }
};
}  // namespace orc
}  // namespace llvm
