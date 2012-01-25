/*
 * Copyright 2011-2012 the Redfish authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define __USE_GNU

#include "util/compiler.h"
#include "util/platform/signal.h"

#include <inttypes.h>
#include <stdio.h>
#include <sys/ucontext.h>
#include <unistd.h>

#if CMAKE_SYSTEM_PROCESSOR == x86
#if __WORDSIZE == 64
/* x86_64 version */
void signal_analyze_plat_data(const void *data, char *out, size_t out_len)
{
	ucontext_t *ctx;
	void *rip, *rbp, *rsp;

	ctx = (ucontext_t*)data;
	rip = (void*)ctx->uc_mcontext.gregs[REG_RIP];
	rbp = (void*)ctx->uc_mcontext.gregs[REG_RBP];
	rsp = (void*)ctx->uc_mcontext.gregs[REG_RSP];
	snprintf(out, out_len, "RIP:%p, RBP:%p, RSP:%p\n", rip, rbp, rsp);
}
#else
/* 32-bit x86 version */
void signal_analyze_plat_data(const void *data, char *out, size_t out_len)
{
	ucontext_t *ctx;
	void *eip, *ebp, *esp;

	ctx = (ucontext_t*)data;
	eip = (void*)ctx->uc_mcontext.gregs[REG_EIP];
	ebp = (void*)ctx->uc_mcontext.gregs[REG_EBP];
	esp = (void*)ctx->uc_mcontext.gregs[REG_ESP];
	snprintf(out, out_len, "EIP:%p, EBP:%p, ESP:%p\n", eip, ebp, esp);
}
#endif
#else
/* TODO: Someone should probably implement this for ARM */
void signal_analyze_plat_data(POSSIBLY_UNUSED(const void *data),
		char *out, size_t out_len)
{
	snprintf(out, out_len, "No additional platform data\n");
}
#endif
