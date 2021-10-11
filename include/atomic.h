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

CI_DECLARE_FUNC(int) ci_atomics_init();

/* Non-inline function calls implementation */
CI_DECLARE_FUNC(void) ci_atomic_load_u32_non_inline(const uint32_t *counter, uint32_t *value);
CI_DECLARE_FUNC(void) ci_atomic_store_u32_non_inline(uint32_t *counter, uint32_t value);
CI_DECLARE_FUNC(void) ci_atomic_add_u32_non_inline(uint32_t *counter, uint32_t add);
CI_DECLARE_FUNC(void) ci_atomic_sub_u32_non_inline(uint32_t *counter, uint32_t sub);
CI_DECLARE_FUNC(void) ci_atomic_load_u32_non_inline_gl(const uint32_t *counter, uint32_t *value);
CI_DECLARE_FUNC(void) ci_atomic_store_u32_non_inline_gl(uint32_t *counter, uint32_t value);
CI_DECLARE_FUNC(void) ci_atomic_add_u32_non_inline_gl(uint32_t *counter, uint32_t add);
CI_DECLARE_FUNC(void) ci_atomic_sub_u32_non_inline_gl(uint32_t *counter, uint32_t sub);

CI_DECLARE_FUNC(void) ci_atomic_load_i32_non_inline(const int32_t *counter, int32_t *value);
CI_DECLARE_FUNC(void) ci_atomic_store_i32_non_inline(int32_t *counter, int32_t value);
CI_DECLARE_FUNC(void) ci_atomic_add_i32_non_inline(int32_t *counter, int32_t add);
CI_DECLARE_FUNC(void) ci_atomic_sub_i32_non_inline(int32_t *counter, int32_t sub);
CI_DECLARE_FUNC(void) ci_atomic_load_i32_non_inline_gl(const int32_t *counter, int32_t *value);
CI_DECLARE_FUNC(void) ci_atomic_store_i32_non_inline_gl(int32_t *counter, int32_t value);
CI_DECLARE_FUNC(void) ci_atomic_add_i32_non_inline_gl(int32_t *counter, int32_t add);
CI_DECLARE_FUNC(void) ci_atomic_sub_i32_non_inline_gl(int32_t *counter, int32_t sub);

CI_DECLARE_FUNC(void) ci_atomic_load_u64_non_inline(const uint64_t *counter, uint64_t *value);
CI_DECLARE_FUNC(void) ci_atomic_store_u64_non_inline(uint64_t *counter, uint64_t value);
CI_DECLARE_FUNC(void) ci_atomic_add_u64_non_inline(uint64_t *counter, uint64_t add);
CI_DECLARE_FUNC(void) ci_atomic_sub_u64_non_inline(uint64_t *counter, uint64_t sub);
CI_DECLARE_FUNC(void) ci_atomic_load_u64_non_inline_gl(const uint64_t *counter, uint64_t *value);
CI_DECLARE_FUNC(void) ci_atomic_store_u64_non_inline_gl(uint64_t *counter, uint64_t value);
CI_DECLARE_FUNC(void) ci_atomic_add_u64_non_inline_gl(uint64_t *counter, uint64_t add);
CI_DECLARE_FUNC(void) ci_atomic_sub_u64_non_inline_gl(uint64_t *counter, uint64_t sub);

CI_DECLARE_FUNC(void) ci_atomic_load_i64_non_inline(const int64_t *counter, int64_t *value);
CI_DECLARE_FUNC(void) ci_atomic_store_i64_non_inline(int64_t *counter, int64_t value);
CI_DECLARE_FUNC(void) ci_atomic_add_i64_non_inline(int64_t *counter, int64_t add);
CI_DECLARE_FUNC(void) ci_atomic_sub_i64_non_inline(int64_t *counter, int64_t sub);
CI_DECLARE_FUNC(void) ci_atomic_load_i64_non_inline_gl(const int64_t *counter, int64_t *value);
CI_DECLARE_FUNC(void) ci_atomic_store_i64_non_inline_gl(int64_t *counter, int64_t value);
CI_DECLARE_FUNC(void) ci_atomic_add_i64_non_inline_gl(int64_t *counter, int64_t add);
CI_DECLARE_FUNC(void) ci_atomic_sub_i64_non_inline_gl(int64_t *counter, int64_t sub);

#if defined(CI_ATOMICS_USE_128BIT)
CI_DECLARE_FUNC(void) ci_atomic_load_u128_non_inline(const unsigned __int128 *counter, unsigned __int128 *value);
CI_DECLARE_FUNC(void) ci_atomic_store_u128_non_inline(unsigned __int128 *counter, unsigned __int128 value);
CI_DECLARE_FUNC(void) ci_atomic_add_u128_non_inline(unsigned __int128 *counter, unsigned __int128 add);
CI_DECLARE_FUNC(void) ci_atomic_sub_u128_non_inline(unsigned __int128 *counter, unsigned __int128 sub);
CI_DECLARE_FUNC(void) ci_atomic_load_u128_non_inline_gl(const unsigned __int128 *counter, unsigned __int128 *value);
CI_DECLARE_FUNC(void) ci_atomic_store_u128_non_inline_gl(unsigned __int128 *counter, unsigned __int128 value);
CI_DECLARE_FUNC(void) ci_atomic_add_u128_non_inline_gl(unsigned __int128 *counter, unsigned __int128 add);
CI_DECLARE_FUNC(void) ci_atomic_sub_u128_non_inline_gl(unsigned __int128 *counter, unsigned __int128 sub);

CI_DECLARE_FUNC(void) ci_atomic_load_i128_non_inline(const __int128 *counter, __int128 *value);
CI_DECLARE_FUNC(void) ci_atomic_store_i128_non_inline(__int128 *counter, __int128 value);
CI_DECLARE_FUNC(void) ci_atomic_add_i128_non_inline(__int128 *counter, __int128 add);
CI_DECLARE_FUNC(void) ci_atomic_sub_i128_non_inline(__int128 *counter, __int128 sub);
CI_DECLARE_FUNC(void) ci_atomic_load_i128_non_inline_gl(const __int128 *counter, __int128 *value);
CI_DECLARE_FUNC(void) ci_atomic_store_i128_non_inline_gl(__int128 *counter, __int128 value);
CI_DECLARE_FUNC(void) ci_atomic_add_i128_non_inline_gl(__int128 *counter, __int128 add);
CI_DECLARE_FUNC(void) ci_atomic_sub_i128_non_inline_gl(__int128 *counter, __int128 sub);
#endif

#ifdef __cplusplus
}
#endif

#define __ci_implement_atomic_ops(spec, name, type)                      \
    spec void ci_atomic_load_##name(const type *counter, type *store) { \
        *store = atomic_load_explicit(counter, memory_order_relaxed);   \
    }                                                                   \
    spec void ci_atomic_store_##name(type *counter, type store) {       \
        atomic_store_explicit(counter, store, memory_order_relaxed);    \
    }                                                                   \
    spec void ci_atomic_add_##name(type *counter, type add) {           \
        atomic_fetch_add_explicit(counter, add, memory_order_seq_cst);  \
    }                                                                   \
    spec void ci_atomic_sub_##name(type *counter, type sub) {           \
        atomic_fetch_sub_explicit(counter, sub, memory_order_relaxed);  \
    }                                                                   \
    spec void ci_atomic_load_##name ## _gl(const type *counter, type *store) { \
        *store = atomic_load_explicit(counter, memory_order_relaxed);   \
    }                                                                   \
    spec void ci_atomic_store_##name ## _gl(type *counter, type store) { \
        atomic_store_explicit(counter, store, memory_order_relaxed);    \
    }                                                                   \
    spec void ci_atomic_add_##name ## _gl(type *counter, type add) {    \
        atomic_fetch_add_explicit(counter, add, memory_order_seq_cst);  \
    }                                                                   \
    spec void ci_atomic_sub_##name ## _gl(type *counter, type sub) {    \
        atomic_fetch_sub_explicit(counter, sub, memory_order_relaxed);  \
    }

#define __ci_implement_atomic_ops_non_inline(spec, name, type)          \
    spec void ci_atomic_load_##name(const type *counter, type *store) { \
        ci_atomic_load_ ## name ## _non_inline(counter, store);         \
    }                                                                   \
    spec void ci_atomic_store_##name(type *counter, type store) {       \
        ci_atomic_load_ ## name ## _non_inline(counter, store);         \
    }                                                                   \
    spec void ci_atomic_add_##name(type *counter, type add) {           \
        ci_atomic_add_ ## name ## _non_inline(counter, add);            \
    }                                                                   \
    spec void ci_atomic_sub_##name(type *counter, type sub) {           \
        ci_atomic_sub_ ## name ## _non_inline(counter, sub);            \
    }                                                                   \
    spec void ci_atomic_load_##name ## _gl(const type *counter, type *store) { \
        ci_atomic_load_ ## name ## _non_inline_gl(counter, store);      \
    }                                                                   \
    spec void ci_atomic_store_##name ## _gl(type *counter, type store) { \
        ci_atomic_load_ ## name ## _non_inline_gl(counter, store);      \
    }                                                                   \
    spec void ci_atomic_add_##name ## _gl(type *counter, type add) {    \
        ci_atomic_add_ ## name ## _non_inline_gl(counter, add);         \
    }                                                                   \
    spec void ci_atomic_sub_##name ## _gl(type *counter, type sub) {    \
        ci_atomic_sub_ ## name ## _non_inline_gl(counter, sub);         \
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
