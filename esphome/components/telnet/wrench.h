#define WRENCH_COMBINED
/*******************************************************************************
Copyright (c) 2023 Curt Hartung -- curt.hartung@gmail.com

MIT Licence

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*******************************************************************************/
#ifndef _WRENCH_H
#define _WRENCH_H
/*------------------------------------------------------------------------------*/

#define WRENCH_VERSION_MAJOR 3
#define WRENCH_VERSION_MINOR 2
#define WRENCH_VERSION_BUILD 5

/************************************************************************
The compiler was not designed to be particularly memory or space efficient, for
small embedded systems it is strongly reccommeded (nay, required) that
only bytecode be executed. This flag allows the source code to be
explicitly unavailable. Esp32-class processors have no trouble compiling
on-the-fly but ATMega/SAMD21 are a no-go here.
*/
// #define WRENCH_WITHOUT_COMPILER
/***********************************************************************/

/***********************************************************************
Cause the interpreter to compile into the smallest program size
possible at the cost of some speed. This loss is from the removal of
unrolled loops and inlined functionality, it also makes
use of some goto spaghetti which can play havok with branch-prediction
and actually slow down PC-class processors.
WRENCH_REALLY_COMPACT reduces size further by removing the jumptable
interpreter in favor of a giant switch(). This saves ~6k at the cost
of a chunk of speed so only use it if you need to.
*/
// #define WRENCH_COMPACT           // saves a lot, costs some speed
// #define WRENCH_REALLY_COMPACT    // saves a little more, costs more speed
/***********************************************************************/

/***********************************************************************
Some architectures (most embedded) really don't like reading more than
8 bits on an unaligned memory location. If your architecture
allows it (PC, Mac, linux, unix etc..) this is a good optimization
*/
// #define WRENCH_UNALIGNED_READS
/***********************************************************************/

/***********************************************************************
wrench automatically detect endian-ness and defines these three
macros for reading data from the code stream, if you have special
memory requirements, as some embedded systems to, you may re-defines
them here, but they MUST match the endian-ness of the target
architecture, see vm.h for the current definitions
*/
// #define READ_32_FROM_PC( P )
// #define READ_16_FROM_PC( P )
// #define READ_8_FROM_PC( P )
/***********************************************************************/

/************************************************************************
how many stack locations the wrench state will pre-allocate. The stack
cannot be grown so this needs to be enough for the life of the process.
this does NOT include global/array space which is allocated separately, just
function calls. Unless you are using piles of local data AND recursing
like crazy a modest size should be more than enough.

This will consume 8 bytes per stack entry
*/
#define WRENCH_DEFAULT_STACK_SIZE 64
/***********************************************************************/

/************************************************************************
including debug functionality incurs NO performance penalty on the VM

..BUT

it does add a small size penalty. If you are trying to cram wrench
into the smallest possible space, this compiles out the debug functions.
You can still run debug-enabled code, the VM will just skip it.
*/
//!!!!!!!!!!!!! EXPERIMENTAL DO NOT USE
// #define WRENCH_INCLUDE_DEBUG_CODE

/************************************************************************
if you WANT full sprintf support for floats (%f/%g) this adds it, at
the cost of using the standard c library for it, not for small embedded
systems but larger ones? who cares.
*/
// #define WRENCH_FLOAT_SPRINTF

/***********************************************************************/

/************************************************************************
File operations: define ONE of these. If you use "Custom" then you
need to link in a source file with the functions defined in:

  /discrete_src/std_io_defs.h

examples are in

  /discrete_src/std_io_linux.cpp
  /discrete_src/std_io_win32.cpp
  /discrete_src/std_io_spiffs.cpp
  /discrete_src/std_io_littlefs.cpp

*/
// #define WRENCH_WIN32_FILE_IO
// #define WRENCH_LINUX_FILE_IO
// #define WRENCH_SPIFFS_FILE_IO    // !!!!!!!!!!!! PRE-RELEASE DO NOT USE
// #define WRENCH_LITTLEFS_FILE_IO  // !!!!!!!!!!!! PRE-RELEASE DO NOT USE
// #define WRENCH_CUSTOM_FILE_IO
/***********************************************************************/

#include <stdint.h>
#include <stddef.h>

struct WRState;
struct WRValue;
struct WRContext;
struct WRFunction;

// hashing function used inside wrench, it's a stripped down murmer,
// not academically fantastic but very good and very fast
uint32_t wr_hash(const void *dat, const int len);
uint32_t wr_hashStr(const char *dat);

// in order to minimize text segment size, only a minimum of strings
// are embedded in the code. error messages are very verbose enums
enum WRError {
  WR_ERR_None = 0,

  WR_ERR_compiler_not_loaded,
  WR_ERR_function_hash_signature_not_found,
  WR_ERR_library_constant_not_loaded,
  WR_ERR_unknown_opcode,
  WR_ERR_unexpected_EOF,
  WR_ERR_unexpected_token,
  WR_ERR_bad_expression,
  WR_ERR_bad_label,
  WR_ERR_statement_expected,
  WR_ERR_unterminated_string_literal,
  WR_ERR_newline_in_string_literal,
  WR_ERR_bad_string_escape_sequence,
  WR_ERR_tried_to_load_non_resolvable,
  WR_ERR_break_keyword_not_in_looping_structure,
  WR_ERR_continue_keyword_not_in_looping_structure,
  WR_ERR_expected_while,
  WR_ERR_compiler_panic,
  WR_ERR_constant_redefined,
  WR_ERR_struct_in_struct,

  WR_ERR_run_must_be_called_by_itself_first,
  WR_ERR_hash_table_size_exceeded,
  WR_ERR_wrench_function_not_found,
  WR_ERR_array_must_be_indexed,
  WR_ERR_context_not_found,

  WR_ERR_usr_data_template_already_exists,
  WR_ERR_usr_data_already_exists,
  WR_ERR_usr_data_template_not_found,
  WR_ERR_usr_data_refernce_not_found,

  WR_ERR_bad_goto_label,
  WR_ERR_bad_goto_location,
  WR_ERR_goto_target_not_found,

  WR_ERR_switch_with_no_cases,
  WR_ERR_switch_case_or_default_expected,
  WR_ERR_switch_construction_error,
  WR_ERR_switch_bad_case_hash,
  WR_ERR_switch_duplicate_case,

  WR_ERR_bad_bytecode_CRC,

  WR_ERR_execute_function_zero_called_more_than_once,

  WR_warning_enums_follow,

  WR_WARN_c_function_not_found,
  WR_WARN_lib_function_not_found,
};

/***************************************************************/
/**************************************************************/
//                       State Management

// create/destroy a WRState object that can run multiple contexts/threads
WRState *wr_newState(int stackSize = WRENCH_DEFAULT_STACK_SIZE);
void wr_destroyState(WRState *w);

/***************************************************************/
/**************************************************************/
//                        Running Code

// compile source code and return a new'ed block of bytecode ready
// for wr_run(), this memory must be delete[]'ed
// return value is a WRError
// optionally an "errMsg" buffer can be passed which will output a
//     human-readable string of what went wrong and where.
// compilerOptionFlags : OR'ed mask of of WrenchCompilerOptionFlags:
enum WrenchCompilerOptionFlags {
  WR_INCLUDE_GLOBALS = 1 << 0,  // include all globals NOTE: wr_getGlobalRef(...)
                                // will not function without this

  WR_EMBED_DEBUG_CODE = 1 << 1,  // include per-instruction NOTE: WrenchDebugInterface
                                 // will not function without this

  WR_EMBED_SOURCE_CODE = 1 << 2,  // include a copy of the source code
};

WRError wr_compile(const char *source, const int size, unsigned char **out, int *outLen, char *errMsg = 0,
                   const unsigned int compilerOptionFlags = WR_INCLUDE_GLOBALS);

// w:          state (see wr_newState)
// block:      location of bytecode
// blockSize:  number of bytes in the block

// RETURNS:    an allocated WRContext
//             NOTE: This Context is automatically destyroyed when
//             wr_destroyState() is called, but can be manually deleted
//             with wr_destroyContext(...) (see below)
WRContext *wr_run(WRState *w, const unsigned char *block, const int blockSize);

// macro for automatically freeing the WRContext
#define wr_runOnce(w, block, blockSize) wr_destroyContext(wr_run((w), (block), (blockSize)))

// After wr_run(...) or wr_callFunction(...) has run, there is always a
// return value (default 0) this function fetches it
WRValue *wr_returnValueFromLastCall(WRState *w);

// wr_run actually calls these two functions back-to back. If you want
// to separate the process you can call them on your own.
WRContext *wr_newContext(WRState *w, const unsigned char *block, const int blockSize);
WRValue *wr_executeContext(WRContext *context);

// after wr_run() this allows any function in the script to be
// called with the given arguments, returning a single value
//
// contextId:    the context in which the function was loaded
// functionName: plaintext name of the function to be called
// argv:         array of WRValues to call the function with (optional)
// argn:         how many arguments argv contains (optional, but if
//               zero then argv will be ignored)
// RETURNS:      WRValue returned by the function/program, or NULL for
//               error (see w->err for the code)
WRValue *wr_callFunction(WRContext *context, const char *functionName, const WRValue *argv = 0, const int argn = 0);

// exactly the same as the above but the hash is supplied directly to
// save compute time, us wr_hashStr(...) to obtain the hash of the
// functionName
WRValue *wr_callFunction(WRContext *context, const int32_t hash, const WRValue *argv = 0, const int argn = 0);

// The raw function pointer can be pre-loaded with wr_getFunction() and
// passed directly
// This method is exposed so functions can be called with an absolute
// minimum of overhead
WRValue *wr_callFunction(WRContext *context, WRFunction *function, const WRValue *argv = 0, const int argn = 0);

// after wrench executes it may have set an error code, this is how to
// retreive it. This sytems is coarse at the moment. Re-entering the
// interpreter clears the last error
WRError wr_getLastError(WRState *w);

// once wr_run() is called the returned context object can be used to
// pre-fetch a function pointer. This reduces the overhead of calling
// that function to almost nothing.
WRFunction *wr_getFunction(WRContext *context, const char *functionName);

// want direct access to a global? okay then, if the code was compiled
// with symbols (see compile(...) suite) then this will give you a
// reference to it
WRValue *wr_getGlobalRef(WRContext *context, const char *label);

// Destroy a context you no longer need and free up all the memory it
// was using
// NOTE: all contexts ARE AUTOMATICALLY FREED when wr_destroyState(...)
//       is called, it it NOT necessary to call this on each context
void wr_destroyContext(WRContext *context);

/***************************************************************/
/***************************************************************/
//                 Callbacks from wrench

// register a function inside a state that can be called (by ALL
// contexts)
// callback will contain:
// c:             context called from
// argv:          pointer to arguments function was called
//                with (may be null!)
// argn:          how many arguments it was called with
// retVal:        this value will be passed back, default: integer zero
// usr:           opaque pointer function was registered with
typedef void (*WR_C_CALLBACK)(WRContext *c, const WRValue *argv, const int argn, WRValue &retVal, void *usr);

// IMPORTANT: The values passed may be references (keepin' it real) so
// always use the getters inside the WRValue class:

// int:    .asInt();
// float:  .asFloat();
// binary: .array( unsigned int* size, char* type );
// string: .c_str( unsigned int* len );

// tests:
// .isFloat() fast check if this is a float value
// .isInt() fast check if this is a float value

// this will do its pest to represent the value as a string to the
// supplied buffer, len is maximum size allowed
// string: .asString(char* string, size_t len )

// w:        state to register with (will be available to all contexts)
// name:     name of the function
// function: callback (see typdef above)
// usr:      opaque pointer that will be passed to the callback (optional)
void wr_registerFunction(WRState *w, const char *name, WR_C_CALLBACK function, void *usr = 0);

// serialize WRValues to and from binary
// NOTE: on success, wr_serialize returns a malloc()'d buffer that must be freed!
bool wr_serialize(char **buf, int *len, const WRValue &value);
bool wr_deserialize(WRContext *context, WRValue &value, const char *buf, const int len);

/***************************************************************/
/***************************************************************/
//                   wrench Library calls

// Wrench allows libraries to be loaded in the form of <name>::<func>
// these calls impose no overhead if not used

// library calls do assume a deep knowledge of the internal workings of
// wrench so the callback is an absolute minimum of info:
// stackTop: current top of the stack, arguments have been pushed here,
//           and any return value must be placed here, not pushed! see
//           one of the many library examples
// argn:     how many arguments this function was called with. no
//           guarantees are made about matching a function signature,
//           library calls validate if they care.
// stackTop         : [return value]
// stackTop - 1     : [argN ]
// stackTop - 2     : [argN-1]
// ...
// stackTop - (N-1) : [arg2]
// stackTop - N     : [arg1]
typedef void (*WR_LIB_CALLBACK)(WRValue *stackTop, const int argn, WRContext *context);

// w:         state to register with (will be available to all contexts)
// signature: library signature must be in the form of <lib>::<name>
//            ex: "math::cos"
// function:  callback (see typdef above)
void wr_registerLibraryFunction(WRState *w, const char *signature, WR_LIB_CALLBACK function);
void wr_registerLibraryConstant(WRState *w, const char *signature, const WRValue &value);

// you can write your own libraries (look at std.cpp for all the
// standard ones) but several have been provided:

void wr_loadAllLibs(WRState *w);  // load all libraries in one call

void wr_loadSysLib(WRState *w);        // system/internal functions
void wr_loadMathLib(WRState *w);       // provides most of the calls in math.h
void wr_loadStdLib(WRState *w);        // standard functions like sprintf/rand/
void wr_loadIOLib(WRState *w);         // IO funcs (time/file/io)
void wr_loadStringLib(WRState *w);     // string functions
void wr_loadMessageLib(WRState *w);    // messaging between contexts
void wr_loadSerializeLib(WRState *w);  // serialize WRValues to and from binary

// arduino-specific functions, be sure to add arduino_lib.cpp to your
// sketch. much thanks to Koepel for contributing
void wr_loadAllArduinoLibs(WRState *w);

void wr_loadArduinoSTDLib(WRState *w);
void wr_loadArduinoIOLib(WRState *w);
void wr_loadArduinoLCDLib(WRState *w);

/***************************************************************/
/***************************************************************/
//             wrench types and how to make them!

// The WRValue is the basic data unit inside wrench, it can be a
// float/int/array whatever... think of it as a thneed. It's 8 bytes on
// a 32 bit system and you can create one on the stack no problemo, no
// heap will be harmed

// load a value up and make it ready for calling a function
WRValue &wr_makeInt(WRValue *val, int i);
WRValue &wr_makeFloat(WRValue *val, float f);

// a string has to exist in a context so it can be worked with
WRValue &wr_makeString(WRContext *context, WRValue *val, const char *data, const int len = 0);

// turning a value into a container allocates a hash table which must
// be released with destroy!
void wr_makeContainer(WRValue *val, const uint16_t sizeHint = 0);
void wr_destroyContainer(WRValue *val);

void wr_addValueToContainer(WRValue *container, const char *name, WRValue *value);
void wr_addIntToContainer(WRValue *container, const char *name, const int32_t value);
void wr_addFloatToContainer(WRValue *container, const char *name, const float value);
void wr_addArrayToContainer(WRValue *container, const char *name, char *array, const uint32_t size);

/******************************************************************/
//                    "standard" functions

extern int32_t wr_Seed;

// inside-baseball time. This has to be here so stuff that includes
// "wrench.h" can create a WRValue.

/*

 wrench values have a 32-bit type vector

type bits: XXXX  XXXX  ____  ____  ____  ____  __TT
                                        [8-bit type]
           [             32-bit xtype              ]

the bottom two bits 'T' represent the main type:
int       00 -- integer, ui / i are the value
float     01 -- float, f is the value
reference 10 -- reference, *r is a pointer to the real value
extended  11 -- extended type, see upper bits.

This is so operation tables can be constructed 4x4 for very quick
resolution of how to add/subtract/AND/etc... two values without a
time-consuming if-else-tree. single bit shift + OR and we're there:

            (typeA << 2) | typeB

Always results in a value from 0-15 which can index into a function
table:

    FunctionTypedef wr_func[16] =
    {
      do_I_I,  do_I_F,  do_I_R,  do_I_E,
      do_F_I,  do_F_F,  do_F_R,  do_F_E,
      do_R_I,  do_R_F,  do_R_R,  do_R_E,
      do_E_I,  do_E_F,  do_E_R,  do_E_E,
    };

such that
wr_func[ (typeA << 2) | typeB ]( look ma, no ifs! );

In the case of "extended" types these are the ones that take extra
logic: arrays and hashes.

The "extended" types are:

(x is "don't care")

0x20xxxxxx  raw array: r->c points to a flat array, and
                       r->p2 holds the size of the array in the
                              middle 21 bits:

                              0x1FFFFF00

                              such that shifting them >>8 yields
                              the actual array size
                       This is done so the value is never garbage
                       collected, or matched to the wrong type, but
                       does limit the size of an array to 2megabytes

0x40xxxxxx  debug break: stop execution and do debugging work

0x60xxxxxx  iterator: p2 holds the element we are on
                        va holds the array to be iterated

0x80xxxxxx  reference array: refers to a particular element of an
                             array.

                             r-> points to the array we are holding an
                             element of

                             p2 holds the element in the middle 21
                             bits:

               0x1FFFFF00

               such that shifting them >>8 yields
                             the actual element

                 This is done so the value is never garbage
               collected, or matched to the wrong type, but
               does limit the size of an array to 2megabytes

0xA0xxxxxx  array/utility hash:
                        va-> holds a pointer to the actual array object (gc_object)
                        which has been allocated and is subject to
                        garbage collection

                        there are 4 kinds of "array" objects:

            SV_VALUE            0x01 array of WRValues
            SV_CHAR             0x02 array of chars (this
                                     is how strings are represented)
            SV_VOID_HASH_TABLE  0x04 hash table of void*
                                     used for internal structures
                                     (do not gc)
            SV_HASH_TABLE       0x03 hash table of WRValues
                                     (DO descend for gc)

0xC0xxxxxx  struct: This value is a constructed "struct" object with a
                   hash table of values

                   va-> refers to the container of initialized objects


0xE0xxxxxx  hash table: hash table of WRValues

                   va-> refers to the hash table object

*/

//------------------------------------------------------------------------------
#if __cplusplus <= 199711L
enum WRValueType
#else
enum WRValueType : uint8_t
#endif
{
  WR_INT = 0x00,
  WR_FLOAT = 0x01,
  WR_REF = 0x02,
  WR_EX = 0x03,
};

//------------------------------------------------------------------------------
#if __cplusplus <= 199711L
enum WRExType
#else
enum WRExType : uint8_t
#endif
{
  WR_EX_NONE = 0x00,  // 0000

  WR_EX_RAW_ARRAY = 0x20,  // 0010

  // EX types have one of the upper two bits set
  WR_EX_DEBUG_BREAK = 0x40,  // 0100

  WR_EX_ITERATOR = 0x60,      // 0110
  WR_EX_ARRAY_MEMBER = 0x80,  // 1000
  WR_EX_ARRAY = 0xA0,         // 1010

  // see EXPECTS_HASH_INDEX!!
  WR_EX_STRUCT = 0xC0,      // 1100
  WR_EX_HASH_TABLE = 0xE0,  // 1110
};

//------------------------------------------------------------------------------
enum WRGCObjectType {
  SV_VALUE = 0x01,            // 0001
  SV_CHAR = 0x02,             // 0010
  SV_HASH_TABLE = 0x03,       // 0011
  SV_VOID_HASH_TABLE = 0x04,  // 0100 // used for wrench internals, not meant for VM to care about
};

#define EX_TYPE_MASK 0xE0
#define IS_DEBUG_BREAK(X) ((X) == WR_EX_DEBUG_BREAK)
#define IS_ARRAY_MEMBER(X) (((X) & EX_TYPE_MASK) == WR_EX_ARRAY_MEMBER)
#define IS_ARRAY(X) ((X) == WR_EX_ARRAY)
#define IS_ITERATOR(X) ((X) == WR_EX_ITERATOR)
#define IS_RAW_ARRAY(X) (((X) & EX_TYPE_MASK) == WR_EX_RAW_ARRAY)
#define IS_HASH_TABLE(X) ((X) == WR_EX_HASH_TABLE)

#define EXPECTS_HASH_INDEX(X) (((X) == WR_EX_STRUCT) || ((X) == WR_EX_HASH_TABLE))

#if !defined(ARDUINO) && \
    (__arm__ || WIN32 || _WIN32 || __linux__ || __MINGW32__ || __APPLE__ || __MINGW64__ || __clang__ || __GNUC__)
#include <memory.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <cstring>
#include <cstdlib>
#endif

#ifdef ARDUINO
#include <Arduino.h>
#ifdef BYTE_ORDER
#if BYTE_ORDER == LITTLE_ENDIAN
#define ARDUINO_LITTLE_ENDIAN
#elif BYTE_ORDER == BIG_ENDIAN
#define ARDUINO_BIG_ENDIAN
#else
#error
#endif

#else

#define TEST_LITTLE_ENDIAN \
  (((union { \
     unsigned x; \
     unsigned char c; \
   }){1}) \
       .c)
#ifdef TEST_LITTLE_ENDIAN
#define ARDUINO_LITTLE_ENDIAN
#else
#define ARDUINO_BIG_ENDIAN
#endif

#endif
#endif

#if defined(__BYTE_ORDER) && __BYTE_ORDER == __BIG_ENDIAN || defined(__BIG_ENDIAN__) || defined(__ARMEB__) || \
    defined(__THUMBEB__) || defined(__AARCH64EB__) || defined(_MIBSEB) || defined(__MIBSEB) || defined(__MIBSEB__) || \
    defined(ARDUINO_BIG_ENDIAN)
#define WRENCH_BIG_ENDIAN
#elif defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN || defined(__LITTLE_ENDIAN__) || defined(__ARMEL__) || \
    defined(_M_PPC) || defined(__THUMBEL__) || defined(__AARCH64EL__) || defined(_MSC_VER) || defined(_MIPSEL) || \
    defined(__MIPSEL) || defined(__MIPSEL__) || defined(ARDUINO_LITTLE_ENDIAN)
#define WRENCH_LITTLE_ENDIAN
#elif defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define WRENCH_LITTLE_ENDIAN
#elif defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define WRENCH_BIG_ENDIAN

#else

// if you are reading this, please define the appropriate endian-ness
// here for your compiler(WRENCH_LITTLE_ENDIAN / WRENCH_BIG_ENDIAN) and:
#error "Endian-ness not detected! Please contact curt.hartung@gmail.com so it can be added"

#endif

class WRGCObject;

//------------------------------------------------------------------------------
struct WRValue {
  // never reference the data members directly, they are unions and
  // bad things will happen. Always access them with one of these
  // methods
  int asInt() const;
  void setInt(const int val);

  float asFloat() const;
  void setFloat(const float val);

  bool isFloat() const { return type == WR_FLOAT || (type == WR_REF && r->type == WR_FLOAT); }
  bool isInt() const { return type == WR_INT || (type == WR_REF && r->type == WR_INT); }
  bool isWrenchArray() const { return IS_ARRAY(xtype); }
  bool isRawArray() const { return IS_RAW_ARRAY(xtype); }
  bool isHashTable() const { return IS_HASH_TABLE(xtype); }

  // if this value is an array, return [or create] the 'index'-th element
  // if create is true and this value is NOT an array, it will be converted into one
  WRValue *indexArray(WRContext *context, const uint32_t index, const bool create);

  // if this value is a hash table, return [or create] the 'index' hash item
  // if create is true and this value is NOT a hash, it will be converted into one
  WRValue *indexHash(WRContext *context, const uint32_t hash, const bool create);

  // string: must point to a buffer long enough to contain at least maxLen bytes.
  // the "string" pointer will be passed back, if maxLen is 0 (not
  // reccomended) the string is assumed to be unlimited size
  char *asString(char *string, size_t maxLen = 0) const;

  // return a raw pointer to the data array if this is one, otherwise null
  void *array(unsigned int *len = 0, char arrayType = SV_CHAR) const;

  inline void init() {
    p = 0;
    p2 = 0;
  }  // call upon first create or when you're sure no memory is hanging from one

  // private: // is what this SHOULD be.. but that's impractical since the
  //  VM is not an object that can be friended.

  WRValue &singleValue() const;  // return a single value for comparison
  WRValue &deref() const;        // if this value is a ARRAY_MEMBER, copy the referred value in

  uint32_t getHashEx() const;  // harder
  uint32_t getHash() const     // for easy cases
  {
    if (type <= WR_FLOAT) {
      return ui;
    }

    return getHashEx();
  }

  union  // first 4 bytes (or 8 on a 64-bit system but.. what are you doing here?)
  {
    int32_t i;
    uint32_t ui;
    WRValue *frame;
    float f;
    const void *p;
    void *r1;
    char *c;
    WRValue *r;
    WRGCObject *va;
    WR_C_CALLBACK ccb;
    WR_LIB_CALLBACK lcb;
    WRFunction *wrf;
  };

  union {
#ifdef WRENCH_LITTLE_ENDIAN
    struct {
#if (__cplusplus <= 199711L)
      uint8_t type;  // carries the type
      uint8_t padL;  // pad carries an array pointer (up to 2megs using bottom 4 bits of xtype)
      uint8_t padH;
      uint8_t xtype;  // carries the extra type (if it exists)
#else
      WRValueType type;  // carries the type
      uint8_t padL;      // pad carries an array pointer (up to 2megs using bottom 4 bits of xtype)
      uint8_t padH;
      WRExType xtype;  // carries the extra type (if it exists)
#endif
    };
#else  // WRENCH_BIG_ENDIAN
    struct {
#if (__cplusplus <= 199711L)
      uint8_t xtype;  // carries the extra type (if it exists)
      uint8_t padH;
      uint8_t padL;  // pad carries an array pointer (up to 2megs using bottom 4 bits of xtype)
      uint8_t type;  // carries the type
#else
      WRExType xtype;  // carries the extra type (if it exists)
      uint8_t padH;
      uint8_t padL;      // pad carries an array pointer (up to 2megs using bottom 4 bits of xtype)
      WRValueType type;  // carries the type
#endif
    };
#endif

    uint32_t p2;
    union {
      int returnOffset;
      WRValue *r2;
      void *usr;
    };
  };

  inline WRValue &operator=(const WRValue &V) {
    r1 = V.r1;
    r2 = V.r2;
    return *this;
  }
};

//------------------------------------------------------------------------------
// Helper class to represent a wrench value, in all cases it does NOT
// managerthe memory, but relies on a WRContext to do that
class WrenchValue {
 public:
  WrenchValue(WRContext *context, const char *label) : m_context(context), m_value(wr_getGlobalRef(context, label)) {}
  WrenchValue(WRContext *context, WRValue *value) : m_context(context), m_value(value) {}

  bool isValid() const { return m_value ? true : false; }

  // if the value is not the correct type it will be converted to
  // that type, preserving the value as best it can
  operator int32_t *() { return asInt(); }
  int32_t *asInt() { return (m_value->type == WR_INT) ? &(m_value->i) : makeInt(); }

  operator float *() { return asFloat(); }
  float *asFloat() { return (m_value->type == WR_FLOAT) ? &(m_value->f) : makeFloat(); }

  // this will convert it to an array if it isn't one
  WRValue &operator[](const int index) { return *asArrayMember(index); }
  WRValue *asArrayMember(const int index);
  int arraySize();  // returns -1 if this is not an array

  // convert the value in-place and return a pointer to it
  int32_t *makeInt();
  float *makeFloat();

 private:
  WRContext *m_context;
  WRValue *m_value;
};

struct WrenchPacket;
struct WrenchSymbol;
struct WrenchFunction;

template<class> class SimpleLL;

//------------------------------------------------------------------------------
class WRDebugServerInterface {
 public:
  WRDebugServerInterface(WRState *w, const unsigned char *bytes = 0, const int len = 0,
                         bool (*receiveFunction)(char *data, const size_t length, const int timeoutMilliseconds) = 0,
                         size_t (*writeFunction)(const char *data, const size_t length) = 0);

  ~WRDebugServerInterface();

  WRContext *loadBytes(const unsigned char *bytes, const int len);
  char *m_localBytes;
  int m_localBytesLen;

  // private: again.. TECHNICALLY.. but we are keeping things simple

  int m_steppingOverReturnVector;
  int m_lineSteps;
  int m_opcodeSteps;

  struct LineBreak {
    int line;
    LineBreak *next;
  };
  SimpleLL<LineBreak> *m_lineBreaks;
  SimpleLL<WrenchPacket> *m_packetQ;

  void codewordEncountered(uint16_t codeword, WRValue *stackTop);

  void processPacket(WrenchPacket *packet);

  WRState *m_w;
  bool (*m_receiveFunction)(char *data, const size_t length, const int timeoutMilliseconds);
  size_t (*m_writeFunction)(const char *data, const size_t length);

  int m_onLine;
  int m_onFunction;
  bool m_firstCall;

  const unsigned char *m_embeddedSource;
  int m_embeddedSourceSize;
  int m_embeddedSourceHash;

  const unsigned char *m_symbolBlock;
  int m_symbolBlockSize;

  WRContext *m_context;
  const WRValue *m_argv;
  int m_argn;
  const unsigned char *m_pc;
  WRValue *m_frameBase;
  WRValue *m_stackTop;

  bool m_brk;
};

//------------------------------------------------------------------------------
class WRDebugClientInterface {
 public:
  WRDebugClientInterface(size_t (*receiveFunction)(char *data, const size_t length, const int timeoutMilliseconds),
                         size_t (*writeFunction)(const char *data, const size_t length));

  WRDebugClientInterface(WRDebugServerInterface *localServer);

  ~WRDebugClientInterface();

  //	const WRDebugMessage& status();

  // load and prepare a program for running
  // if byteCode/size is null then it re-loads it's existing program
  void load(const char *byteCode = 0, const int size = 0);
  void run();  // run or continue

  bool getSourceCode(const char **data, int *len);
  uint32_t getSourceCodeHash();

  const WRValue *getGlobalValue(const char *name);
  const char *getGlobalName(const int index);
  void setGlobalValue(const char *name, WRValue const &value);

  const WRValue *getLocalValue(const char *name);
  const char *getLocalName(const int index);
  void setLocalValue(const char *name, WRValue const &value);

  void stepOverLine(const int steps = 1);
  void stepOverOpcode(const int steps = 1);

  void stepIntoLine(const int steps = 1);
  void stepIntoOpcode(const int steps = 1);

  void setBreakpoint(const int lineNumber);
  void clearBreakpoint(const int lineNumber);

  // private:

  bool transmit(WrenchPacket &packet);
  WrenchPacket *receive(const int timeoutMilliseconds = 0);

  void loadSourceBlock();
  void loadSymbols();

  void init();

  char *m_sourceBlock;
  int m_sourceBlockLen;
  uint32_t m_sourceBlockHash;
  WrenchPacket *m_packet;

  SimpleLL<WrenchPacket> *m_packetQ;

  void populateSymbols(const char *block, const int size);
  SimpleLL<WrenchSymbol> *m_globals;
  SimpleLL<WrenchFunction> *m_functions;
  bool m_symbolsLoaded;

  WRDebugServerInterface *m_localServer;
  size_t (*m_receiveFunction)(char *data, const size_t length, const int timeoutMilliseconds);
  size_t (*m_writeFunction)(const char *data, const size_t length);
};

#ifdef WRENCH_REALLY_COMPACT
#ifndef WRENCH_COMPACT
#define WRENCH_COMPACT
#endif
#endif

#define WRENCH_JUMPTABLE_INTERPRETER
#ifdef WRENCH_REALLY_COMPACT
#undef WRENCH_JUMPTABLE_INTERPRETER
#elif !defined(__clang__)
#if _MSC_VER
#undef WRENCH_JUMPTABLE_INTERPRETER
#endif
#endif

#ifndef WRENCH_COMBINED
#include "utils.h"
#include "simple_ll.h"
#include "gc_object.h"
#include "serializer.h"
#include "vm.h"
#include "opcode.h"
#include "str.h"
#include "opcode_stream.h"
#include "wrench_debug.h"
#include "cc.h"
#include "std_io_defs.h"
#endif

#endif
