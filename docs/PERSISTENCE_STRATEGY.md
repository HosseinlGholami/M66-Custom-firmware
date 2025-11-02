# Parameter Persistence Strategy Guide

## 🎯 Overview: Two-Flag Design Pattern

Your parameter system uses a **deferred write strategy** with two independent flags:

| Flag | Purpose | When Set | When Cleared |
|------|---------|----------|--------------|
| **`persist`** | **Configuration**: Should this param be saved to NVRAM? | At initialization or via `param_set_persist()` | Via `param_set_persist(FALSE)` |
| **`dirty`** | **State Tracking**: Has this param changed since last save? | Every `param_set_*()` call | After successful `param_commit()` |

---

## 📊 How They Work Together

```c
// When you call param_set_int32(PARAM_MQTT_PORT, 1883):
┌─────────────────────────────────────────────────┐
│ param_set_int32(PARAM_MQTT_PORT, 1883)          │
│                                                 │
│ 1. ✅ Validate key and type                     │
│ 2. 🔒 Lock mutex (thread-safe)                  │
│ 3. 💾 Write to RAM: value.i32 = 1883            │
│ 4. 🏷️  Set dirty = TRUE (modified!)             │
│ 5. ❌ Do NOT write to file yet!                 │
│ 6. 🔓 Unlock mutex                              │
└─────────────────────────────────────────────────┘
       ↓
   Changes are in RAM only (FAST! ~2µs)
       ↓
   File write happens later when YOU call param_commit()
```

### When `param_commit()` is Called:

```c
┌─────────────────────────────────────────────────┐
│ param_commit()                                  │
│                                                 │
│ 1. 🔍 Loop through ALL parameters               │
│ 2. ✅ Find params where:                        │
│        persist == TRUE  (config says save it)   │
│    AND dirty == TRUE    (it was modified)       │
│ 3. 💾 Write those params to NVRAM file          │
│ 4. 🧹 Clear dirty flag for saved params         │
│ 5. ✅ Return number of params saved             │
└─────────────────────────────────────────────────┘
```

---

## 🔄 Complete Lifecycle Example

```c
/* STEP 1: Initialization (boot time) */
param_init();
// Loads existing persistent params from NVRAM
// All params start with dirty = FALSE

/* STEP 2: Modify persistent parameter */
param_set_int16(PARAM_MQTT_PORT, 1883);
// ✅ RAM: value = 1883
// ✅ RAM: dirty = TRUE
// ✅ RAM: persist = TRUE (from param_config)
// ❌ NVRAM: Not written yet!

param_set_string(PARAM_APN, "internet");
// ✅ RAM: value = "internet"
// ✅ RAM: dirty = TRUE
// ✅ RAM: persist = TRUE
// ❌ NVRAM: Not written yet!

/* STEP 3: Modify RAM-only parameter */
param_set_int16(PARAM_SENSOR_TEMP, 2543);
// ✅ RAM: value = 2543
// ✅ RAM: dirty = TRUE
// ✅ RAM: persist = FALSE (will NOT be saved!)
// ❌ NVRAM: Will never be written (by design)

/* STEP 4: Explicit commit (you control WHEN) */
s32 saved = param_commit();
// Returns: 2 (saved MQTT_PORT and APN)
// ✅ NVRAM: MQTT_PORT and APN written to file
// ✅ RAM: MQTT_PORT.dirty = FALSE
// ✅ RAM: APN.dirty = FALSE
// ✅ RAM: SENSOR_TEMP.dirty = FALSE (cleared but not saved)

/* STEP 5: Another modification */
param_set_int16(PARAM_MQTT_PORT, 8883);
// ✅ RAM: dirty = TRUE again
// ❌ NVRAM: Still has old value (1883)

/* STEP 6: Commit again */
param_commit();
// ✅ NVRAM: Updated to 8883
```

---

## ⚡ Why This Design? Performance & Flash Wear

### Current Design: **Deferred Write (Batched)**

```c
/* Fast: 100 updates in RAM */
for (int i = 0; i < 100; i++) {
    param_set_int16(PARAM_SENSOR_TEMP, sensor_read());
    Ql_Sleep(100);  // Every 100ms
}

/* Slow: 1 write to NVRAM when you decide */
param_commit();  // Single file operation
```

**Performance:**
- ✅ 100 RAM writes: ~200µs total (2µs each)
- ✅ 1 NVRAM write: ~50ms (only when YOU call commit)
- ✅ Flash wear: 1 erase/write cycle

### Alternative: **Immediate Write** ❌

```c
/* If we wrote to NVRAM on every param_set_*(): */
for (int i = 0; i < 100; i++) {
    param_set_int16(PARAM_SENSOR_TEMP, sensor_read());
    // ❌ Would write to NVRAM here (50ms each!)
    Ql_Sleep(100);
}
```

**Performance:**
- ❌ 100 NVRAM writes: ~5000ms total (50ms each)
- ❌ Flash wear: 100 erase/write cycles
- ❌ Slow, blocks other tasks, wears flash

---

## 🎯 When to Call `param_commit()`

### ✅ Good Times to Commit:

1. **After Configuration Changes**
   ```c
   param_set_string(PARAM_APN, new_apn);
   param_set_int16(PARAM_MQTT_PORT, new_port);
   param_commit();  // Save new config
   ```

2. **Before Critical Operations**
   ```c
   param_set_int32(PARAM_FOTA_STATE, FOTA_DOWNLOADING);
   param_commit();  // Save state before risky operation
   perform_firmware_update();
   ```

3. **Periodic Saves (e.g., every hour)**
   ```c
   void timer_callback(void) {
       if (has_uncommitted_changes()) {
           param_commit();
       }
   }
   ```

4. **Before Power Down**
   ```c
   void shutdown_handler(void) {
       param_commit();  // Save everything before power off
       Ql_Sleep(100);
       Ql_Reset(0);
   }
   ```

### ❌ Bad Times to Commit:

1. **High-Frequency Updates**
   ```c
   // BAD: Commits every 100ms!
   while (1) {
       param_set_int16(PARAM_SENSOR_TEMP, sensor_read());
       param_commit();  // ❌ Too frequent! Wears flash!
       Ql_Sleep(100);
   }
   ```

2. **Inside ISR or Critical Section**
   ```c
   void uart_rx_interrupt(void) {
       param_set_int8(PARAM_RX_COUNT, count);
       param_commit();  // ❌ Too slow for ISR!
   }
   ```

---

## 📊 Comparison: `persist` vs `dirty`

| Aspect | `persist` Flag | `dirty` Flag |
|--------|----------------|--------------|
| **Meaning** | "Should be saved to NVRAM" | "Has been modified since last save" |
| **Set by** | Configuration (param_config) or runtime API | Automatically by `param_set_*()` |
| **Cleared by** | `param_set_persist(FALSE)` | `param_commit()` |
| **Purpose** | Configuration policy | Change tracking |
| **Example** | Network config (persist=TRUE)<br>Sensor data (persist=FALSE) | Any modified parameter |
| **Mutable?** | ✅ Yes (via `param_set_persist()`) | ✅ Yes (automatically) |

---

## 🔍 Visual State Machine

```
┌─────────────────────────────────────────────────────┐
│ Parameter State Machine (per parameter)            │
└─────────────────────────────────────────────────────┘

  [persist=TRUE, dirty=FALSE]
           │
           │ param_set_*()
           ↓
  [persist=TRUE, dirty=TRUE] ←──────┐
           │                        │
           │ param_commit()         │ param_set_*()
           ↓                        │ (again)
  [persist=TRUE, dirty=FALSE] ──────┘
           ↑
           │ (saved to NVRAM)
           
           
  [persist=FALSE, dirty=FALSE]
           │
           │ param_set_*()
           ↓
  [persist=FALSE, dirty=TRUE]
           │
           │ param_commit()
           ↓
  [persist=FALSE, dirty=FALSE]
           ↑
           │ (NOT saved, just cleared)
```

---

## 🚀 Use Cases

### Use Case 1: Network Configuration (Persistent)

```c
/* User updates APN via SMS command */
void handle_sms_config(const char* new_apn) {
    param_set_string(PARAM_APN, new_apn);
    param_set_persist(PARAM_APN, TRUE);  // Ensure persistence
    
    s32 saved = param_commit();
    if (saved > 0) {
        send_sms("Config saved. Rebooting...");
        Ql_Sleep(1000);
        Ql_Reset(0);  // Reboot to apply
    }
}
```

### Use Case 2: Sensor Data (RAM-only, Fast Sync)

```c
/* Task 1: Read sensor every 100ms */
void sensor_task(s32 taskId) {
    while (1) {
        s16 temp = read_temperature();
        param_set_int16(PARAM_SENSOR_TEMP, temp);
        // No commit! Just RAM update for other tasks
        Ql_Sleep(100);
    }
}

/* Task 2: Publish sensor data every 60s */
void mqtt_task(s32 taskId) {
    while (1) {
        s16 temp;
        param_get_int16(PARAM_SENSOR_TEMP, &temp);
        mqtt_publish("temp", temp);
        // No commit needed! Reading from RAM
        Ql_Sleep(60000);
    }
}
```

### Use Case 3: Mixed (Some Persistent, Some RAM)

```c
/* Configuration flow */
void connect_network(void) {
    /* 1. Load persistent config from NVRAM */
    char apn[256];
    s16 port;
    param_get_string(PARAM_APN, apn, sizeof(apn));
    param_get_int16(PARAM_MQTT_PORT, &port);
    
    /* 2. Update RAM-only runtime state */
    param_set_int8(PARAM_NET_RSSI, get_signal_strength());
    param_set_int32(PARAM_TASK_COUNTER, 0);
    
    /* 3. No commit needed! Runtime state is RAM-only */
    
    /* 4. If connection succeeds, optionally save last-known-good config */
    if (connection_success) {
        param_set_string(PARAM_LAST_GOOD_APN, apn);
        param_commit();  // Save backup config
    }
}
```

---

## 🛠️ Advanced: Runtime Persistence Control

You can change the `persist` flag at runtime:

```c
/* Start with RAM-only (fast logging) */
param_set_persist(PARAM_SENSOR_TEMP, FALSE);

/* ... collect 1000 samples in RAM ... */

/* Now decide to save the last value */
param_set_persist(PARAM_SENSOR_TEMP, TRUE);
param_commit();  // This will now save it!

/* Switch back to RAM-only */
param_set_persist(PARAM_SENSOR_TEMP, FALSE);
```

---

## 📋 Summary: When File Write Happens

| Operation | RAM Update? | File Write? | Notes |
|-----------|-------------|-------------|-------|
| `param_set_*()` | ✅ Yes (immediate) | ❌ No | Fast, sets dirty=TRUE |
| `param_get_*()` | ❌ No | ❌ No | Just reads RAM |
| `param_commit()` | ❌ No | ✅ Yes (if persist=TRUE && dirty=TRUE) | Slow, clears dirty |
| `param_init()` | ✅ Yes (load) | ❌ No (read-only) | Loads from NVRAM at boot |

---

## 🎯 Key Takeaways

1. **`param_set_*()` NEVER writes to NVRAM** (by design)
2. **Only `param_commit()` writes to NVRAM** (you control when)
3. **`persist` = configuration** (should save?)
4. **`dirty` = change tracking** (was modified?)
5. **Deferred write = fast + low flash wear**

---

## 🔧 If You Want Immediate Write (Not Recommended)

If you really want to write to NVRAM on every set, you can add this to `param_set_int32()`:

```c
s32 param_set_int32(ParamKey_e key, s32 value)
{
    /* ... existing validation ... */
    
    param_lock();
    param_data[key].value.i32 = value;
    param_data[key].dirty = TRUE;
    param_unlock();
    
    /* NEW: Optional immediate write */
    if (param_data[key].persist) {
        param_commit();  // ⚠️ Slow! Wears flash!
    }
    
    return 0;
}
```

**But this defeats the purpose of the design!** You lose:
- ❌ Performance (50ms per write)
- ❌ Flash lifespan (100k cycles → 10k cycles)
- ❌ Batching efficiency

---

## 🎓 Recommended Pattern

```c
/* Good practice: Explicit, controlled commits */

// 1. Modify multiple params (fast, in RAM)
param_set_string(PARAM_APN, "internet");
param_set_string(PARAM_MQTT_HOST, "broker.example.com");
param_set_int16(PARAM_MQTT_PORT, 1883);

// 2. Single commit (slow, to NVRAM)
param_commit();  // Saves all dirty+persistent params in one file write

// This is:
// ✅ Fast (3 RAM writes + 1 NVRAM write)
// ✅ Efficient (1 flash cycle instead of 3)
// ✅ Atomic (all-or-nothing save)
```

---

**Author**: Hossein Gholami  
**Date**: 2025-11-01  
**Version**: 1.0

