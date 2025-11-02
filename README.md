# M66 Firmware Architecture (Multi‑Threaded) — **Architecture Document**

**Device**: Industrial controller board (12 V DC), Quectel **M66** modem, I²C **PCF8574** expander, opto‑isolated inputs, relay/driver outputs  
**Services**: FOTA, MQTT telemetry/commands, I²C GPIO (DI/DO) with interrupt, SMS control, Watchdog, NVRAM config, diagnostics

---

## 1) Scope & Objectives

### 1.1 Goals
- Reliable field device that **connects over GSM/GPRS**, publishes telemetry, receives commands, and **updates firmware OTA**.
- Deterministic **I/O control** via I²C expander with **interrupt-driven** digital inputs and safe digital outputs.
- **Resilient runtime**: watchdog, brownout awareness, crash logging, and rollback from failed OTA.
- **Configurable** via MQTT/SMS and stored in **non‑volatile KV** with atomic commits and wear‑levelling.

### 1.2 Non‑Goals
- Cloud backend design (broker, dashboards) is out of scope.
- UI/HMI; device has headless operation.
- Cryptographic key provisioning process is out of scope (keys assumed available if TLS is used).

---

## 2) System Context

### 2.1 Hardware (summary)
- **Power**: 12 V input, protected by fuse/TVS/varistor/ideal diode, DC/DC to 5 V & 4 V rails.
- **Compute/Modem**: Quectel **M66** (OpenCPU target) running RTOS tasks.
- **I/O**: I²C **PCF8574** (0x20…0x27). DO via transistor/ULN2803A, DI via opto‑isolators; shared **/INT** line.
- **Indicators**: NETLIGHT; optional STATUS LED.
- **Storage**: internal flash partitioned for **app A/B** + **NVRAM KV** + **crash log**.
- **Optional**: eFuse/PMIC FAULT signal to GPIO for brownout/over‑current status.

### 2.2 External Systems
- **MQTT broker** (public/private), optional TLS.
- **HTTP(S) object store** for OTA images.
- **SMS network** for fallback control.

---

## 3) Architecture Overview

The firmware is decomposed into independent **tasks (threads)** communicating via an internal **message bus** (queues + events). Shared state (configuration) is accessed via a single **KV service** to guarantee atomicity and durability.

```mermaid
flowchart LR
  subgraph Device
    A[sys_main] --> B[net_mgr]
    B <--> C[mqtt_cli]
    A --> D[io_mgr]
    A --> E[sms_srv]
    A --> F[fota_mgr]
    A --> G[kv_store]
    A --> H[wdog]
    A --> I[fault_logger]

    D -->|DI change events| C
    E -->|commands| C
    C -->|downlink cmds| D
    C -->|FOTA trigger| F
    F -->|progress/status| C
    G <--> C
    G <--> E
  end

  C -->|MQTT| Broker[(MQTT Broker)]
  F -->|HTTP(S)| Store[(Firmware Store)]
  E -->|SMS| PLMN[(SMS Network)]
```

---

## 4) Task Model & Responsibilities

| Task        | Priority | Stack | Responsibilities | Inputs | Outputs |
|-------------|----------|-------|------------------|--------|---------|
| **sys_main** | High     | Med   | Startup sequence, dependency ordering, health timer | — | Task creation, periodic heartbeat |
| **net_mgr**  | High     | Med   | Modem power/boot, SIM, APN/PDP, IP bring‑up, signal metrics | sys_main | Link events (UP/DOWN), RSSI |
| **mqtt_cli** | High     | High  | Connect/reconnect, subscribe/publish, keepalive, QoS1 ACK, backoff | net_mgr, kv_store | Command frames, acks, telemetry |
| **io_mgr**   | Med      | Med   | I²C PCF8574 init, **/INT** ISR, debounce, DI events, DO actuation with shadow | mqtt_cli, interrupts | DI change events |
| **sms_srv**  | Med      | Med   | Receive/parse SMS commands, send SMS responses, admin whitelist | PLMN, kv_store | Control/kv events |
| **fota_mgr** | Med      | High  | OTA job lifecycle: validate, download (chunked), verify hash, flag, reboot/rollback | mqtt_cli/sms, Store | Progress, result, reboot |
| **kv_store** | Med      | Med   | Journaled KV over flash, atomic commits, wear leveling, schema/versioning | all tasks | KV values, change notifications |
| **wdog**     | High     | Low   | Watchdog feeding policy, liveness aggregation, escalation & reboot | periodic beats | Reboot with reason |
| **fault_logger** | Low  | Low   | Crash records, counters (brownout, reconnects), ring buffer | tasks | Persisted logs |

**Preemption & Timing**: High priority tasks run networking, FOTA, and watchdog. I/O and SMS are medium. Logging is low to minimize impact.

---

## 5) Inter‑Task Communication

- **Message Bus**: Typed events carrying small payloads (e.g., `EV_LINK_UP`, `EV_DI_CHANGE(bitmask)`, `EV_FOTA_PROGRESS(pct)`).  
- **Queues**: One RX queue per task to avoid contention; producers push events to consumers.  
- **Timers**: Software timers for debounce, periodic telemetry, reconnect backoff.  
- **Config API**: `kv_get(key)`, `kv_set(key,value)` via requests that serialize access through `kv_store`.

---

## 6) Configuration & NVRAM (KV)

### 6.1 Key Set (initial)
- Identity: `dev_id`, `imei` (read‑only cache)  
- Network: `apn`, `apn_user`, `apn_pass`  
- MQTT: `host`, `port`, `user`, `pass`, `keepalive_s`, `pub_period_s`  
- I/O: `i2c_addr_pcf`, `debounce_ms`, `di_invert_mask`, `do_powerup_mask`  
- FOTA: `auto_apply` (bool), `channel` (prod/stage)  
- SMS: `admin_msisdn` (whitelist), `sms_enable`  
- Security: `tls_enable`, `ca_slot`, `psk_id`, `psk_key` (if PSK)  

### 6.2 Storage Strategy
- **Journaled dual‑page** layout (A/B) with: header (magic, version, seqno, CRC) + TLVs.  
- **Atomic commit**: append TLVs → write new header with CRC → swap active page.  
- **Wear‑levelling**: rotate pages; background GC when >75% filled.  
- **Schema Migration**: version field; on boot, migrate defaults for new keys.

---

## 7) Networking & MQTT

### 7.1 Network Lifecycle
1. Modem power & init  
2. SIM & PIN (if any)  
3. APN set, PDP activation, IP acquired  
4. Time sync (optional), publish link event

### 7.2 MQTT Behavior
- **Connect** with client‑id=`dev_id`, clean‑session=false (if supported).  
- **Keepalive**: 60 s (configurable), ping if idle.  
- **QoS**: Telemetry QoS0; Commands/Acks QoS1.  
- **Reconnect Backoff**: 1→2→4→8→… up to 5 min + jitter.  
- **Offline Buffering**: Device caches at least last N telemetry frames (NVRAM or RAM ring).

### 7.3 Topic Convention
- `plant/{dev_id}/state` — periodic consolidated status  
- `plant/{dev_id}/event` — DI edges, faults  
- `plant/{dev_id}/cmd` — downlink commands (JSON)  
- `plant/{dev_id}/fota` — FOTA control & progress

> Exact payload schemas will be specified in the Interface Contract (Section 12).

---

## 8) I/O Subsystem (I²C + GPIO + Interrupt)

### 8.1 PCF8574 Usage
- **Outputs (DO)**: Controlled via a shadow byte to ensure atomic bit updates; at boot, apply `do_powerup_mask`.  
- **Inputs (DI)**: Interrupt‑driven using shared **/INT** (active‑low).

### 8.2 Debounce
- On **/INT**: read current byte → compare with last stable → start per‑bit debounce timers for changed bits.  
- After `debounce_ms` stable: emit `EV_DI_CHANGE` with `changed_mask` and `new_state`.  
- `di_invert_mask` applied if opto logic is inverted (active‑low).

### 8.3 Safety
- **Software interlocks** optional (e.g., prevent pump + valve simultaneous start if PSU limited).  
- **Brownout** input (if wired) triggers throttling and fault log entry.

---

## 9) FOTA (Firmware Over‑The‑Air)

### 9.1 Trigger Sources
- **MQTT** command on `…/cmd` (e.g., `{ "op":"fota", "url":"…", "sha256":"…", "size": … }`).  
- **SMS**: `FOTA <url> <sha256>` (restricted to admin).

### 9.2 Workflow
1. **Validate**: URL, free space, expected size/hash.  
2. **Download**: Chunked (1–4 KB), HTTP range support for resume.  
3. **Verify**: Streaming SHA‑256 against expected; size check.  
4. **Apply**: Write upgrade flag/metadata.  
5. **Reboot** into new slot.  
6. **Confirm**: On successful boot, mark “confirmed”; otherwise **rollback** to previous slot.

### 9.3 Progress & Resilience
- Progress events at 0/25/50/75/100% on `…/fota`.  
- Power‑loss safe via state cookie; resume if possible.  
- Watchdog long‑op heartbeat while downloading.

---

## 10) Watchdog & Reliability

- **Policy**: Central watchdog aggregates heartbeats from `net_mgr`, `mqtt_cli`, `io_mgr`, `sms_srv`, `fota_mgr`, and `sys_main`.  
- **Threshold**: Missing 3 consecutive beats from any **critical** task → immediate reboot with reason code.  
- **Reset Cause** recorded to crash log (watchdog, brownout, manual, OTA).  
- **Graceful Paths**: During FOTA apply or KV commit, watchdog window extended moderately.

---

## 11) SMS Control (Fallback/Field Ops)

**Commands** (text), subject to whitelist and enable flag:  
- `STATUS` → returns RSSI, IP, DI/DO, VBAT, FW, uptime.  
- `DO <ch> <0|1>` → set digital output.  
- `KV GET <key>` / `KV SET <key> <value>` → configuration.  
- `FOTA <url> <sha256>` → trigger OTA.  
- `REBOOT` → immediate reboot.

Rate‑limit SMS parsing to prevent resource starvation.

---

## 12) Interface Contract (External)

### 12.1 MQTT Topics (Device‑Scoped)
- **Uplink**  
  - `plant/{dev_id}/state` (periodic)  
  - `plant/{dev_id}/event` (as‑needed)  
  - `plant/{dev_id}/fota` (progress/result)
- **Downlink**  
  - `plant/{dev_id}/cmd`

### 12.2 Payload Conventions
- Encoding: **UTF‑8 JSON**; timestamps in **epoch ms**; booleans for logic; arrays for DI/DO bitmaps optional.  
- All downlink commands include `req_id` (string) and device replies include `req_id` to correlate.

> Detailed JSON schemas will be finalized with the backend team and versioned independently.

---

## 13) Telemetry & Diagnostics

- **Counters**: reconnects, DI edges, FOTA fails, KV commits, SMS handled.  
- **Metrics**: RSSI/BER, IP, uptime, min/max VBAT since boot.  
- **Crash Log**: last N resets with reason and context snippet.  
- **Remote Log Level**: via KV (`log_level`: error/warn/info/debug).

---

## 14) Security Considerations

- **MQTT**: Prefer TLS; if not feasible, sign sensitive commands (HMAC) and enforce admin list for SMS.  
- **FOTA**: Hash verify (SHA‑256); optional signature verify if key provisioning available.  
- **Config**: Protect credentials in KV (obfuscation at rest; avoid plaintext SMS).  
- **Brute‑force**: Lockout after repeated invalid SMS commands (optional).

---

## 15) Resource Budgets & Performance Targets

- **Flash**: Dual‑slot app images + 64–128 KB NVRAM/Logs target.  
- **RAM**: Task stacks sized per role; total working set **≤ 60–70%** of available RAM under load.  
- **CPU**: Telemetry cycle < 10 ms excluding I/O wait; debounce granularity 10–20 ms.  
- **Network**: Reconnect ≤ 10 s typical; OTA throughput matches GPRS conditions (best‑effort).

---

## 16) Power & Boot

- **Boot Order**: rails → GPIO/I²C → KV mount → modem power → IP → MQTT → I/O → SMS → idle.  
- **Power Loss**: KV commits atomic; OTA resume cookie; output states restored from KV or safe defaults.  
- **Startup Outputs**: Use `do_powerup_mask` to avoid unintended actuation.

---

## 17) Test Strategy

- **Unit**: KV journal, debounce logic, payload parser (MQTT/SMS).  
- **Integration**: I²C/INT with real PCF8574; loopback DO→DI tests.  
- **System**: Network loss/recovery, broker restart, FOTA resume, watchdog kicks under stress.  
- **Field**: SMS control in low‑signal areas; brownout injection; long‑run stability (≥ 7 days).

---

## 18) Risks & Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| GPRS instability | Command lag / drops | Backoff, resume, QoS1 for commands, offline buffer |
| Power dips during FOTA | Soft‑brick | A/B slots, verified apply, rollback |
| DI false triggers | Spurious events | Interrupt + debounce, input filtering, invert masks |
| Flash wear | KV loss | Journal + wear‑levelling, commit rate limits |
| Unauthorized control | Safety | SMS whitelist, TLS/HMAC, role‑based command gating |

---

## 19) Open Items

- Finalize TLS feasibility and certificate/PSK provisioning path.  
- Confirm exact DI/DO bit mapping on PCF8574.  
- Decide telemetry rate & payload schema with cloud team.  
- Define image signing keys & ownership (if used).

---

## 20) Acceptance Criteria (Excerpt)

- Device **boots to MQTT online** within 90 s with valid SIM/APN.  
- **DI interrupt + debounce**: detects edges with ≤ 30 ms added latency.  
- **FOTA**: download→apply→confirm; rollback proven on forced failure.  
- **Watchdog**: provably reboots on hung MQTT or I/O task within 30 s.  
- **KV**: survives 1,000+ updates without corruption; recovery after power cut during commit.

---

## 21) Versioning & Release

- SemVer `MAJOR.MINOR.PATCH`.  
- Each release delivers: firmware image(s), KV schema version, migration notes, and acceptance report.

---

## 22) Glossary

- **A/B**: Dual firmware slots for OTA safety.  
- **DI/DO**: Digital Input/Output.  
- **KV**: Key‑Value configuration store.  
- **PDP**: Packet Data Protocol (GPRS context).  
- **INT**: Interrupt line from PCF8574 (active‑low).

---