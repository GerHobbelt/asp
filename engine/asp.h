/*
 * Asp engine API definitions.
 *
 * Copyright (c) 2024 Canadensys Aerospace Corporation.
 * See LICENSE.txt at https://bitbucket.org/asplang/asp for details.
 */

#ifndef ASP_080177a8_14ce_11ed_b65f_7328ac4c64a3_H
#define ASP_080177a8_14ce_11ed_b65f_7328ac4c64a3_H

#include <asp-api.h>
#include <asp-ver.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#ifdef ASP_DEBUG
#include <stdio.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Result returned from AspAddCode, AspSeal, AspSealCode, and AspPageCode. */
typedef enum
{
    AspAddCodeResult_OK = 0x00,
    AspAddCodeResult_InvalidFormat = 0x01,
    AspAddCodeResult_InvalidVersion = 0x02,
    AspAddCodeResult_InvalidCheckValue = 0x03,
    AspAddCodeResult_OutOfCodeMemory = 0x04,
    AspAddCodeResult_InvalidState = 0x08,
} AspAddCodeResult;

/* Result returned from AspInitialize, AspReset, and AspStep, among others. */
typedef enum
{
    AspRunResult_OK = 0x00,
    AspRunResult_Complete = 0x01,
    AspRunResult_InitializationError = 0x02,
    AspRunResult_InvalidState = 0x03,
    AspRunResult_InvalidInstruction = 0x04,
    AspRunResult_InvalidEnd = 0x05,
    AspRunResult_BeyondEndOfCode = 0x06,
    AspRunResult_StackUnderflow = 0x07,
    AspRunResult_CycleDetected = 0x08,
    AspRunResult_InvalidContext = 0x0A,
    AspRunResult_Redundant = 0x0B,
    AspRunResult_UnexpectedType = 0x0C,
    AspRunResult_SequenceMismatch = 0x0D,
    AspRunResult_StringFormattingError = 0x0E,
    AspRunResult_InvalidFormatString = 0x0F,
    AspRunResult_NameNotFound = 0x10,
    AspRunResult_KeyNotFound = 0x11,
    AspRunResult_ValueOutOfRange = 0x12,
    AspRunResult_IteratorAtEnd = 0x13,
    AspRunResult_MalformedFunctionCall = 0x14,
    AspRunResult_UndefinedAppFunction = 0x15,
    AspRunResult_InvalidAppFunction = 0x16,
    AspRunResult_DivideByZero = 0x18,
    AspRunResult_ArithmeticOverflow = 0x19,
    AspRunResult_OutOfDataMemory = 0x20,
    AspRunResult_Again = 0xFA,
    AspRunResult_Abort = 0xFB,
    AspRunResult_Call = 0xFC,
    AspRunResult_InternalError = 0xFE,
    AspRunResult_NotImplemented = 0xFF,
    AspRunResult_Application = 0x100,
    AspRunResult_Max = INT32_MAX,
} AspRunResult;

/* Floating-point translator type. */
typedef double (*AspFloatConverter)(uint8_t ieee754_binary64[8]);

/* Code reader type. */
typedef AspRunResult (*AspCodeReader)
    (void *id, uint32_t offset, size_t *size, void *codePage);

#ifdef __cplusplus
}
#endif

/* Internal types. */
#include <asp-priv.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialization. */
ASP_API size_t AspDataEntrySize(void);
ASP_API void AspEngineVersion(uint8_t version[4]);
ASP_API AspRunResult AspInitialize
    (AspEngine *, void *code, size_t codeSize, void *data, size_t dataSize,
     const AspAppSpec *, void *context);
ASP_API AspRunResult AspInitializeEx
    (AspEngine *, void *code, size_t codeSize, void *data, size_t dataSize,
     const AspAppSpec *, void *context, AspFloatConverter);
ASP_API AspRunResult AspSetCodePaging
    (AspEngine *, uint8_t pageCount, size_t pageSize, AspCodeReader);
ASP_API void AspCodeVersion(const AspEngine *, uint8_t version[4]);
ASP_API size_t AspMaxCodeSize(const AspEngine *);
ASP_API size_t AspMaxDataSize(const AspEngine *);
ASP_API AspAddCodeResult AspAddCode
    (AspEngine *, const void *code, size_t codeSize);
ASP_API AspAddCodeResult AspSeal(AspEngine *);
ASP_API AspAddCodeResult AspSealCode
    (AspEngine *, const void *code, size_t codeSize);
ASP_API AspAddCodeResult AspPageCode(AspEngine *, void *id);
ASP_API AspRunResult AspReset(AspEngine *);
ASP_API AspRunResult AspSetArguments(AspEngine *, const char * const *);
ASP_API AspRunResult AspSetArgumentsString(AspEngine *, const char *);
ASP_API AspRunResult AspSetCycleDetectionLimit(AspEngine *, uint32_t);
ASP_API uint32_t AspGetCycleDetectionLimit(const AspEngine *);

/* Execution control. */
ASP_API AspRunResult AspRestart(AspEngine *);
ASP_API AspRunResult AspStep(AspEngine *);
ASP_API bool AspIsReady(const AspEngine *);
ASP_API bool AspIsRunning(const AspEngine *);
ASP_API bool AspIsRunnable(const AspEngine *);
ASP_API size_t AspProgramCounter(const AspEngine *);
ASP_API size_t AspLowFreeCount(const AspEngine *);
ASP_API size_t AspCodePageReadCount(AspEngine *, bool reset);
#ifdef ASP_DEBUG
ASP_API uint32_t AspDataAddress(const AspEngine *, const AspDataEntry *);
ASP_API uint32_t AspUseCount(const AspDataEntry *);
ASP_API void AspTraceFile(AspEngine *, FILE *);
ASP_API void AspDump(const AspEngine *, FILE *);
#endif

/* API for use by application functions. */
ASP_API bool AspIsNone(const AspDataEntry *);
ASP_API bool AspIsEllipsis(const AspDataEntry *);
ASP_API bool AspIsBoolean(const AspDataEntry *);
ASP_API bool AspIsInteger(const AspDataEntry *);
ASP_API bool AspIsFloat(const AspDataEntry *);
ASP_API bool AspIsIntegral(const AspDataEntry *);
ASP_API bool AspIsNumber(const AspDataEntry *);
ASP_API bool AspIsNumeric(const AspDataEntry *);
ASP_API bool AspIsSymbol(const AspDataEntry *);
ASP_API bool AspIsRange(const AspDataEntry *);
ASP_API bool AspIsString(const AspDataEntry *);
ASP_API bool AspIsTuple(const AspDataEntry *);
ASP_API bool AspIsList(const AspDataEntry *);
ASP_API bool AspIsSequence(const AspDataEntry *);
ASP_API bool AspIsSet(const AspDataEntry *);
ASP_API bool AspIsDictionary(const AspDataEntry *);
ASP_API bool AspIsForwardIterator(const AspDataEntry *);
ASP_API bool AspIsReverseIterator(const AspDataEntry *);
ASP_API bool AspIsIterator(const AspDataEntry *);
ASP_API bool AspIsIterable(const AspDataEntry *);
ASP_API bool AspIsFunction(const AspDataEntry *);
ASP_API bool AspIsModule(const AspDataEntry *);
ASP_API bool AspIsAppIntegerObject(const AspDataEntry *);
ASP_API bool AspIsAppPointerObject(const AspDataEntry *);
ASP_API bool AspIsAppObject(const AspDataEntry *);
ASP_API bool AspIsType(const AspDataEntry *);
ASP_API bool AspIsTrue(AspEngine *, const AspDataEntry *);
ASP_API bool AspIntegerValue(const AspDataEntry *, int32_t *);
ASP_API bool AspFloatValue(const AspDataEntry *, double *);
ASP_API bool AspSymbolValue(const AspDataEntry *, int32_t *);
ASP_API bool AspRangeValues
    (AspEngine *, const AspDataEntry *,
     int32_t *start, int32_t *end, int32_t *step, bool *bounded);
ASP_API bool AspStringValue
    (AspEngine *, const AspDataEntry *,
     size_t *size, char *buffer, size_t index, size_t bufferSize);
ASP_API AspDataEntry *AspToString(AspEngine *, AspDataEntry *);
ASP_API AspDataEntry *AspToRepr(AspEngine *, const AspDataEntry *);
ASP_API AspRunResult AspCount
    (AspEngine *, const AspDataEntry *, int32_t *count);
ASP_API AspDataEntry *AspElement
    (AspEngine *, const AspDataEntry *sequence, int32_t index);
ASP_API int32_t AspRangeElement
    (AspEngine *, const AspDataEntry *range, int32_t index);
ASP_API char AspStringElement
    (AspEngine *, const AspDataEntry *str, int32_t index);
ASP_API AspDataEntry *AspFind
    (AspEngine *, const AspDataEntry *tree, const AspDataEntry *key);
ASP_API AspDataEntry *AspAt(AspEngine *, const AspDataEntry *iterator);
ASP_API bool AspAtSame
    (AspEngine *,
     const AspDataEntry *iterator1, const AspDataEntry *iterator2);
ASP_API AspDataEntry *AspNext(AspEngine *, AspDataEntry *iterator);
ASP_API AspDataEntry *AspIterable(AspEngine *, const AspDataEntry *iterator);
ASP_API bool AspAppObjectTypeValue
    (AspEngine *, const AspDataEntry *, int16_t *);
ASP_API bool AspAppIntegerObjectValues
    (AspEngine *, const AspDataEntry *, int16_t *appType, int32_t *value);
ASP_API bool AspAppPointerObjectValues
    (AspEngine *, const AspDataEntry *, int16_t *appType, void **value);
ASP_API AspDataEntry *AspNewNone(AspEngine *);
ASP_API AspDataEntry *AspNewEllipsis(AspEngine *);
ASP_API AspDataEntry *AspNewBoolean(AspEngine *, bool);
ASP_API AspDataEntry *AspNewInteger(AspEngine *, int32_t);
ASP_API AspDataEntry *AspNewFloat(AspEngine *, double);
ASP_API AspDataEntry *AspNewSymbol(AspEngine *, int32_t);
ASP_API AspDataEntry *AspNewRange
    (AspEngine *, int32_t start, int32_t end, int32_t step);
ASP_API AspDataEntry *AspNewUnboundedRange
    (AspEngine *, int32_t start, int32_t step);
ASP_API AspDataEntry *AspNewString
    (AspEngine *, const char *buffer, size_t bufferSize);
ASP_API AspDataEntry *AspNewTuple(AspEngine *);
ASP_API AspDataEntry *AspNewList(AspEngine *);
ASP_API AspDataEntry *AspNewSet(AspEngine *);
ASP_API AspDataEntry *AspNewDictionary(AspEngine *);
ASP_API AspDataEntry *AspNewIterator
    (AspEngine *, AspDataEntry *iterable, bool reversed);
ASP_API AspDataEntry *AspNewAppIntegerObject
    (AspEngine *, int16_t appType, int32_t value,
     void (*destructor)(AspEngine *, int16_t appType, int32_t value));
ASP_API AspDataEntry *AspNewAppPointerObject
    (AspEngine *, int16_t appType, void *value,
     void (*destructor)(AspEngine *, int16_t appType, void *value));
ASP_API AspDataEntry *AspNewType(AspEngine *, const AspDataEntry *);
ASP_API bool AspTupleAppend
    (AspEngine *, AspDataEntry *tuple, AspDataEntry *value, bool take);
ASP_API bool AspListAppend
    (AspEngine *, AspDataEntry *list, AspDataEntry *value, bool take);
ASP_API bool AspListInsert
    (AspEngine *, AspDataEntry *list,
     int32_t index, AspDataEntry *value, bool take);
ASP_API bool AspListErase(AspEngine *, AspDataEntry *list, int32_t index);
ASP_API bool AspInsertAt
    (AspEngine *, AspDataEntry *iterator, AspDataEntry *value, bool take);
ASP_API bool AspEraseAt(AspEngine *, AspDataEntry *iterator);
ASP_API bool AspStringAppend
    (AspEngine *, AspDataEntry *str,
     const char *buffer, size_t bufferSize);
ASP_API bool AspSetInsert
    (AspEngine *, AspDataEntry *set, AspDataEntry *key, bool take);
ASP_API bool AspSetErase
    (AspEngine *, AspDataEntry *set, const AspDataEntry *key);
ASP_API bool AspDictionaryInsert
    (AspEngine *, AspDataEntry *dictionary,
     AspDataEntry *key, AspDataEntry *value, bool take);
ASP_API bool AspDictionaryErase
    (AspEngine *, AspDataEntry *dictionary, const AspDataEntry *key);
ASP_API bool AspAddPositionalArgument
    (AspEngine *, AspDataEntry *value, bool take);
ASP_API bool AspAddNamedArgument
    (AspEngine *, int32_t symbol, AspDataEntry *value, bool take);
ASP_API bool AspAddIterableGroupArgument
    (AspEngine *, AspDataEntry *value, bool take);
ASP_API bool AspAddDictionaryGroupArgument
    (AspEngine *, AspDataEntry *value, bool take);
ASP_API void AspClearFunctionArguments(AspEngine *);
ASP_API AspRunResult AspCall(AspEngine *, AspDataEntry *function);
ASP_API AspRunResult AspReturnValue(AspEngine *, AspDataEntry **);
ASP_API int32_t AspNextSymbol(AspEngine *);
ASP_API AspDataEntry *AspLoadLocal(AspEngine *, int32_t symbol);
ASP_API bool AspStoreLocal
    (AspEngine *, int32_t symbol, AspDataEntry *value, bool take);
ASP_API bool AspEraseLocal(AspEngine *, int32_t symbol);
ASP_API void AspRef(AspEngine *, AspDataEntry *);
ASP_API void AspUnref(AspEngine *, AspDataEntry *);
ASP_API AspDataEntry *AspArguments(AspEngine *);
ASP_API void *AspContext(const AspEngine *);
ASP_API bool AspAgain(const AspEngine *);
ASP_API AspRunResult AspAssert(AspEngine *, bool);

#ifdef __cplusplus
}
#endif

#endif
