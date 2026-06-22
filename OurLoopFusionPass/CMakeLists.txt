add_llvm_library( LLVMOurLoopFusionPass MODULE BUILDTREE_ONLY
  OurLoopFusionPass.cpp

  DEPENDS
  intrinsics_gen
  PLUGIN_TOOL
  opt
  )
