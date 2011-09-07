/*
 * Copyright (C) 2010 gonzoj
 *
 * Please check the CREDITS file for further information.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "winapi.h"
#include "wtypes.h"
#include "wutil.h"

/* supported libraries */

static char *kernel32 = "KERNEL32.dll";

static char *user32 = "USER32.dll";

/* supported KERNEL32.dll functions */

bool __attribute__((stdcall)) IsValidCodePage(unsigned cpage) {
	return FALSE;
}

void * __attribute__((stdcall)) GetStdHandle(dword std_handle) {
	return NULL;
}

void __attribute__((stdcall)) Sleep(dword ms) {
	usleep(ms * 1000);
}

bool __attribute__((stdcall)) TlsFree(dword index) {
	return FALSE;
}

void * __attribute__((stdcall)) TlsGetValue(dword index) {
	return NULL;
}

bool __attribute__((stdcall)) TlsSetValue(dword index, void *val) {
	return FALSE;
}

void __attribute__((stdcall)) RaiseException(dword code, dword flags, dword nargs, void *args) {
	code = code;
	return;
}

dword __attribute__((stdcall)) TlsAlloc() {
	return 42;
}

void * __attribute__((stdcall)) AddVectoredExceptionHandler(bool first, void *handler) {
	return NULL;
}

bool * __attribute__((stdcall)) RemoveVectoredExceptionHandler(void *handler) {
	return FALSE;
}

void * __attribute__((stdcall)) GetProcAddress(void *module, char *proc) {
	return winapi_get_proc(module, proc);
}

void * __attribute__((stdcall)) GetModuleHandleA(char *module) {
	if (!strcmp(strtoupper(module), kernel32)) {
		return kernel32;
	} else if (!strcmp(strtoupper(module), user32)) {
		return user32;
	} else {
		logmessage("error: module called GetModuleHandleA for module %s which is unsupported\n", module);
		return NULL;
	}
}

dword __attribute__((stdcall)) GetTickCount() {
	return 42;
}

bool __attribute__((stdcall)) GetVersionExA(void *ver_info) {
	return FALSE;
}

void __attribute__((stdcall)) GetSystemInfo(void *sys_info) {
	sys_info = sys_info;
	return;
}

dword __attribute__((stdcall)) QueryDosDeviceA(char *name, char *path, dword ucch_max) {
	return 0;
}

size_t __attribute__((stdcall)) VirtualQuery(void *addr, void *buf, size_t len) {
	return 0;
}

bool __attribute__((stdcall)) CloseHandle(void *handle) {
	return FALSE;
}

void * __attribute__((stdcall)) GetCurrentProcess() {
	return (void *) -1;
}

bool __attribute__((stdcall))  FreeLibrary(void *module) {
	return FALSE;
}

bool __attribute__((stdcall))  DuplicateHandle(void *src_phandle, void *src_handle, void *dest_phandle, void *dest_handle, dword access, bool inherit, dword options) {
	return FALSE;
}

void * __attribute__((stdcall)) LoadLibraryA(char *file) {
	if (!strcmp(strtoupper(file), kernel32)) {
		return kernel32;
	} else if (!strcmp(strtoupper(file), user32)) {
		return user32;
	} else {
		logmessage("error: module called LoadLibraryA for file %s which is unsupported\n", file);
		return NULL;
	};
}

void * __attribute__((stdcall)) GetProcessHeap() {
	return NULL;
}

bool __attribute__((stdcall)) HeapFree(void *heap, dword flags, void *mem) {
	return FALSE;
}

bool __attribute__((stdcall)) TerminateProcess(void *p, int exit_code) {
	return FALSE;
}

dword __attribute__((stdcall)) UnhandledExceptionFilter(void *excp_info) {
	return 0;
}

void * __attribute__((stdcall)) SetUnhandledExceptionFilter(void *filter) {
	return NULL;
}

bool __attribute__((stdcall)) QueryPerformanceCounter(void *perform_count) {
	return FALSE;
}

dword __attribute__((stdcall)) GetCurrentThreadId() {
	return 0;
}

dword __attribute__((stdcall)) GetCurrentProcessId() {
	return 0;
}

void __attribute__((stdcall)) GetSystemTimeAsFileTime(void *sys_time) {
	sys_time = sys_time;
	return;
}

void __attribute__((stdcall)) RtlUnwind(void *frame, void *ip, void *excp, int ret) {
	frame = frame;
	return;
}

/* supported USER32.dll functions */

dword __attribute__((stdcall)) CharUpperBuffA(char *buf, dword size) {
	return 0;
}

void * __attribute__((stdcall)) BeginPaint(void *hwnd, void *paint) {
	return NULL;
}

/* WINAPI emulation */

typedef struct {
	char *symbol;
	void *func;
} win_proc;

#define n_libkernel32 32

win_proc libkernel32[] = {
		{ "IsValidCodePage", IsValidCodePage },
		{ "GetStdHandle", GetStdHandle },
		{ "Sleep", Sleep },
		{ "TlsFree", TlsFree },
		{ "TlsGetValue", TlsGetValue },
		{ "TlsSetValue", TlsSetValue },
		{ "RaiseException", RaiseException },
		{ "TlsAlloc", TlsAlloc },
		{ "GetProcAddress", GetProcAddress },
		{ "GetModuleHandleA", GetModuleHandleA },
		{ "GetTickCount", GetTickCount },
		{ "GetVersionExA", GetVersionExA },
		{ "GetSystemInfo", GetSystemInfo },
		{ "QueryDosDeviceA", QueryDosDeviceA },
		{ "VirtualQuery", VirtualQuery },
		{ "CloseHandle", CloseHandle },
		{ "GetCurrentProcess", GetCurrentProcess },
		{ "FreeLibrary", FreeLibrary },
		{ "DuplicateHandle", DuplicateHandle },
		{ "LoadLibraryA", LoadLibraryA },
		{ "GetProcessHeap", GetProcessHeap },
		{ "HeapFree", HeapFree },
		{ "TerminateProcess", TerminateProcess },
		{ "UnhandledExceptionFilter", UnhandledExceptionFilter },
		{ "SetUnhandledExceptionFilter", SetUnhandledExceptionFilter },
		{ "QueryPerformanceCounter", QueryPerformanceCounter },
		{ "GetCurrentThreadId", GetCurrentThreadId },
		{ "GetCurrentProcessId", GetCurrentProcessId },
		{ "GetSystemTimeAsFileTime", GetSystemTimeAsFileTime },
		{ "RtlUnwind", RtlUnwind },
		{ "AddVectoredExceptionHandler", AddVectoredExceptionHandler },
		{ "RemoveVectoredExceptionHandler", RemoveVectoredExceptionHandler }
};

#define n_libuser32 2

win_proc libuser32[] = {
		{ "CharUpperBuffA", CharUpperBuffA },
		{ "BeginPaint", BeginPaint }
};

/*
 *  experimental
 *
 *  Warden modules depend on various WINAPI functions. Due to the stdcall calling convention
 *  we the callee need to clean up the stack (arguments that were pushed). Right now I've implemented
 *  the functions the latest generation of modules depends on. However, if a module should call a function
 *  which we haven't implemented, the stack will get trashed. The idea of the function beneath is, to
 *  detect the number of arguments pushed and to align the stack accordingly. Haven't finished this, since
 *  this is not as trivial as it might sound. :(
 */

/*
#include <libdis.h>

void __attribute__((stdcall)) align_stack() {
	unsigned ret, push = 0;
	asm("movl 0x4(%%ebp), %0" : "=r" (ret));
	printf("return address is 0x%08X\n", ret);

	char line[0x100];
	int pos = 0;
	int size;
	x86_insn_t insn;
	x86_init(opt_none, NULL, NULL);
	while ( pos < 10 ) {
		size = x86_disasm((unsigned char *) ret - 10, 0x100, 0, pos, &insn);
		if ( size ) {
			x86_format_insn(&insn, line, 0x100, intel_syntax);
			printf("%s\n", line);
			pos += size;
			if (strstr(line, "push")) {
				push++;
			}
			if (strstr(line, "call")) {
				break;
			}
		} else {
			printf("Invalid instruction\n");
			pos++;
		}
	}
	x86_cleanup();

	printf("%i push\n", push);

	switch (push) {
	case 1:
		asm("leave; ret $0x4;");
	case 2:
		asm("leave; ret $0x8;");
	case 3:
		asm("leave; ret $0xC;");
	case 4:
		asm("leave; ret $0x10;");
	case 5:
		asm("leave; ret $0x14;");
	case 6:
		asm("leave; ret $0x18;");
	case 7:
		asm("leave; ret $0x1C;");
	case 8:
		asm("leave; ret $0x20;");
	}
}
*/

/* for now, we pass this function if we haven't got a implementation for the requested function */
void __attribute__((stdcall)) winapi_dummy(void) {
	return; /* if there are arguments, the stack will get fucked */
}

void * winapi_get_proc(char *lib, char *func) {
	win_proc *l = NULL;
	int size = 0;

	if (!lib) {
		logmessage("error: module requested proc from unsupported library\n", func);
		return winapi_dummy;
	} else if (!strcmp(lib, "KERNEL32.dll")) {
		l = libkernel32;
		size = n_libkernel32;
	} else if (!strcmp(lib, "USER32.dll")) {
		l = libuser32;
		size = n_libuser32;
	} else {
		logmessage("error: module requested proc %s from unsupported library %s\n", func, lib);
		return winapi_dummy;
	}

	int i;
	for (i = 0; i < size; i++) {
		if (!strcmp(l[i].symbol, func)) {
			return l[i].func;
		}
	}
	logmessage("error: module requested unsupported proc %s from library %s\n", func, lib);
	return winapi_dummy;
}
