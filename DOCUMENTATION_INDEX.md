# M66 Firmware Documentation Index

**Author**: Hossein Gholami  
**Date**: 2025-11-01  
**Version**: 2.0

---

## 📚 Complete Documentation Library

All documentation organized by purpose and complexity level.

---

## 🚀 Getting Started (Start Here!)

### For Beginners

1. **[README.md](README.md)** ⭐ **START HERE**
   - Overview of the entire project
   - Architecture diagram
   - Feature list
   - Quick links to all guides
   - **Time**: 10 minutes

2. **[QUICK_START.md](QUICK_START.md)** ⚡ **HANDS-ON GUIDE**
   - 5-minute quick start
   - Common use cases with code examples
   - API quick reference
   - Troubleshooting tips
   - **Time**: 15 minutes

### For Existing Users

3. **[README_REFACTORING.md](README_REFACTORING.md)** 🔄 **MIGRATION GUIDE**
   - History of architectural changes
   - Why we moved to modular design
   - Before/after comparisons
   - **Time**: 5 minutes

---

## 📖 Module-Specific Guides (Deep Dives)

### Parameter System

4. **[NVRAM_MODULE_GUIDE.md](NVRAM_MODULE_GUIDE.md)** 💾 **COMPLETE REFERENCE**
   - Full API documentation
   - Thread-safety details
   - Performance benchmarks
   - RAM optimization strategies
   - Advanced features
   - **Time**: 30 minutes
   - **Complexity**: Medium

5. **[PERSISTENCE_STRATEGY.md](PERSISTENCE_STRATEGY.md)** 🎯 **DESIGN DEEP-DIVE**
   - Why callbacks vs polling?
   - Deferred write strategy explained
   - When to call `param_commit()`
   - `persist` vs `dirty` flags
   - Performance comparison tables
   - **Time**: 20 minutes
   - **Complexity**: Medium-Advanced

### GPIO Module

6. **[GPIO_MODULE_GUIDE.md](GPIO_MODULE_GUIDE.md)** ⚡ **COMPLETE REFERENCE**
   - Table-driven configuration
   - Callback system explained
   - EINT setup and debouncing
   - Multi-relay control examples
   - Performance metrics
   - Troubleshooting guide
   - **Time**: 30 minutes
   - **Complexity**: Medium

---

## 📊 Documentation by Purpose

### I Want To...

#### **...Understand the Overall System**
→ Read [README.md](README.md) (10 min)

#### **...Get Started Quickly**
→ Follow [QUICK_START.md](QUICK_START.md) (15 min)

#### **...Add a New GPIO**
→ Check [GPIO_MODULE_GUIDE.md](GPIO_MODULE_GUIDE.md) Section 3: "GPIO Configuration Table"

#### **...Add a New Parameter**
→ Check [QUICK_START.md](QUICK_START.md) Section: "Add a New Parameter"

#### **...Make GPIO Respond to Parameter Changes**
→ Read [GPIO_MODULE_GUIDE.md](GPIO_MODULE_GUIDE.md) Section 5: "Behind the Scenes: How Callbacks Work"

#### **...Understand Why Not Use Polling**
→ Read [PERSISTENCE_STRATEGY.md](PERSISTENCE_STRATEGY.md) Section: "Why This Design? Performance & Flash Wear"

#### **...Save Configuration to NVRAM**
→ Check [NVRAM_MODULE_GUIDE.md](NVRAM_MODULE_GUIDE.md) Section: "Saving and Loading Parameters"

#### **...Control Relay via SMS/MQTT**
→ Check [QUICK_START.md](QUICK_START.md) Section: "Common Patterns"

#### **...Optimize RAM Usage**
→ Read [NVRAM_MODULE_GUIDE.md](NVRAM_MODULE_GUIDE.md) Section: "RAM Optimization Strategy"

#### **...Make My Code Thread-Safe**
→ Read [NVRAM_MODULE_GUIDE.md](NVRAM_MODULE_GUIDE.md) Section: "Thread Safety"

#### **...Migrate from Old Code**
→ Read [README_REFACTORING.md](README_REFACTORING.md)

---

## 📋 Documentation by Complexity

### ⭐ Beginner (Start Here)
1. [README.md](README.md) - Project overview
2. [QUICK_START.md](QUICK_START.md) - Hands-on examples

### ⭐⭐ Intermediate
3. [GPIO_MODULE_GUIDE.md](GPIO_MODULE_GUIDE.md) - GPIO deep-dive
4. [NVRAM_MODULE_GUIDE.md](NVRAM_MODULE_GUIDE.md) - Parameter system

### ⭐⭐⭐ Advanced
5. [PERSISTENCE_STRATEGY.md](PERSISTENCE_STRATEGY.md) - Design rationale
6. [README_REFACTORING.md](README_REFACTORING.md) - Architecture evolution

---

## 🎓 Recommended Reading Order

### Path 1: New Developer (Never Used M66)

```
Day 1:
1. README.md (Overview)
2. QUICK_START.md (Get it working)
3. Build and flash to hardware

Day 2:
4. NVRAM_MODULE_GUIDE.md (Understand parameters)
5. Experiment with adding parameters

Day 3:
6. GPIO_MODULE_GUIDE.md (Master GPIO control)
7. Add GPIOs for your hardware

Day 4:
8. PERSISTENCE_STRATEGY.md (Deep understanding)
9. Optimize your application
```

### Path 2: Experienced Embedded Developer

```
Hour 1:
1. README.md (15 min) - Architecture overview
2. QUICK_START.md (15 min) - API reference
3. Build firmware (10 min)
4. Review custom/main.c (20 min)

Hour 2:
5. Skim NVRAM_MODULE_GUIDE.md (focus on API)
6. Skim GPIO_MODULE_GUIDE.md (focus on config table)
7. Start customizing for your application

As Needed:
8. PERSISTENCE_STRATEGY.md (when optimizing)
9. Module-specific sections (when troubleshooting)
```

### Path 3: Migrating Existing Code

```
Step 1: Read README_REFACTORING.md (understand changes)
Step 2: Read QUICK_START.md (new API patterns)
Step 3: Migrate parameters to new enum-based system
Step 4: Migrate GPIO control to table-driven config
Step 5: Test and verify
```

---

## 🔍 Quick Reference Tables

### API Quick Links

| What You Want | Where to Find It |
|---------------|------------------|
| Parameter API | [NVRAM_MODULE_GUIDE.md](NVRAM_MODULE_GUIDE.md) - Section 4 |
| GPIO API | [GPIO_MODULE_GUIDE.md](GPIO_MODULE_GUIDE.md) - Section 11 |
| Build Instructions | [QUICK_START.md](QUICK_START.md) - Section 2 |
| Initialization Sequence | [QUICK_START.md](QUICK_START.md) - Section 10 |
| Callback Registration | [GPIO_MODULE_GUIDE.md](GPIO_MODULE_GUIDE.md) - Section 5 |

### Common Code Patterns

| Pattern | Where to Find Example |
|---------|----------------------|
| Remote relay control | [QUICK_START.md](QUICK_START.md) - Pattern 1 |
| Inter-task communication | [QUICK_START.md](QUICK_START.md) - Pattern 2 |
| Cloud configuration update | [QUICK_START.md](QUICK_START.md) - Pattern 3 |
| Multi-relay control | [GPIO_MODULE_GUIDE.md](GPIO_MODULE_GUIDE.md) - Section 8 |
| Button with callback | [GPIO_MODULE_GUIDE.md](GPIO_MODULE_GUIDE.md) - Section 9 |

### Troubleshooting Index

| Problem | Solution Location |
|---------|------------------|
| Build errors | [QUICK_START.md](QUICK_START.md) - Section 11 |
| GPIO not responding | [GPIO_MODULE_GUIDE.md](GPIO_MODULE_GUIDE.md) - Section 12 |
| Parameter not persisting | [NVRAM_MODULE_GUIDE.md](NVRAM_MODULE_GUIDE.md) - Section 6 |
| Thread-safety issues | [NVRAM_MODULE_GUIDE.md](NVRAM_MODULE_GUIDE.md) - Section 3 |
| EINT not triggering | [GPIO_MODULE_GUIDE.md](GPIO_MODULE_GUIDE.md) - Section 12 |

---

## 📚 External Resources

### Quectel Documentation (Required Reading)

1. **M66 Hardware Design Manual**
   - Pin definitions
   - Electrical specifications
   - Recommended PCB layout

2. **M66 OpenCPU SDK User Guide**
   - SDK API reference
   - RTOS functions
   - Peripheral drivers

3. **M66 AT Command Manual**
   - Network commands
   - SMS commands
   - GPS commands

### Helpful Background Reading

- ARM Cortex-M architecture
- FreeRTOS fundamentals
- Embedded C best practices
- Flash memory wear leveling

---

## 🔄 Document Update History

| Date | Version | Changes |
|------|---------|---------|
| 2025-11-01 | 2.0 | Added GPIO module documentation |
| 2025-11-01 | 1.5 | Added PERSISTENCE_STRATEGY.md |
| 2025-11-01 | 1.0 | Initial modular documentation |

---

## 💡 Documentation Best Practices

### When Reading Documentation

1. **Start broad** (README.md) then drill down
2. **Skim first**, then read details as needed
3. **Try code examples** while reading
4. **Keep QUICK_START.md** open for API reference
5. **Use Ctrl+F** to search within documents

### When Adding Features

1. **Check existing patterns** in guides
2. **Follow examples** from documentation
3. **Update documentation** with your changes
4. **Add troubleshooting** if you hit issues

---

## 📞 Getting Help

### Before Asking for Help

1. ✅ Read relevant documentation section
2. ✅ Check [QUICK_START.md](QUICK_START.md) troubleshooting
3. ✅ Review example code in `custom/main.c`
4. ✅ Check `build\gcc\build.log` for errors
5. ✅ Try `APP_DEBUG()` to trace execution

### When Reporting Issues

Include:
- Which guide you were following
- What you tried to do
- What actually happened
- Relevant code snippet
- Build log if applicable

---

## 🎯 Documentation Goals

This documentation aims to be:

- ✅ **Complete** - Cover all features
- ✅ **Clear** - Simple language, good examples
- ✅ **Practical** - Real-world use cases
- ✅ **Progressive** - Start simple, go deep
- ✅ **Searchable** - Good structure, clear headings

---

## 📝 Contributing to Documentation

If you improve the documentation:

1. Keep the same format and style
2. Update this index if adding new documents
3. Add examples where helpful
4. Update version numbers
5. Add to "Document Update History"

---

## 🎓 Learning Outcomes

After reading all documentation, you should be able to:

- ✅ Understand the modular architecture
- ✅ Add new parameters and GPIOs
- ✅ Use callbacks for automatic control
- ✅ Implement thread-safe inter-task communication
- ✅ Optimize RAM and flash usage
- ✅ Build and deploy to M66 hardware
- ✅ Troubleshoot common issues
- ✅ Extend the system with new modules

---

**Total Reading Time**: ~2 hours for complete understanding  
**Practical Experience**: ~4-8 hours for first application

---

## 🚀 Ready to Start?

**Absolute Beginners**: Start with [README.md](README.md)  
**Want to Code Now**: Jump to [QUICK_START.md](QUICK_START.md)  
**Deep Understanding**: Read all guides in order

**Built with ❤️ by Hossein Gholami - November 2025**

---

## Quick Navigation

| Document | Purpose | Time | Level |
|----------|---------|------|-------|
| [README.md](README.md) | Overview | 10 min | ⭐ |
| [QUICK_START.md](QUICK_START.md) | Hands-on | 15 min | ⭐ |
| [NVRAM_MODULE_GUIDE.md](NVRAM_MODULE_GUIDE.md) | Parameters | 30 min | ⭐⭐ |
| [GPIO_MODULE_GUIDE.md](GPIO_MODULE_GUIDE.md) | GPIO | 30 min | ⭐⭐ |
| [PERSISTENCE_STRATEGY.md](PERSISTENCE_STRATEGY.md) | Design | 20 min | ⭐⭐⭐ |
| [README_REFACTORING.md](README_REFACTORING.md) | Migration | 5 min | ⭐⭐ |

**Happy coding! 🎉**

