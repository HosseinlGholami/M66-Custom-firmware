# Phase 1.1: NVRAM Parameter Storage Module

## 🎯 Goal
Create a simple, reliable parameter storage system using the M66's file system to store configuration in non-volatile memory.

---

## 📋 Requirements

### Functional
- Store key-value pairs (strings)
- Read parameters by key
- Write parameters by key
- Commit changes to NVRAM
- Load from NVRAM on boot
- Handle file corruption gracefully

### Non-Functional
- Simple text format (human-readable)
- Maximum 20 parameters
- Key length: max 32 characters
- Value length: max 128 characters
- Fast read/write (< 100ms)

---

## 🏗️ Architecture

```
Application
    ↓
param_get() / param_set() / param_commit()
    ↓
RAM Cache (param_entry_t array[20])
    ↓
param_commit() → File System
    ↓
NVRAM (config.txt)
```

###Storage Format

**File:** `config.txt`  
**Format:** Simple KEY=VALUE pairs, one per line

```
apn=internet
mqtt_host=mqtt.example.com
mqtt_port=1883
dev_id=M66_001
```

---

## 📁 Files to Create

### 1. `custom/param.h` - Header File
```c
/**
 * @file    param.h
 * @brief   NVRAM parameter storage module
 * @author  Your Name
 * @date    2025-11-01
 */

#ifndef PARAM_H
#define PARAM_H

#include "ql_type.h"

/*============================================================================
 * Constants
 *===========================================================================*/
#define PARAM_MAX_ENTRIES       20      // Maximum number of parameters
#define PARAM_KEY_MAX_LEN       32      // Maximum key length
#define PARAM_VALUE_MAX_LEN     128     // Maximum value length
#define PARAM_FILE_NAME         "config.txt"  // Storage file name

/*============================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Initialize parameter module
 * Loads parameters from NVRAM if file exists
 * @return 0 on success, negative on error
 */
s32 param_init(void);

/**
 * @brief Get parameter value by key
 * @param key Parameter key (e.g., "apn")
 * @param value Buffer to store value
 * @param max_len Maximum buffer length
 * @return 0 on success, -1 if not found
 */
s32 param_get(const char *key, char *value, u32 max_len);

/**
 * @brief Set parameter value by key
 * Updates RAM cache only - call param_commit() to save
 * @param key Parameter key
 * @param value Parameter value
 * @return 0 on success, negative on error
 */
s32 param_set(const char *key, const char *value);

/**
 * @brief Commit all changes to NVRAM
 * Writes RAM cache to file system
 * @return 0 on success, negative on error
 */
s32 param_commit(void);

/**
 * @brief Delete a parameter
 * @param key Parameter key to delete
 * @return 0 on success, -1 if not found
 */
s32 param_delete(const char *key);

/**
 * @brief Clear all parameters
 * @return 0 on success, negative on error
 */
s32 param_clear_all(void);

/**
 * @brief Get number of stored parameters
 * @return Number of parameters
 */
u32 param_count(void);

/**
 * @brief Print all parameters to debug output
 */
void param_print_all(void);

#endif /* PARAM_H */
```

### 2. `custom/param.c` - Implementation File

See the actual implementation below...

---

## 🔨 Implementation Steps

### Step 1: Create param.c Skeleton
```c
#include "param.h"
#include "ql_stdlib.h"
#include "ql_fs.h"
#include "ql_trace.h"

// Private data structures
// Helper functions
// Public API implementation
```

### Step 2: Define Data Structures
```c
typedef struct {
    char key[PARAM_KEY_MAX_LEN];
    char value[PARAM_VALUE_MAX_LEN];
    bool valid;  // TRUE if entry is used
} param_entry_t;

static param_entry_t param_cache[PARAM_MAX_ENTRIES];
static bool param_initialized = FALSE;
```

### Step 3: Implement Helper Functions
- `find_entry()` - Find parameter by key
- `load_from_file()` - Read from NVRAM
- `save_to_file()` - Write to NVRAM
- `parse_line()` - Parse "KEY=VALUE" format

### Step 4: Implement Public API
- `param_init()` - Initialize and load
- `param_get()` - Read from cache
- `param_set()` - Write to cache
- `param_commit()` - Save to file

### Step 5: Test with main.c
Add UART commands to test all functions

---

## 🧪 Testing Plan

### Test 1: Basic Write/Read
```c
param_init();
param_set("apn", "internet");
param_commit();

char value[128];
param_get("apn", value, sizeof(value));
// Expected: value = "internet"
```

### Test 2: Persistence
```c
// First boot
param_set("test", "hello");
param_commit();
// Reboot
param_init();
param_get("test", value, sizeof(value));
// Expected: value = "hello"
```

### Test 3: Multiple Parameters
```c
param_set("key1", "value1");
param_set("key2", "value2");
param_set("key3", "value3");
param_commit();
// Check count
// Expected: param_count() == 3
```

### Test 4: Update Existing
```c
param_set("apn", "first");
param_commit();
param_set("apn", "second");
param_commit();
param_get("apn", value, sizeof(value));
// Expected: value = "second"
```

### Test 5: Delete
```c
param_set("temp", "data");
param_commit();
param_delete("temp");
param_commit();
// Expected: param_get("temp") returns -1
```

---

## 🐛 Debug Tips

### Check File System
```c
s32 handle = Ql_FS_Open("config.txt", QL_FS_READ_ONLY);
if (handle < 0) {
    APP_DEBUG("File does not exist or error: %d\r\n", handle);
} else {
    APP_DEBUG("File opened successfully\r\n");
    Ql_FS_Close(handle);
}
```

### Verify Write
```c
s32 result = param_commit();
APP_DEBUG("Commit result: %d\r\n", result);
```

### Print All Parameters
```c
param_print_all();  // Will show all stored parameters
```

---

## ⚠️ Common Issues

### Issue 1: File Not Found on First Boot
**Solution:** This is normal - `param_init()` will create file on first `param_commit()`

### Issue 2: Values Not Persisting
**Solution:** Make sure you call `param_commit()` after `param_set()`

### Issue 3: Corrupted File
**Solution:** Delete `config.txt` and start fresh:
```c
Ql_FS_Delete("config.txt");
param_init();
```

---

## 📊 Expected Output

### Successful Initialization
```
[APP] Param: Initializing...
[APP] Param: Loaded 3 parameters from NVRAM
[APP] Param: Initialization complete
```

### Setting Parameters
```
[APP] Param: Set key1=value1
[APP] Param: Set key2=value2
[APP] Param: Committing to NVRAM...
[APP] Param: Commit successful (2 entries)
```

### Reading Parameters
```
[APP] Param: Get apn=internet
[APP] Param: Get mqtt_host=mqtt.example.com
```

---

## ✅ Definition of Done

- [ ] param.h created with all API functions
- [ ] param.c implements all functions
- [ ] Compiles without errors
- [ ] Can write and read parameters
- [ ] Parameters persist across reboot
- [ ] Can update existing parameters
- [ ] Can delete parameters
- [ ] UART test interface working
- [ ] No memory leaks
- [ ] Handles errors gracefully

---

## 🚀 Next Steps

After completing this module:

1. **Test Thoroughly** - Try all edge cases
2. **Add Default Values** - Load defaults on first boot
3. **Move to Phase 1.2** - Create UART test interface
4. **Document** - Add comments and usage examples
5. **Move to Phase 2** - Network connectivity

---

**Time Estimate:** 2-3 hours  
**Difficulty:** ⭐⭐ (Beginner-Intermediate)  
**Dependencies:** None - this is your first module!

**Ready?** → Create `custom/param.h` and `custom/param.c` using the templates above!

