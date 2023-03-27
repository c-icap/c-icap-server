 /*
 *  Copyright (C) 2004-2021 Christos Tsantilas
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA  02110-1301  USA.
 */

#ifndef __C_ICAP_ATOMIC_H
#define __C_ICAP_ATOMIC_H

#include "c-icap.h"
#include "ci_threads.h"

/*
  The user of this header file can use the following defines:
 - CI_ATOMICS_USE_128BIT: To allow use the 128bit numbers with the c-icap atomics
   interface (the ci_atomic_*_?128 functions). The compilers can use 128bit
   integers are the gcc and clang.
   Example use:
   #define CI_ATOMICS_USE_128BIT
   #include "c-icap/atomic.h"
 - CI_ATOMICS_INLINE: Use the inline version of the c-icap atomics
   functions.
   Example use:
   #define CI_ATOMICS_INLINE
   #include "c-icap/atomic.h"
*/

/*
   The use of atomics in in-lined functions is problematic: The user
   may use a compiler which is not supporting atomics operations or use
   a C++ compiler which is not compatible with C11 atomics.
   This implementation tries to provide a limited solution.

   The goal of this interface is NOT to provide compatible atomic
   operations for various C and C++ compilers. The goal is to provide
   a limited but fast interface for use from c-icap modules/services
   to access c-icap global counters or statistic counters.
   The c-icap and the services/modules internally may use the C atomic
   or c++ std::atomic interfaces they prefer.
*/

/**
 \defgroup ATOMICS Simple atomic operations for c-icap
 \ingroup API
 \brief The c-icap atomics operations are currently implemented for
 * 32bit and 64bit unsigned and signed integers. To use the functions
 * bellow replace with the correct TYPELABEL and TYPE.\n
 * The _gl suffixed functions are for use with inter-process atomic
 * objects, where the non-suffixed functions are for use inside the
 * same process.
 \details
 * Currently the following integer types are supported:
 *   - for unsigned 32bit integers use TYPE="uint32_t" and TYPELABEL= "u32"
 *   - for signed 32bit integers TYPE="int32_t" and TYPELABEL= "i32"
 *   - for unsigned 64bit integers TYPE="uint64_t" and TYPELABEL= "u64"
 *   - for signed 64bit integers TYPE="int64_t" and TYPELABEL= "i64"
 *
 * The c-icap atomic operations are implemented using C11 atomics.
 * If the C11 atomics are not supported, then a pthread mutex or an
 * inter-process mutex (for the _gl suffixed functions - normally a
 * posix or a sysv mutex) is used to lock the atomic object before
 * an update or read operation.
 *
 */
#if defined(__CI_DOXYGEN__)
/**
 * Reads atomically the value from an atomic of type TYPE.
 \ingroup ATOMICS
 */
void ci_atomic_load_TYPELABEL(_CI_ATOMIC_TYPE const TYPE *counter, TYPE *value);

/**
 * Sets atomically a value to an atomic of type TYPE.
 \ingroup ATOMICS
 */
void ci_atomic_store_TYPELABEL(_CI_ATOMIC_TYPE TYPE *counter, TYPE value);

/**
 * Adds atomically an amount "add" to the atomic "counter" of type TYPE.
 \ingroup ATOMICS
 */
void ci_atomic_add_TYPELABEL(_CI_ATOMIC_TYPE TYPE *counter, TYPE add);

/**
 * Subtracts atomically an amount "sub" from the atomic "counter" of type TYPE.
 \ingroup ATOMICS
 */
void ci_atomic_sub_TYPELABEL(_CI_ATOMIC_TYPE TYPE *counter, TYPE sub);

/**
 * Adds atomically an amount "add" to the atomic "counter" of type TYPE
 \return the value of atomic "counter" before the atomic operation
 \ingroup ATOMICS
 */
TYPE ci_atomic_fetch_add_TYPELABEL(_CI_ATOMIC_TYPE TYPE *counter, TYPE add);

/**
 * Subtracts atomically an amount "sub" from the atomic "counter" of type TYPE.
 \return the value of atomic "counter" before the atomic operation
 \ingroup ATOMICS
 */
TYPE ci_atomic_fetch_sub_TYPELABEL(_CI_ATOMIC_TYPE TYPE *counter, TYPE sub);

/**
 * Reads atomically the value from an inter-process atomic variable, of type TYPE.
 \ingroup ATOMICS
 */
void ci_atomic_load_TYPELABEL_gl(_CI_ATOMIC_TYPE const TYPE *counter, TYPE *value);

/**
 * Sets atomically a value to an inter-process atomic variable, of type TYPE.
 \ingroup ATOMICS
 */
void ci_atomic_store_TYPELABEL_gl(_CI_ATOMIC_TYPE TYPE *counter, TYPE value);

/**
 * Adds atomically an amount "add" to the inter-process atomic variable "counter", of type TYPE.
 \ingroup ATOMICS
 */
void ci_atomic_add_TYPELABEL_gl(_CI_ATOMIC_TYPE TYPE *counter, TYPE add);

/**
 * Subtracts atomically an amount "sub" from the inter-process atomic variable "counter", of type TYPE.
 \ingroup ATOMICS
 */
void ci_atomic_sub_TYPELABEL_gl(_CI_ATOMIC_TYPE TYPE *counter, TYPE sub);

/**
 * Adds atomically an amount "add" to the inter-process atomic "counter" of type TYPE
 \return the value of atomic "counter" before the atomic operation
 \ingroup ATOMICS
 */
TYPE ci_atomic_fetch_add_TYPELABEL_gl(_CI_ATOMIC_TYPE TYPE *counter, TYPE add);

/**
 * Subtracts atomically an amount "sub" from the inter-process atomic "counter" of type TYPE.
 \return the value of atomic "counter" before the atomic operation
 \ingroup ATOMICS
 */
TYPE ci_atomic_fetch_sub_TYPELABEL_gl(_CI_ATOMIC_TYPE TYPE *counter, TYPE sub);

#endif


#if defined(CI_ATOMICS_INLINE)
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_ATOMICS__)
#define __CI_INLINE_POSIX_ATOMICS 1
#include <stdatomic.h>
#include <inttypes.h>

#elif defined( __cplusplus)

# if defined (_MSVC_LANG)

#    if defined(_M_X64) && _MSVC_LANG >= 201103L
#       define __CI_INLINE_CPLUSPLUS_ATOMICS 1
#    endif

#elif __cplusplus >= 201103L // Not _MSC_VER

#include <atomic>
//__x86_64 defined by clang++, g++ and intel C++ compilers.
#if defined(__x86_64)
#  define __CI_INLINE_CPLUSPLUS_ATOMICS 1
#endif /* defined(__x86_64) */
#endif /* __cplusplus >= 201103L */
#endif /* defined(__cplusplus) */
#endif /* defined(CI_ATOMICS_INLINE) */

#ifdef __cplusplus
extern "C"
{
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_ATOMICS__)
#define _CI_ATOMIC_TYPE _Atomic
#else
#define _CI_ATOMIC_TYPE
#endif

CI_DECLARE_FUNC(int) ci_atomics_init();

/* Non-inline function calls implementation */
CI_DECLARE_FUNC(void) ci_atomic_load_u32_non_inline(_CI_ATOMIC_TYPE const uint32_t *counter, uint32_t *value);
CI_DECLARE_FUNC(void) ci_atomic_store_u32_non_inline(_CI_ATOMIC_TYPE uint32_t *counter, uint32_t value);
CI_DECLARE_FUNC(void) ci_atomic_add_u32_non_inline(_CI_ATOMIC_TYPE uint32_t *counter, uint32_t add);
CI_DECLARE_FUNC(void) ci_atomic_sub_u32_non_inline(_CI_ATOMIC_TYPE uint32_t *counter, uint32_t sub);
CI_DECLARE_FUNC(uint32_t) ci_atomic_fetch_add_u32_non_inline(_CI_ATOMIC_TYPE uint32_t *counter, uint32_t add);
CI_DECLARE_FUNC(uint32_t) ci_atomic_fetch_sub_u32_non_inline(_CI_ATOMIC_TYPE uint32_t *counter, uint32_t sub);
CI_DECLARE_FUNC(void) ci_atomic_load_u32_non_inline_gl(_CI_ATOMIC_TYPE const uint32_t *counter, uint32_t *value);
CI_DECLARE_FUNC(void) ci_atomic_store_u32_non_inline_gl(_CI_ATOMIC_TYPE uint32_t *counter, uint32_t value);
CI_DECLARE_FUNC(void) ci_atomic_add_u32_non_inline_gl(_CI_ATOMIC_TYPE uint32_t *counter, uint32_t add);
CI_DECLARE_FUNC(void) ci_atomic_sub_u32_non_inline_gl(_CI_ATOMIC_TYPE uint32_t *counter, uint32_t sub);
CI_DECLARE_FUNC(uint32_t) ci_atomic_fetch_add_u32_non_inline_gl(_CI_ATOMIC_TYPE uint32_t *counter, uint32_t add);
CI_DECLARE_FUNC(uint32_t) ci_atomic_fetch_sub_u32_non_inline_gl(_CI_ATOMIC_TYPE uint32_t *counter, uint32_t sub);

CI_DECLARE_FUNC(void) ci_atomic_load_i32_non_inline(_CI_ATOMIC_TYPE const int32_t *counter, int32_t *value);
CI_DECLARE_FUNC(void) ci_atomic_store_i32_non_inline(_CI_ATOMIC_TYPE int32_t *counter, int32_t value);
CI_DECLARE_FUNC(void) ci_atomic_add_i32_non_inline(_CI_ATOMIC_TYPE int32_t *counter, int32_t add);
CI_DECLARE_FUNC(void) ci_atomic_sub_i32_non_inline(_CI_ATOMIC_TYPE int32_t *counter, int32_t sub);
CI_DECLARE_FUNC(int32_t) ci_atomic_fetch_add_i32_non_inline(_CI_ATOMIC_TYPE int32_t *counter, int32_t add);
CI_DECLARE_FUNC(int32_t) ci_atomic_fetch_sub_i32_non_inline(_CI_ATOMIC_TYPE int32_t *counter, int32_t sub);
CI_DECLARE_FUNC(void) ci_atomic_load_i32_non_inline_gl(_CI_ATOMIC_TYPE const int32_t *counter, int32_t *value);
CI_DECLARE_FUNC(void) ci_atomic_store_i32_non_inline_gl(_CI_ATOMIC_TYPE int32_t *counter, int32_t value);
CI_DECLARE_FUNC(void) ci_atomic_add_i32_non_inline_gl(_CI_ATOMIC_TYPE int32_t *counter, int32_t add);
CI_DECLARE_FUNC(void) ci_atomic_sub_i32_non_inline_gl(_CI_ATOMIC_TYPE int32_t *counter, int32_t sub);
CI_DECLARE_FUNC(int32_t) ci_atomic_fetch_add_i32_non_inline_gl(_CI_ATOMIC_TYPE int32_t *counter, int32_t add);
CI_DECLARE_FUNC(int32_t) ci_atomic_fetch_sub_i32_non_inline_gl(_CI_ATOMIC_TYPE int32_t *counter, int32_t sub);

CI_DECLARE_FUNC(void) ci_atomic_load_u64_non_inline(_CI_ATOMIC_TYPE const uint64_t *counter, uint64_t *value);
CI_DECLARE_FUNC(void) ci_atomic_store_u64_non_inline(_CI_ATOMIC_TYPE uint64_t *counter, uint64_t value);
CI_DECLARE_FUNC(void) ci_atomic_add_u64_non_inline(_CI_ATOMIC_TYPE uint64_t *counter, uint64_t add);
CI_DECLARE_FUNC(void) ci_atomic_sub_u64_non_inline(_CI_ATOMIC_TYPE uint64_t *counter, uint64_t sub);
CI_DECLARE_FUNC(uint64_t) ci_atomic_fetch_add_u64_non_inline(_CI_ATOMIC_TYPE uint64_t *counter, uint64_t add);
CI_DECLARE_FUNC(uint64_t) ci_atomic_fetch_sub_u64_non_inline(_CI_ATOMIC_TYPE uint64_t *counter, uint64_t sub);
CI_DECLARE_FUNC(void) ci_atomic_load_u64_non_inline_gl(_CI_ATOMIC_TYPE const uint64_t *counter, uint64_t *value);
CI_DECLARE_FUNC(void) ci_atomic_store_u64_non_inline_gl(_CI_ATOMIC_TYPE uint64_t *counter, uint64_t value);
CI_DECLARE_FUNC(void) ci_atomic_add_u64_non_inline_gl(_CI_ATOMIC_TYPE uint64_t *counter, uint64_t add);
CI_DECLARE_FUNC(void) ci_atomic_sub_u64_non_inline_gl(_CI_ATOMIC_TYPE uint64_t *counter, uint64_t sub);
CI_DECLARE_FUNC(uint64_t) ci_atomic_fetch_add_u64_non_inline_gl(_CI_ATOMIC_TYPE uint64_t *counter, uint64_t add);
CI_DECLARE_FUNC(uint64_t) ci_atomic_fetch_sub_u64_non_inline_gl(_CI_ATOMIC_TYPE uint64_t *counter, uint64_t sub);

CI_DECLARE_FUNC(void) ci_atomic_load_i64_non_inline(_CI_ATOMIC_TYPE const int64_t *counter, int64_t *value);
CI_DECLARE_FUNC(void) ci_atomic_store_i64_non_inline(_CI_ATOMIC_TYPE int64_t *counter, int64_t value);
CI_DECLARE_FUNC(void) ci_atomic_add_i64_non_inline(_CI_ATOMIC_TYPE int64_t *counter, int64_t add);
CI_DECLARE_FUNC(void) ci_atomic_sub_i64_non_inline(_CI_ATOMIC_TYPE int64_t *counter, int64_t sub);
CI_DECLARE_FUNC(int64_t) ci_atomic_fetch_add_i64_non_inline(_CI_ATOMIC_TYPE int64_t *counter, int64_t add);
CI_DECLARE_FUNC(int64_t) ci_atomic_fetch_sub_i64_non_inline(_CI_ATOMIC_TYPE int64_t *counter, int64_t sub);
CI_DECLARE_FUNC(void) ci_atomic_load_i64_non_inline_gl(_CI_ATOMIC_TYPE const int64_t *counter, int64_t *value);
CI_DECLARE_FUNC(void) ci_atomic_store_i64_non_inline_gl(_CI_ATOMIC_TYPE int64_t *counter, int64_t value);
CI_DECLARE_FUNC(void) ci_atomic_add_i64_non_inline_gl(_CI_ATOMIC_TYPE int64_t *counter, int64_t add);
CI_DECLARE_FUNC(void) ci_atomic_sub_i64_non_inline_gl(_CI_ATOMIC_TYPE int64_t *counter, int64_t sub);
CI_DECLARE_FUNC(int64_t) ci_atomic_fetch_add_i64_non_inline_gl(_CI_ATOMIC_TYPE int64_t *counter, int64_t add);
CI_DECLARE_FUNC(int64_t) ci_atomic_fetch_sub_i64_non_inline_gl(_CI_ATOMIC_TYPE int64_t *counter, int64_t sub);


#if defined(CI_ATOMICS_USE_128BIT)
CI_DECLARE_FUNC(void) ci_atomic_load_u128_non_inline(_CI_ATOMIC_TYPE const unsigned __int128 *counter, unsigned __int128 *value);
CI_DECLARE_FUNC(void) ci_atomic_store_u128_non_inline(_CI_ATOMIC_TYPE unsigned __int128 *counter, unsigned __int128 value);
CI_DECLARE_FUNC(void) ci_atomic_add_u128_non_inline(_CI_ATOMIC_TYPE unsigned __int128 *counter, unsigned __int128 add);
CI_DECLARE_FUNC(void) ci_atomic_sub_u128_non_inline(_CI_ATOMIC_TYPE unsigned __int128 *counter, unsigned __int128 sub);
CI_DECLARE_FUNC(unsigned __int128) ci_atomic_fetch_add_u128_non_inline(_CI_ATOMIC_TYPE unsigned __int128 *counter, unsigned __int128 add);
CI_DECLARE_FUNC(unsigned __int128) ci_atomic_fetch_sub_u128_non_inline(_CI_ATOMIC_TYPE unsigned __int128 *counter, unsigned __int128 sub);
CI_DECLARE_FUNC(void) ci_atomic_load_u128_non_inline_gl(_CI_ATOMIC_TYPE const unsigned __int128 *counter, unsigned __int128 *value);
CI_DECLARE_FUNC(void) ci_atomic_store_u128_non_inline_gl(_CI_ATOMIC_TYPE unsigned __int128 *counter, unsigned __int128 value);
CI_DECLARE_FUNC(void) ci_atomic_add_u128_non_inline_gl(_CI_ATOMIC_TYPE unsigned __int128 *counter, unsigned __int128 add);
CI_DECLARE_FUNC(void) ci_atomic_sub_u128_non_inline_gl(_CI_ATOMIC_TYPE unsigned __int128 *counter, unsigned __int128 sub);
CI_DECLARE_FUNC(unsigned __int128) ci_atomic_fetch_add_u128_non_inline_gl(_CI_ATOMIC_TYPE unsigned __int128 *counter, unsigned __int128 add);
CI_DECLARE_FUNC(unsigned __int128) ci_atomic_fetch_sub_u128_non_inline_gl(_CI_ATOMIC_TYPE unsigned __int128 *counter, unsigned __int128 sub);

CI_DECLARE_FUNC(void) ci_atomic_load_i128_non_inline(_CI_ATOMIC_TYPE const __int128 *counter, __int128 *value);
CI_DECLARE_FUNC(void) ci_atomic_store_i128_non_inline(_CI_ATOMIC_TYPE __int128 *counter, __int128 value);
CI_DECLARE_FUNC(void) ci_atomic_add_i128_non_inline(_CI_ATOMIC_TYPE __int128 *counter, __int128 add);
CI_DECLARE_FUNC(void) ci_atomic_sub_i128_non_inline(_CI_ATOMIC_TYPE __int128 *counter, __int128 sub);
CI_DECLARE_FUNC(__int128) ci_atomic_fetch_add_i128_non_inline(_CI_ATOMIC_TYPE __int128 *counter, __int128 add);
CI_DECLARE_FUNC(__int128) ci_atomic_fetch_sub_i128_non_inline(_CI_ATOMIC_TYPE __int128 *counter, __int128 sub);
CI_DECLARE_FUNC(void) ci_atomic_load_i128_non_inline_gl(_CI_ATOMIC_TYPE const __int128 *counter, __int128 *value);
CI_DECLARE_FUNC(void) ci_atomic_store_i128_non_inline_gl(_CI_ATOMIC_TYPE __int128 *counter, __int128 value);
CI_DECLARE_FUNC(void) ci_atomic_add_i128_non_inline_gl(_CI_ATOMIC_TYPE __int128 *counter, __int128 add);
CI_DECLARE_FUNC(void) ci_atomic_sub_i128_non_inline_gl(_CI_ATOMIC_TYPE __int128 *counter, __int128 sub);
CI_DECLARE_FUNC(__int128) ci_atomic_fetch_add_i128_non_inline_gl(_CI_ATOMIC_TYPE __int128 *counter, __int128 add);
CI_DECLARE_FUNC(__int128) ci_atomic_fetch_sub_i128_non_inline_gl(_CI_ATOMIC_TYPE __int128 *counter, __int128 sub);
#endif

#ifdef __cplusplus
}
#endif

#define __ci_implement_atomic_ops(spec, name, type)                      \
    spec void ci_atomic_load_##name(_CI_ATOMIC_TYPE const type *counter, type *store) { \
        *store = atomic_load_explicit(counter, memory_order_relaxed);   \
    }                                                                   \
    spec void ci_atomic_store_##name(_CI_ATOMIC_TYPE type *counter, type store) {       \
        atomic_store_explicit(counter, store, memory_order_relaxed);    \
    }                                                                   \
    spec void ci_atomic_add_##name(_CI_ATOMIC_TYPE type *counter, type add) {           \
        atomic_fetch_add_explicit(counter, add, memory_order_seq_cst);  \
    }                                                                   \
    spec void ci_atomic_sub_##name(_CI_ATOMIC_TYPE type *counter, type sub) {           \
        atomic_fetch_sub_explicit(counter, sub, memory_order_relaxed);  \
    }                                                                   \
    spec type ci_atomic_fetch_add_##name(_CI_ATOMIC_TYPE type *counter, type add) { \
        return atomic_fetch_add_explicit(counter, add, memory_order_seq_cst); \
    }                                                                   \
    spec type ci_atomic_fetch_sub_##name(_CI_ATOMIC_TYPE type *counter, type sub) { \
        return atomic_fetch_sub_explicit(counter, sub, memory_order_relaxed); \
    }                                                                   \
    spec void ci_atomic_load_##name ## _gl(_CI_ATOMIC_TYPE const type *counter, type *store) { \
        *store = atomic_load_explicit(counter, memory_order_relaxed);   \
    }                                                                   \
    spec void ci_atomic_store_##name ## _gl(_CI_ATOMIC_TYPE type *counter, type store) { \
        atomic_store_explicit(counter, store, memory_order_relaxed);    \
    }                                                                   \
    spec void ci_atomic_add_##name ## _gl(_CI_ATOMIC_TYPE type *counter, type add) {    \
        atomic_fetch_add_explicit(counter, add, memory_order_seq_cst);  \
    }                                                                   \
    spec void ci_atomic_sub_##name ## _gl(_CI_ATOMIC_TYPE type *counter, type sub) {    \
        atomic_fetch_sub_explicit(counter, sub, memory_order_relaxed);  \
    }                                                                   \
    spec type ci_atomic_fetch_add_##name ## _gl(_CI_ATOMIC_TYPE type *counter, type add) { \
        return atomic_fetch_add_explicit(counter, add, memory_order_seq_cst); \
    }                                                                   \
    spec type ci_atomic_fetch_sub_##name ## _gl(_CI_ATOMIC_TYPE type *counter, type sub) { \
        return atomic_fetch_sub_explicit(counter, sub, memory_order_relaxed); \
    }


#define __ci_implement_atomic_ops_non_inline(spec, name, type)          \
    spec void ci_atomic_load_##name(_CI_ATOMIC_TYPE const type *counter, type *store) { \
        ci_atomic_load_ ## name ## _non_inline(counter, store);         \
    }                                                                   \
    spec void ci_atomic_store_##name(_CI_ATOMIC_TYPE type *counter, type store) {       \
        ci_atomic_store_ ## name ## _non_inline(counter, store);         \
    }                                                                   \
    spec void ci_atomic_add_##name(_CI_ATOMIC_TYPE type *counter, type add) {           \
        ci_atomic_add_ ## name ## _non_inline(counter, add);            \
    }                                                                   \
    spec void ci_atomic_sub_##name(_CI_ATOMIC_TYPE type *counter, type sub) {           \
        ci_atomic_sub_ ## name ## _non_inline(counter, sub);            \
    }                                                                   \
    spec type ci_atomic_fetch_add_##name(_CI_ATOMIC_TYPE type *counter, type add) { \
        return ci_atomic_fetch_add_ ## name ## _non_inline(counter, add);            \
    }                                                                   \
    spec type ci_atomic_fetch_sub_##name(_CI_ATOMIC_TYPE type *counter, type sub) { \
        return ci_atomic_fetch_sub_ ## name ## _non_inline(counter, sub); \
    }                                                                   \
    spec void ci_atomic_load_##name ## _gl(_CI_ATOMIC_TYPE const type *counter, type *store) { \
        ci_atomic_load_ ## name ## _non_inline_gl(counter, store);      \
    }                                                                   \
    spec void ci_atomic_store_##name ## _gl(_CI_ATOMIC_TYPE type *counter, type store) { \
        ci_atomic_store_ ## name ## _non_inline_gl(counter, store);      \
    }                                                                   \
    spec void ci_atomic_add_##name ## _gl(_CI_ATOMIC_TYPE type *counter, type add) {    \
        ci_atomic_add_ ## name ## _non_inline_gl(counter, add);         \
    }                                                                   \
    spec void ci_atomic_sub_##name ## _gl(_CI_ATOMIC_TYPE type *counter, type sub) {    \
        ci_atomic_sub_ ## name ## _non_inline_gl(counter, sub);         \
    }                                                                   \
    spec void ci_atomic_fetch_add_##name ## _gl(_CI_ATOMIC_TYPE type *counter, type add) { \
        return ci_atomic_fetch_add_ ## name ## _non_inline_gl(counter, add);         \
    }                                                                   \
    spec void ci_atomic_fetch_sub_##name ## _gl(_CI_ATOMIC_TYPE type *counter, type sub) { \
        return ci_atomic_fetch_sub_ ## name ## _non_inline_gl(counter, sub); \
    }


#if defined(__CI_INLINE_POSIX_ATOMICS)
/*
  The above lines implement the following inlined functions:
  void  ci_atomic_load_u32(uint32_t *counter, uint32_t *value);
  void ci_atomic_add_u32(uint32_t *counter, uint32_t add);
  void ci_atomic_sub_u32(uint32_t *counter, uint32_t sub);

  void  ci_atomic_load_i32(int32_t *counter, int32_t *value);
  void ci_atomic_add_i32(int32_t *counter, int32_t add);
  void ci_atomic_sub_i32(int32_t *counter, int32_t sub);

  void  ci_atomic_load_u64(uint64_t *counter, uint64_t *value);
  void ci_atomic_add_u64(uint64_t *counter, uint64_t add);
  void ci_atomic_sub_u64(uint64_t *counter, uint64_t sub);

  void  ci_atomic_load_i64(int64_t *counter, int64_t *value);
  void ci_atomic_add_i64(int64_t *counter, int64_t add);
  void ci_atomic_sub_i64(int64_t *counter, int64_t sub);

  #if defined(CI_ATOMICS_USE_128BIT)
  void  ci_atomic_load_u128(unsigned __int128 *counter, unsigned __int128 *value);
  void  ci_atomic_add_u128(unsigned __int128 *counter, unsigned __int128 add);
  void  ci_atomic_sub_u128(unsigned __int128 *counter, unsigned __int128 sub);

  void  ci_atomic_load_i128(__int128 *counter, __int128 *value);
  void  ci_atomic_add_i128(__int128 *counter, __int128 add);
  void  ci_atomic_sub_i128(__int128 *counter, __int128 sub);
  #endif
*/
__ci_implement_atomic_ops(static inline, u32, uint32_t);
__ci_implement_atomic_ops(static inline, i32, int32_t);
__ci_implement_atomic_ops(static inline, u64, uint64_t);
__ci_implement_atomic_ops(static inline, i64, int64_t);
#if defined(CI_ATOMICS_USE_128BIT)
__ci_implement_atomic_ops(static inline, u128, unsigned __int128);
__ci_implement_atomic_ops(static inline, i128, __int128);
#endif

#elif defined(__CI_INLINE_CPLUSPLUS_ATOMICS)

#define __ci_implement_atomic_ops_cplusplus(spec, name, type)           \
    spec void ci_atomic_load_##name(const type *counter, type *store) {       \
        *store = std::atomic_load_explicit(reinterpret_cast<const std::atomic<type> *>(counter), std::memory_order_relaxed); \
    }                                                                   \
    spec void ci_atomic_store_##name(type *counter, type store) {       \
        std::atomic_store_explicit(reinterpret_cast<std::atomic<type> *>(counter), store, std::memory_order_relaxed); \
    }                                                                   \
    spec void ci_atomic_add_##name(type *counter, type add) {           \
        std::atomic_fetch_add_explicit(reinterpret_cast<std::atomic<type> *>(counter), add, std::memory_order_relaxed); \
    }                                                                   \
    spec void ci_atomic_sub_##name(type *counter, type sub) {           \
        std::atomic_fetch_sub_explicit(reinterpret_cast<std::atomic<type> *>(counter), sub, std::memory_order_relaxed); \
    }                                                                   \
    spec type ci_atomic_fetch_add_##name(type *counter, type add) {           \
        return std::atomic_fetch_add_explicit(reinterpret_cast<std::atomic<type> *>(counter), add, std::memory_order_relaxed); \
    }                                                                   \
    spec type ci_atomic_fetch_sub_##name(type *counter, type sub) {     \
        return std::atomic_fetch_sub_explicit(reinterpret_cast<std::atomic<type> *>(counter), sub, std::memory_order_relaxed); \
    }

__ci_implement_atomic_ops_cplusplus(inline, u32, uint32_t);
__ci_implement_atomic_ops_cplusplus(inline, i32, int32_t);
__ci_implement_atomic_ops_cplusplus(inline, u64, uint64_t);
__ci_implement_atomic_ops_cplusplus(inline, i64, int64_t);

#if defined(CI_ATOMICS_USE_128BIT)
__ci_implement_atomic_ops_non_inline(static inline, u128, unsigned __int128);
__ci_implement_atomic_ops_non_inline(static inline, i128, __int128);
#endif // defined(CI_ATOMICS_USE_128BIT)

#else //defined(__CI_INLINE_CPLUSPLUS_ATOMICS)

__ci_implement_atomic_ops_non_inline(static inline, u32, uint32_t);
__ci_implement_atomic_ops_non_inline(static inline, i32, int32_t);
__ci_implement_atomic_ops_non_inline(static inline, u64, uint64_t);
__ci_implement_atomic_ops_non_inline(static inline, i64, int64_t);
#if defined(CI_ATOMICS_USE_128BIT)
__ci_implement_atomic_ops_non_inline(static inline, u128, unsigned __int128);
__ci_implement_atomic_ops_non_inline(static inline, i128, __int128);
#endif

#endif

#endif /* __C_ICAP_ATOMIC_H */
