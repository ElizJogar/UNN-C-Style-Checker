cmake_minimum_required(VERSION 3.10)
project(c-style-cast-checker)

find_package(LLVM REQUIRED CONFIG)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")

include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

set(CLANG_LIBS
    clangAST
    clangASTMatchers
    clangAnalysis
    clangBasic
    clangDriver
    clangEdit
    clangFrontend
    clangFrontendTool
    clangLex
    clangParse
    clangSema
    clangEdit
    clangRewrite
    clangRewriteFrontend
    clangStaticAnalyzerFrontend
    clangStaticAnalyzerCheckers
    clangStaticAnalyzerCore
    clangSerialization
    clangToolingCore
    clangTooling
    clangFormat
    )

set (LIBS
    LLVMScalarOpts
    LLVMInstCombine
    LLVMTransformUtils
    LLVMAnalysis
    LLVMTarget
    LLVMOption # Support
    LLVMMCParser # MC, Support
    LLVMMC # Object, Support
    LLVMObject # BitReader, Core, Support
    LLVMBitReader # Core, Support
    LLVMCore # Support
    LLVMSupport
    )

list(FIND LLVM_AVAILABLE_LIBS "LLVMFrontendOpenMP" _llvm_frontendOpenMP)
if (${_llvm_frontendOpenMP} GREATER -1)
    list(APPEND LIBS LLVMFrontendOpenMP)
endif()

add_executable(${PROJECT_NAME} main.cpp)
add_executable(test_app test/test.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS})

#if (${LLVM_LINK_LLVM_DYLIB})
#  target_link_libraries(${PROJECT_NAME} PRIVATE clang-cpp)
#else()
  #target_link_libraries(${PROJECT_NAME} PRIVATE ${CLANG_LIBS})
#endif()
target_link_libraries(${PROJECT_NAME} PRIVATE clang-cpp)
