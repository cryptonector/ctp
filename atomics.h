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
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef ATOMICS_H
#define ATOMICS_H

#ifdef HAVE_ILLUMOS_ATOMICS
#include <atomics.h>
#else
#include <stdint.h>
uint32_t atomic_inc_32_nv(volatile uint32_t *);
uint32_t atomic_dec_32_nv(volatile uint32_t *);

uint64_t atomic_inc_64_nv(volatile uint64_t *);
uint64_t atomic_dec_64_nv(volatile uint64_t *);

void *atomic_cas_ptr(volatile void **, void *, void *);
uint32_t atomic_cas_32(volatile uint32_t *, uint32_t, uint32_t);
uint64_t atomic_cas_64(volatile uint64_t *, uint64_t, uint64_t);

void *atomic_read_ptr(volatile void **);
uint32_t atomic_read_32(volatile uint32_t *);
uint64_t atomic_read_64(volatile uint32_t *);

void atomic_write_ptr(volatile void **, void *);
void atomic_write_32(volatile uint32_t *, uint32_t);
void atomic_write_64(volatile uint64_t *, uint64_t);
#endif

#endif /* ATOMICS_H */
