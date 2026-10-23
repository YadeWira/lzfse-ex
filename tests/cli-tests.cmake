# Tests for the lzfse command line tool.
#
# Regression tests for the cases that used to crash or exhaust memory:
# an empty input file (double free / endless loop) and input that is not a
# valid LZFSE stream (the tool used to double its output buffer until malloc
# failed).
#
# Required variables:
#   LZFSE_CLI     - path to the built command line tool
#   LZFSE_WRAPPER - optional runner (Wine when cross-compiling for Windows)
#   LZFSE_WORK_DIR - scratch directory for this test

if(NOT LZFSE_CLI)
  message(FATAL_ERROR "LZFSE_CLI not set")
endif()

file(MAKE_DIRECTORY "${LZFSE_WORK_DIR}")

# See round-trip.cmake: the runner is passed as a '|'-separated list.
if(DEFINED LZFSE_WRAPPER AND NOT "${LZFSE_WRAPPER}" STREQUAL "")
  string(REPLACE "|" ";" LZFSE_WRAPPER "${LZFSE_WRAPPER}")
else()
  set(LZFSE_WRAPPER "")
endif()

function(lzfse_run out_var)
  execute_process(COMMAND ${LZFSE_WRAPPER} ${LZFSE_CLI} ${ARGN}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
    TIMEOUT 60)
  set(${out_var} "${result}" PARENT_SCOPE)
  if(result MATCHES "timeout")
    message(FATAL_ERROR "lzfse ${ARGN} hung (timeout)")
  endif()
endfunction()

# --- Empty input file: encode must produce a valid stream, decode it back ---
set(EMPTY "${LZFSE_WORK_DIR}/empty.bin")
set(EMPTY_CMP "${LZFSE_WORK_DIR}/empty.lzfse")
set(EMPTY_OUT "${LZFSE_WORK_DIR}/empty.out")
file(WRITE "${EMPTY}" "")

lzfse_run(result -encode -i "${EMPTY}" -o "${EMPTY_CMP}")
if(NOT result EQUAL 0)
  message(FATAL_ERROR "encoding an empty file failed (${result}): ${error_output}")
endif()

file(SIZE "${EMPTY_CMP}" empty_cmp_size)
if(empty_cmp_size EQUAL 0)
  message(FATAL_ERROR "encoding an empty file produced an empty stream")
endif()

lzfse_run(result -decode -i "${EMPTY_CMP}" -o "${EMPTY_OUT}")
if(NOT result EQUAL 0)
  message(FATAL_ERROR "decoding an empty stream failed (${result}): ${error_output}")
endif()

file(SIZE "${EMPTY_OUT}" empty_out_size)
if(NOT empty_out_size EQUAL 0)
  message(FATAL_ERROR "decoding an empty stream produced ${empty_out_size} bytes")
endif()

# --- Decoding an empty file is invalid input, not a crash or a hang -------
lzfse_run(result -decode -i "${EMPTY}" -o "${EMPTY_OUT}")
if(result MATCHES "timeout")
  message(FATAL_ERROR "decoding an empty file hung")
endif()
if(result EQUAL 0)
  message(FATAL_ERROR "decoding an empty file was reported as success")
endif()

# --- Garbage input must fail quickly -------------------------------------
set(GARBAGE "${LZFSE_WORK_DIR}/garbage.bin")
set(GARBAGE_OUT "${LZFSE_WORK_DIR}/garbage.out")
file(WRITE "${GARBAGE}" "not an lzfse stream, definitely not one at all")

lzfse_run(result -decode -i "${GARBAGE}" -o "${GARBAGE_OUT}")
if(result EQUAL 0)
  message(FATAL_ERROR "decoding garbage was reported as success")
endif()

# --- Normal roundtrip still works ----------------------------------------
set(INPUT "${LZFSE_WORK_DIR}/input.bin")
set(INPUT_CMP "${LZFSE_WORK_DIR}/input-roundtrip.lzfse")
set(INPUT_OUT "${LZFSE_WORK_DIR}/input-roundtrip.out")
file(WRITE "${INPUT}" "The quick brown fox jumps over the lazy dog. 0123456789")
lzfse_run(result -encode -i "${INPUT}" -o "${INPUT_CMP}")
if(NOT result EQUAL 0)
  message(FATAL_ERROR "encoding failed (${result})")
endif()
lzfse_run(result -decode -i "${INPUT_CMP}" -o "${INPUT_OUT}")
if(NOT result EQUAL 0)
  message(FATAL_ERROR "decoding failed (${result})")
endif()

file(SHA256 "${INPUT}" input_hash)
file(SHA256 "${INPUT_OUT}" output_hash)
if(NOT input_hash STREQUAL output_hash)
  message(FATAL_ERROR "roundtrip output does not match the input")
endif()
