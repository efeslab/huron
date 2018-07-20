#ifndef __AE_FUNCHOOK_H_
#define __AE_FUNCHOOK_H_

typedef void (*func_t)(void);

// For hooking functions
void hook_function(func_t func, func_t hook, func_t* orig_ptr);
void hook_function_by_name(const char* lib, const char* name, func_t hook, func_t* result);

#endif // __AE_FUNCHOOK_H_
