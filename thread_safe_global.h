/*
 * Copyright (c) 2015 Cryptonector LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THREAD_SAFE_VAR_H
#define THREAD_SAFE_VAR_H

#include <sys/types.h>
#include <stdint.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * A thread_safe_var is an object that holds a pointer to what would
 * typically be configuration information.  Thus a thread_safe_var is
 * a thread-safe "global variable".  Threads can safely read and write
 * this via thread_safe_var_get() and thread_safe_var_set(), with
 * configuration values destroyed only when the last reference is
 * released.  Applications should store mostly read-only values in a
 * thread_safe_var -- typically configuration information, the sort of
 * data that rarely changes.
 *
 * Writes are serialized.  Readers don't block and do not spin, and
 * mostly perform only fast atomic operations; the only blocking
 * operations done by readers are for uncontended resources.
 */
typedef struct thread_safe_var_s *thread_safe_var;

typedef void (*thread_safe_var_dtor_f)(void *);

int  thread_safe_var_init(thread_safe_var *, thread_safe_var_dtor_f);
void thread_safe_var_destroy(thread_safe_var);

int  thread_safe_var_get(thread_safe_var, void **, uint64_t *);
int  thread_safe_var_wait(thread_safe_var);
int  thread_safe_var_set(thread_safe_var, void *, uint64_t *);
void thread_safe_var_release(thread_safe_var);

#ifdef __cplusplus
}
#endif

#endif /* THREAD_SAFE_VAR_H */
