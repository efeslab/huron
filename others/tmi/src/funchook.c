#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <sys/user.h>
#include <sys/mman.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <malloc.h>
#include "tramps.h"
#include "funchook.h"
#include "tmi.h"

#define MAX_HOOK_SIZE PAGE_SIZE

static void* allocate_hook_page(void* func)
{
    Dl_info dli = {0};
    char* base;
    if(!dladdr(func, &dli))
        return NULL;

    base = (char*)dli.dli_fbase;
    do
    {
        base -= PAGE_SIZE;
        if((((char*)dli.dli_fbase) - base) >= INT_MAX)
            die("Could not allocate trampline!\n");
    }
    while(mmap(base, MAX_HOOK_SIZE, PROT_EXEC | PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0) == MAP_FAILED);
    
    memset(base, 0, MAX_HOOK_SIZE);

    return base;
        
}

static void copy_code(unsigned char** dst, const void* max_dst, const unsigned char** src, int mode, int size)
{
	int *id;

   MUST((void*)((*dst) + size) < max_dst);

	switch (mode)
	{
	case 0:
		*dst += size;
		*src += size;
		return;
	case 1:
		memcpy(*dst, *src, size);
		*dst += size;
		*src += size;
		return;
	case 2:
		memcpy(*dst, *src, size);
		id = (int*)&((*dst)[2]);
		*id += (*src - *dst);
		*dst += size;
		*src += size;
		break;
	}
}

static int get_disp(unsigned char rm, int rex64, int* copymode)
{
	if ((rm >> 6) == 3)
		return 0;
	int size = 0;
	if ((rm & 7) == 4)
		size += 1; // SIB
	switch (rm >> 6)
	{
	case 0:
		if ((rm & 7) == 5)
		{
			size += 4;
			if (rex64)
				*copymode = 2;
		}
		break;
	case 1: // disp8
		size += 1;
		break;
	case 2: // disp32
		size += 4;
		break;
	}
	return size;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
static int decode(const void* orig, void* dst, void* max_dst)
{
	const unsigned char* s = (const unsigned char*)orig;
	unsigned char* d = (unsigned char*)dst;
	int reset = 1;
	int rex64 = 0;
	int map = 0;
	int size = 1;
	int copymode = 1;
	int quit = 0;
	int fsovr = 0;
	int rep = 0;
    int maxofs = -1;

	for (;!quit && (size > 0); copy_code(&d, max_dst, &s, copymode, size))
	{
		unsigned char b = *s;
		if (reset)
		{
			rex64 = 0;
			map = 0;
			size = 0;
			fsovr = 0;
			rep = 0;
			copymode = 1;
		}
		reset = 1;

		if ((b >> 4) == 4) // REX
		{
			rex64 = (b & 8) ? 1 : 0;
			reset = 0;
			size = 1;
			continue;
		}
		if (b == 0xf) // change map
		{
			map = 1;
			reset = 0;
			size = 1;
			continue;
		}
		if (b == 0xf3) // rep
		{
			rep = 1;
			reset = 0;
			size = 1;
			continue;
		}
		if (b == 0x64) // FS segment override
		{
			fsovr = 1;
			reset = 0;
			size = 1;
			continue;
		}

		switch (map)
		{
		case 0:
			switch (b)
			{
			case 0x39: // cmp r
				//size = 1 + get_disp(s[1], rex64, &copymode);
				size = 2;
				break;

			case 0x3d: // cmp
			case 0xb8: // mov
			case 0xb9: // mov
				size = 5;
				break;
			case 0x31: // xor r
				size = 2 + get_disp(s[1], rex64, &copymode);
				break;
			case 0x83: // or 8-bit
				size = 3 + get_disp(s[1], rex64, &copymode);
				break;
			case 0x8b: // mov
				size = 2 + get_disp(s[1], rex64, &copymode);
                if (((s[1] & 7) == 4) && s[2] == 0x25)
                   size += 4; // HACK!!!

				break;
			case 0x85:
				size = 2 + get_disp(s[1], rex64, &copymode);
				break;
			case 0xc7:  // mov
				size = 6 + get_disp(s[1], 0 /* hack */, &copymode);
				break;
			case 0x89: // mov r/m
				size = 2 + get_disp(s[1], rex64, &copymode);
				break;
			case 0xf7: // multiple ALU
				size = 2;
				break;
			case 0xc3:
				size = 1;
				quit = 1;
                maxofs = (d - (unsigned char*)dst) + 1;
				break;
			default:
				if ((b >= 0x70 && b <= 0x7f) || b == 0xe3) // jmp short
				{
					memcpy(d, s, 2); // special case for jmps
					const unsigned char* news = s + 2 + (int)(char)s[1];
					unsigned char* newd = d + 2 + (int)(char)s[1];
					if (*newd == 0) {
                        int tofs;
						int t = decode(news, newd, max_dst);
                        if(t < 0)
                            return t;
                        tofs = (t + newd - (unsigned char*)dst);
                        if(tofs > maxofs)
                            maxofs = tofs;
                    }
					size = 2;
				}
				else
				{
                    printf("Unknown instruction %i in map 0.\n", b);
					return -1;
				}
			}
			break;
		case 1:
			switch (b)
			{
			case 0x05: // syscall
				size = 1;
				break;
            default:
                printf("Unknown instruction %i in map 1.\n", b);
                return -1;
			}
			break;

        default:
            printf("Unknown instruction map: %i\n", map);
            return -1;
		}
	}
	return maxofs;
}
#pragma GCC diagnostic pop

void hook_function(func_t func, func_t hook, func_t* orig_ptr)
{
    char* data;
    int size;
    uintptr_t funcbase, funcptr;
    JUMP_TRAMPOLINE* tramp;

    data = (char*)allocate_hook_page(func);
    MUST(data != NULL);

    size = decode(func, data, data + MAX_HOOK_SIZE);
    MUST(size > 0 && size < MAX_HOOK_SIZE);

    funcptr = (uintptr_t)func;
    funcbase = funcptr & ~(PAGE_SIZE - 1);
    MUST(0 == mprotect((void*)funcbase, funcptr + sizeof(JUMP_TRAMPOLINE) - funcbase, PROT_WRITE | PROT_READ | PROT_EXEC));

    tramp = (JUMP_TRAMPOLINE*)funcptr;
    memcpy(tramp, &BASE_JUMP_TRAMPOLINE, sizeof(BASE_JUMP_TRAMPOLINE));
    tramp->target_address = hook;

    *orig_ptr = (func_t)data;
}

void hook_function_by_name(const char* lib, const char* name, func_t hook, func_t* orig_ptr)
{
    void* sym;
    void* dl = dlopen(lib, RTLD_NOW | RTLD_NOLOAD);
    MUST(dl != NULL);

    sym = dlsym(dl, name);
    MUST(sym != NULL);

    hook_function(sym, hook, orig_ptr);
}

