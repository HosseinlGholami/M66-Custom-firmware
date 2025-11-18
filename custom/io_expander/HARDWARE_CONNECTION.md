# Hardware Connection Guide

## Complete System Wiring Diagram

```
                           M66 Module
                       ┌───────────────┐
                       │               │
        ┌──────────────│ RI (SCL)      │
        │              │               │
        │  ┌───────────│ DCD (SDA)     │
        │  │           │               │
        │  │  ┌────────│ CTS (INT)     │
        │  │  │        │               │
        │  │  │   ┌────│ 3.3V/5V (VCC) │
        │  │  │   │    │               │
        │  │  │   │  ┌─│ GND           │
        │  │  │   │  │ └───────────────┘
        │  │  │   │  │
        │  │  │   │  │
    ┌───┴──┴──┴───┴──┴─┐     Pull-up Resistors:
    │  SCL SDA INT VCC GND │     - 4.7kΩ on SCL to VCC
    │  PCF8574 #1 (0x42)  │     - 4.7kΩ on SDA to VCC
    │  P0 P1 P2 P3        │     - 10kΩ on INT to VCC
    │  P4 P5 P6 P7        │
    └──────────────────────┘
           │  │  │  │
           │  │  │  └────────── To buttons/inputs
           │  │  └───────────── (inputs)
           │  └──────────────── (outputs)
           └─────────────────── To LEDs/relays
    
    ┌──────────────────────┐
    │  SCF SDA INT VCC GND │
    │  PCF8574 #2 (0x43)  │
    │  P0 P1 P2 P3        │
    │  P4 P5 P6 P7        │
    └──────────────────────┘
           │  │  │  │
           │  │  │  └────────── To buttons/inputs
           │  │  └───────────── (inputs)
           │  └──────────────── (outputs)
           └─────────────────── To LEDs/relays

Note: SCL, SDA, INT, VCC, GND are connected in parallel
      (daisy-chain or star configuration)
```

## Pin Mapping Table

| M66 Pin | Function | PCF8574 Pin | Pull-up Resistor | Notes |
|---------|----------|-------------|------------------|-------|
| RI      | I2C SCL  | SCL (all)   | 4.7kΩ to VCC    | Shared with OLED |
| DCD     | I2C SDA  | SDA (all)   | 4.7kΩ to VCC    | Shared with OLED |
| CTS     | Interrupt| INT (all)   | 10kΩ to VCC     | Active LOW |
| 3.3V/5V | Power    | VCC (all)   | -               | Check PCF8574 voltage |
| GND     | Ground   | GND (all)   | -               | Common ground |

## PCF8574 Pin Layout

```
           PCF8574
         ┌─────────┐
    A0 ──┤ 1    16 ├── VCC (3.3V or 5V)
    A1 ──┤ 2    15 ├── SDA (to M66 DCD)
    A2 ──┤ 3    14 ├── SCL (to M66 RI)
    P0 ──┤ 4    13 ├── INT (to M66 CTS)
    P1 ──┤ 5    12 ├── P7
    P2 ──┤ 6    11 ├── P6
    P3 ──┤ 7    10 ├── P5
   GND ──┤ 8     9 ├── P4
         └─────────┘
```

## I2C Address Configuration

### Address Selection via A0-A2 Pins

| A2 | A1 | A0 | 7-bit Address | 8-bit Write | 8-bit Read |
|----|----|----|---------------|-------------|------------|
| L  | L  | L  | 0x20          | 0x40        | 0x41       |
| L  | L  | H  | 0x21          | 0x42        | 0x43       |
| L  | H  | L  | 0x22          | 0x44        | 0x45       |
| L  | H  | H  | 0x23          | 0x46        | 0x47       |
| H  | L  | L  | 0x24          | 0x48        | 0x49       |
| H  | L  | H  | 0x25          | 0x4A        | 0x4B       |
| H  | H  | L  | 0x26          | 0x4C        | 0x4D       |
| H  | H  | H  | 0x27          | 0x4E        | 0x4F       |

**Note:** L = Connect to GND, H = Connect to VCC

### Your Configuration (0x42, 0x43)

⚠️ **Address Mismatch Warning:**
- You specified addresses 0x42 and 0x43
- These could be:
  1. **8-bit addresses** → Convert to 7-bit: 0x21 (0x42/2) and ???
  2. **Non-standard variant** → Verify with I2C scanner
  3. **PCF8574A** (base 0x38) with different pin configuration

**Action Required:**
```c
// In your code, run I2C scanner:
i2c_scanner_scan();

// Then update io_expander_config.h with actual 7-bit addresses
```

## Pull-up Resistor Placement

```
VCC (3.3V or 5V)
    │
    │
   ┌┴┐ 4.7kΩ (SCL pull-up)
   │ │
   └┬┘
    ├────────── SCL line (to all PCF8574s and M66 RI)
    │
    │
   ┌┴┐ 4.7kΩ (SDA pull-up)
   │ │
   └┬┘
    ├────────── SDA line (to all PCF8574s and M66 DCD)
    │
    │
   ┌┴┐ 10kΩ (INT pull-up)
   │ │
   └┬┘
    ├────────── INT line (to all PCF8574s and M66 CTS)
    
Note: Use only ONE set of pull-ups for the entire bus
```

## Common Connection Examples

### Example 1: LED Output (Active HIGH)

```
PCF8574 P0 ──┬─── 220Ω ───(LED)──── GND
             │
          (internal)
```

When P0 = HIGH → LED ON  
When P0 = LOW  → LED OFF

```c
// Code:
io_expander_set_pins(0, IO_EXPANDER_PIN0);    // LED ON
io_expander_clear_pins(0, IO_EXPANDER_PIN0);  // LED OFF
```

### Example 2: LED Output (Active LOW)

```
VCC ───(LED)─── 220Ω ─── PCF8574 P0
```

When P0 = LOW  → LED ON (sink current)  
When P0 = HIGH → LED OFF

```c
// Code:
io_expander_clear_pins(0, IO_EXPANDER_PIN0);  // LED ON
io_expander_set_pins(0, IO_EXPANDER_PIN0);    // LED OFF
```

### Example 3: Button Input (Active LOW with Pull-up)

```
VCC
 │
10kΩ (external pull-up, or rely on internal weak pull-up)
 │
 ├───── PCF8574 P4 (configured as input)
 │
[Button]
 │
GND
```

When button pressed → P4 = LOW (0)  
When button released → P4 = HIGH (1)

```c
// Code:
io_expander_set_as_input(0, IO_EXPANDER_PIN4);  // Configure as input
u8 button_state;
io_expander_read_pin(0, 4, &button_state);
if (button_state == 0) {
    // Button pressed
}
```

### Example 4: Relay Module (Active HIGH)

```
PCF8574 P0 ───┬─── [Relay Module] ───┬─── Load
              │                      │
             GND                    VCC
```

When P0 = HIGH → Relay ON  
When P0 = LOW  → Relay OFF

```c
// Code:
io_expander_set_pins(0, IO_EXPANDER_PIN0);    // Relay ON
io_expander_clear_pins(0, IO_EXPANDER_PIN0);  // Relay OFF
```

### Example 5: Relay Module (Active LOW)

```
PCF8574 P0 ───┬─── [Relay Module] ───┬─── Load
             VCC                     │
                                    GND
```

When P0 = LOW  → Relay ON  
When P0 = HIGH → Relay OFF

```c
// Code:
io_expander_clear_pins(0, IO_EXPANDER_PIN0);  // Relay ON
io_expander_set_pins(0, IO_EXPANDER_PIN0);    // Relay OFF
```

### Example 6: Sensor Input (Digital Output Sensor)

```
VCC ───[Sensor]─── PCF8574 P4
                   │
                  10kΩ (pull-down)
                   │
                  GND
```

When sensor active → P4 = HIGH (1)  
When sensor inactive → P4 = LOW (0)

```c
// Code:
io_expander_set_as_input(0, IO_EXPANDER_PIN4);
u8 sensor_state;
io_expander_read_pin(0, 4, &sensor_state);
if (sensor_state == 1) {
    // Sensor active
}
```

## Interrupt Pin (INT) Behavior

```
Interrupt Timing:
                   ___      ___
INT (idle)    ────┘   └────┘   └──── (HIGH with pull-up)
                   ^      ^
                   │      │
                   │      └── Read clears interrupt
                   └───────── Pin change triggers interrupt

M66 EINT triggers on falling edge (HIGH → LOW)
```

**Important:**
- INT is open-drain, requires pull-up
- Goes LOW when ANY input changes
- Stays LOW until port is read
- All PCF8574 INT pins tied together (wired-OR)

## Power Supply Considerations

### Voltage Levels
- **PCF8574**: 2.5V - 6V operation
- **M66 I2C**: 3.3V logic (typically)
- **Recommendation**: Use 3.3V if M66 is 3.3V, or use level shifters for 5V

### Current Limits
- **PCF8574 Output**: 25mA per pin maximum
- **PCF8574 Total**: 100mA total (all pins combined)
- **For higher loads**: Use transistor/MOSFET driver or relay module

### Current Budget Example
```
P0: LED (20mA)          ✓ OK
P1: LED (20mA)          ✓ OK
P2: Relay module (5mA)  ✓ OK
P3: Relay module (5mA)  ✓ OK
P4-P7: Inputs (1mA ea)  ✓ OK
Total: ~54mA            ✓ OK (< 100mA)
```

### Overload Example (BAD)
```
P0-P7: LEDs (20mA ea)
Total: 160mA            ✗ EXCEEDS LIMIT!
Solution: Use driver transistors
```

## Bus Topology

### Star Configuration (Recommended for flexibility)
```
         M66 (RI, DCD, CTS)
            │   │   │
     ┌──────┴───┴───┴──────┐
     │      │   │   │      │
     │      │   │   │      │
  PCF8574  │   │   │   PCF8574
   0x42    │   │   │    0x43
           └───┼───┘
               │
            More devices...
```

### Daisy-Chain Configuration (Simpler wiring)
```
M66 → PCF8574(0x42) → PCF8574(0x43) → More devices...
      SCL,SDA,INT     SCL,SDA,INT
```

**Both work fine, choose based on your PCB layout**

## Troubleshooting Hardware Issues

### Symptom: No I2C communication
**Check:**
- [ ] VCC and GND connected
- [ ] SCL connected to RI
- [ ] SDA connected to DCD
- [ ] Pull-up resistors present (4.7kΩ)
- [ ] Correct I2C addresses
- [ ] No shorts on I2C bus

### Symptom: Intermittent communication
**Check:**
- [ ] Pull-up resistor values (4.7kΩ recommended)
- [ ] Wire length (keep short, <30cm for 100kHz)
- [ ] Capacitance on bus
- [ ] Noise on power supply

### Symptom: Interrupts not working
**Check:**
- [ ] INT pin connected to CTS
- [ ] Pull-up resistor on INT (10kΩ)
- [ ] Pins configured as inputs
- [ ] INT pins from all PCF8574s connected together

### Symptom: Outputs don't work
**Check:**
- [ ] Pin configured as output (write 0)
- [ ] Load within current limits (25mA/pin, 100mA total)
- [ ] Proper ground connection
- [ ] Load polarity (active HIGH vs LOW)

### Symptom: Inputs always read LOW
**Check:**
- [ ] Pin configured as input (write 1 first)
- [ ] External pull-up if needed
- [ ] Input voltage levels compatible
- [ ] Sensor/switch wiring

## Bill of Materials (BOM)

| Component | Quantity | Value/Type | Notes |
|-----------|----------|------------|-------|
| PCF8574 IC | 2 | PCF8574/PCF8574A | Or module |
| Resistor | 2 | 4.7kΩ | I2C pull-ups (SCL, SDA) |
| Resistor | 1 | 10kΩ | INT pull-up |
| Capacitor | 2 | 100nF ceramic | Power decoupling (per IC) |
| Wires | - | 22-26 AWG | Keep short |

**Optional (if using bare ICs):**
| Component | Quantity | Value/Type | Notes |
|-----------|----------|------------|-------|
| PCF8574 IC | 2 | DIP-16 or SOIC-16 | |
| IC Socket | 2 | 16-pin DIP | Optional |
| Resistor | 3 | Per address pins | 10kΩ for A0-A2 |

## Safety Notes

1. **ESD Protection**: PCF8574 is CMOS - handle with ESD precautions
2. **Voltage**: Don't exceed 6V on PCF8574 VCC
3. **Current**: Don't exceed 25mA per pin, 100mA total
4. **Reverse Polarity**: Check VCC/GND before powering on
5. **Hot Plugging**: Not recommended - power off before connecting

## Additional Resources

- PCF8574 Datasheet: Check manufacturer website
- I2C Specification: NXP I2C-bus specification
- M66 Hardware Design: Quectel M66 hardware guide

---

**Created:** 2025-11-17  
**Author:** Hossein Gholami  
**Module:** M66 IO Expander Driver  

