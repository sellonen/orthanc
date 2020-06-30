# https://github.com/emscripten-core/emscripten/blob/master/src/settings.js

if (NOT "${EMSCRIPTEN_TRAP_MODE}" STREQUAL "")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s BINARYEN_TRAP_MODE='\"${EMSCRIPTEN_TRAP_MODE}\"'")
endif()

# "DISABLE_EXCEPTION_CATCHING" is a "compile+link" option. HOWEVER,
# setting it inside "WASM_FLAGS" creates link errors, at least with
# side modules. TODO: Understand why
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s DISABLE_EXCEPTION_CATCHING=0")
#set(WASM_FLAGS "${WASM_FLAGS} -s DISABLE_EXCEPTION_CATCHING=0")

if (EMSCRIPTEN_TARGET_MODE STREQUAL "wasm")
  # WebAssembly
  set(WASM_FLAGS "${WASM_FLAGS} -s WASM=1")
  
elseif (EMSCRIPTEN_TARGET_MODE STREQUAL "asm.js")
  # asm.js targeting IE 11
  set(WASM_FLAGS "-s WASM=0 -s ASM_JS=2 -s LEGACY_VM_SUPPORT=1")

else()
  message(FATAL_ERROR "Bad value for EMSCRIPTEN_TARGET_MODE: ${EMSCRIPTEN_TARGET_MODE}")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(WASM_FLAGS "${WASM_FLAGS} -s SAFE_HEAP=1 -s ASSERTIONS=1")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WASM_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WASM_FLAGS}")
