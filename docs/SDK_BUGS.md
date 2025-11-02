# M66 SDK Known Issues & Workarounds
## Critical Bugs in Quectel M66 OpenCPU SDK

**Author**: Hossein Gholami  
**Date**: 2025-11-02  
**SDK Version**: M66_QuecOpen_GS3_SDK_V2.6

---

## 🔴 Critical: Ql_vsnprintf Completely Broken

### Severity
**CRITICAL** - Affects ALL firmware using formatted output with va_list

### Description
The `Ql_vsnprintf` function has a fundamental implementation bug that causes it to write **zero bytes** to the output buffer when used with `va_list` parameters. This makes it completely unusable for any variadic wrapper functions.

### Symptoms
```c
// Code that looks correct but fails:
va_list args;
va_start(args, format);
Ql_vsnprintf(buffer, 256, "Test: %d", args);
va_end(args);

// Result: buffer remains empty, length = 0
// Output appears as garbled characters: +⸮\!⸮:⸮O<⸮
```

### Root Cause
- ABI mismatch in `va_list` handling
- Likely ARM EABI calling convention issue
- Function writes 0 bytes to buffer (confirmed via debug)
- Returns without error, silently failing

### Affected Functions
- ❌ `Ql_vsnprintf` - **BROKEN**
- ❌ Any variadic wrappers using `Ql_vsnprintf` - **BROKEN**
- ✅ `Ql_sprintf` - **WORKS CORRECTLY**
- ✅ `APP_DEBUG` (uses `Ql_sprintf` internally) - **WORKS**

### Workaround ✅

**DO NOT use variadic wrappers with Ql_vsnprintf:**

```c
// ❌ BROKEN - Don't use this pattern
void my_printf(const char* format, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, format);
    Ql_vsnprintf(buffer, sizeof(buffer), format, args);  // Writes 0 bytes!
    va_end(args);
    APP_DEBUG("%s", buffer);  // Prints garbage
}
```

**✅ CORRECT - Use Ql_sprintf directly:**

```c
// ✅ WORKING - Use this pattern instead
void my_printf_wrapper(const char* str)
{
    APP_DEBUG("%s", str);  // Just pass pre-formatted string
}

// Caller formats before calling:
char buffer[256];
Ql_sprintf(buffer, "Test: %d", value);  // Works perfectly!
my_printf_wrapper(buffer);
```

### Implementation Example

See `custom/com/com.c` for production implementation:

```c
/* Global buffer for formatted responses */
static char g_response_buffer[256];
static u32 g_response_mutex = 0;

/* Simple wrapper - no variadic parameters */
static void send_response(const char* str)
{
    if (g_response_mutex != 0) {
        Ql_OS_TakeMutex(g_response_mutex);
    }
    
    APP_DEBUG("%s", str);  // Direct output
    
    if (g_response_mutex != 0) {
        Ql_OS_GiveMutex(g_response_mutex);
    }
}

/* Callers format first, then send */
void cmd_set_parameter(const char* key, s32 value)
{
    Ql_sprintf(g_response_buffer, "OK: %s = %d\r\n", key, value);
    send_response(g_response_buffer);
}
```

### Testing

To verify `Ql_sprintf` works but `Ql_vsnprintf` doesn't:

```c
/* Test 1: Ql_sprintf (works) */
char buffer1[256];
Ql_sprintf(buffer1, "Test: %d", 42);
APP_DEBUG("Result: %s\r\n", buffer1);  // ✅ Output: "Test: 42"

/* Test 2: Ql_vsnprintf (broken) */
void test_vsnprintf(const char* format, ...)
{
    char buffer2[256];
    va_list args;
    va_start(args, format);
    Ql_vsnprintf(buffer2, sizeof(buffer2), format, args);
    va_end(args);
    APP_DEBUG("Result: %s\r\n", buffer2);  // ❌ Output: "" (empty) or garbage
}
test_vsnprintf("Test: %d", 42);
```

### Impact Assessment

This bug affects:
- ✅ **Fixed in our codebase** - `custom/com/` module
- ⚠️ **Potential issue** - Any other M66 firmware using variadic formatting
- ⚠️ **Community impact** - All M66 OpenCPU developers should be aware

### Reported To
- [ ] Quectel Technical Support
- [x] Documented in this project

### Status
**RESOLVED** in our implementation by avoiding `Ql_vsnprintf` entirely.

---

## 🟡 Other Known Issues

### Missing strtok Implementation

**Symptom:** Linker error `undefined reference to 'Ql_strtok'`

**Impact:** Medium

**Workaround:** Implement custom `simple_strtok` function

**Implementation:**
```c
static char* simple_strtok(char* str, char delim)
{
    static char* next_token = NULL;
    char* token_start;
    
    if (str != NULL) {
        next_token = str;
    }
    
    if (next_token == NULL || *next_token == '\0') {
        return NULL;
    }
    
    while (*next_token == delim) {
        next_token++;
    }
    
    if (*next_token == '\0') {
        return NULL;
    }
    
    token_start = next_token;
    
    while (*next_token != '\0' && *next_token != delim) {
        next_token++;
    }
    
    if (*next_token != '\0') {
        *next_token = '\0';
        next_token++;
    }
    
    return token_start;
}
```

**Status:** Workaround implemented in `custom/com/com.c`

---

## 📋 SDK Version Information

```
SDK: M66_QuecOpen_GS3_SDK_V2.6
Compiler: arm- none-eabi-gcc 4.8.3
Platform: M66 GSM Module
Architecture: ARM Cortex-M3
```

---

## 🔍 Debugging Tips

### How to Identify Ql_vsnprintf Issues

1. **Check buffer length after call:**
   ```c
   Ql_vsnprintf(buffer, size, format, args);
   u32 len = Ql_strlen(buffer);
   APP_DEBUG("Buffer length: %d\r\n", len);  // Will show 0 if broken
   ```

2. **Inspect first byte:**
   ```c
   APP_DEBUG("buffer[0] = '%c' (0x%02X)\r\n", buffer[0], (u8)buffer[0]);
   ```

3. **Compare with Ql_sprintf:**
   ```c
   // Test direct call (should work)
   Ql_sprintf(test_buf, "Test: %d", 42);
   APP_DEBUG("Direct: %s\r\n", test_buf);  // Should print "Test: 42"
   ```

### Debug Logging Pattern

Add these logs to trace the issue:
```c
APP_DEBUG("[DEBUG] Formatting into buffer at 0x%08X\r\n", (u32)buffer);
Ql_vsnprintf(buffer, size, format, args);
APP_DEBUG("[DEBUG] Format complete, buffer[0]='%c' (0x%02X)\r\n", 
         buffer[0], (u8)buffer[0]);
APP_DEBUG("[DEBUG] Buffer length = %d\r\n", Ql_strlen(buffer));
APP_DEBUG("[DEBUG] Buffer content: %s\r\n", buffer);
```

---

## 📚 References

### Quectel Documentation
- M66 OpenCPU API Reference (ql_stdlib.h)
- M66 OpenCPU User Guide V1.1

### Related Issues
- Command Interface garbled output (resolved)
- Response buffer corruption (resolved)

### Community
- Stack Overflow: M66 vsnprintf issues
- Quectel Forum: OpenCPU formatting problems

---

## ✅ Best Practices

To avoid SDK bugs:

1. **Never use `Ql_vsnprintf`** with `va_list`
2. **Always use `Ql_sprintf`** for formatting
3. **Test thoroughly** on actual hardware
4. **Add debug logs** during development
5. **Document workarounds** for your team
6. **Check buffer contents** after formatting calls
7. **Use global buffers** with mutex protection

---

## 📝 Contributing

Found a new SDK bug? Please document it here:

1. Describe the symptom
2. Show reproduction code
3. Explain root cause (if known)
4. Provide workaround
5. Test the workaround
6. Update this document

---

## 🎯 Summary

### Critical Issues
- ❌ `Ql_vsnprintf` is broken with `va_list`
- ✅ `Ql_sprintf` works correctly
- ✅ Workaround implemented and tested

### Safe Functions
- ✅ `Ql_sprintf` - Use this!
- ✅ `Ql_strlen`
- ✅ `Ql_memcpy`
- ✅ `Ql_memset`
- ✅ `Ql_strcmp`
- ✅ `APP_DEBUG` (uses `Ql_sprintf`)

### Unsafe Functions
- ❌ `Ql_vsnprintf` - AVOID!
- ⚠️ `Ql_strtok` - Missing (use custom implementation)

---

**Stay safe, code smart! 🛡️**

**Last Updated**: November 2, 2025  
**Maintained By**: Hossein Gholami

