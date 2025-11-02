# M66 Firmware Documentation

**Author**: Hossein Gholami  
**Date**: 2025-11-01  
**Version**: 2.0

---

## 📚 Documentation Library

All project documentation organized in one place.

---

## 🚀 Start Here!

### For First-Time Users

1. **[../README.md](../README.md)** - Project overview (read this first!)
2. **[QUICK_START.md](QUICK_START.md)** - Get started in 5 minutes
3. **[DOCUMENTATION_INDEX.md](DOCUMENTATION_INDEX.md)** - Complete navigation guide

### For Finding Answers

**[DOCUMENTATION_INDEX.md](DOCUMENTATION_INDEX.md)** - Your map to all documentation!

---

## 📖 Custom Documentation (Your Firmware)

### Core Guides

| Document | Description | Level |
|----------|-------------|-------|
| [QUICK_START.md](QUICK_START.md) | 5-minute quick start, API reference | ⭐ Beginner |
| [NVRAM_MODULE_GUIDE.md](NVRAM_MODULE_GUIDE.md) | Parameter system complete guide | ⭐⭐ Intermediate |
| [GPIO_MODULE_GUIDE.md](GPIO_MODULE_GUIDE.md) | GPIO module with callbacks | ⭐⭐ Intermediate |
| [PERSISTENCE_STRATEGY.md](PERSISTENCE_STRATEGY.md) | Design rationale and patterns | ⭐⭐⭐ Advanced |

### Additional Resources

| Document | Description |
|----------|-------------|
| [DOCUMENTATION_INDEX.md](DOCUMENTATION_INDEX.md) | Navigation guide, reading paths, quick links |
| [PROJECT_SUMMARY.md](PROJECT_SUMMARY.md) | Complete project status & metrics |
| [README_REFACTORING.md](README_REFACTORING.md) | Migration history & architectural changes |

---

## 📄 Quectel Official Documentation (PDFs)

### Getting Started

| Document | Description |
|----------|-------------|
| [Quectel_OpenCPU_Quick_Start_Application_Note_V1.1.pdf](Quectel_OpenCPU_Quick_Start_Application_Note_V1.1.pdf) | Quick start guide for OpenCPU |
| [Quectel_OpenCPU_GCC_Installation_Guide_V1.1.pdf](Quectel_OpenCPU_GCC_Installation_Guide_V1.1.pdf) | GCC toolchain setup |
| [Quectel_QFlash_OpenCPU_User_Guide_V1.0.pdf](Quectel_QFlash_OpenCPU_User_Guide_V1.0.pdf) | How to flash firmware |

### M66 Hardware & AT Commands

| Document | Description |
|----------|-------------|
| [M66/Quectel_M66-OpenCPU_Hardware_Design_V1.1.pdf](M66/Quectel_M66-OpenCPU_Hardware_Design_V1.1.pdf) | Hardware specifications & pin definitions |
| [M66/Quectel_M66-OpenCPU_User_Guide_V1.1.pdf](M66/Quectel_M66-OpenCPU_User_Guide_V1.1.pdf) | Complete SDK API reference |
| [M66/Quectel_M66_Series_AT_Commands_Manual_V2.1.pdf](M66/Quectel_M66_Series_AT_Commands_Manual_V2.1.pdf) | AT command reference |
| [M66/Quectel_M66-OpenCPU_Solution_Presentation_V1.0.pdf](M66/Quectel_M66-OpenCPU_Solution_Presentation_V1.0.pdf) | Solution overview |

### Application Notes

| Document | Description |
|----------|-------------|
| [OpenCPU_RIL_Application_Note_V1.1.pdf](OpenCPU_RIL_Application_Note_V1.1.pdf) | RIL (Radio Interface Layer) guide |
| [Quectel_OpenCPU_FOTA_Application_Note_V1.0.pdf](Quectel_OpenCPU_FOTA_Application_Note_V1.0.pdf) | Firmware Over-The-Air updates |
| [Quectel_OpenCPU_Watchdog_Application_Note_V1.0.pdf](Quectel_OpenCPU_Watchdog_Application_Note_V1.0.pdf) | Watchdog usage |
| [Quectel_OpenCPU_Security_Data_Application_Note_V1.0.pdf](Quectel_OpenCPU_Security_Data_Application_Note_V1.0.pdf) | Secure data storage |
| [Quectel_GSM_BT_Application_Note_V1.2.pdf](Quectel_GSM_BT_Application_Note_V1.2.pdf) | GSM & Bluetooth |
| [Quectel_RF_Layout_Application_Note_V2.1.pdf](Quectel_RF_Layout_Application_Note_V2.1.pdf) | RF design guidelines |

---

## 🎯 Quick Navigation

### I Want To...

| Task | Go To |
|------|-------|
| **Get started quickly** | [QUICK_START.md](QUICK_START.md) |
| **Understand the system** | [../README.md](../README.md) |
| **Find specific info** | [DOCUMENTATION_INDEX.md](DOCUMENTATION_INDEX.md) |
| **Learn about parameters** | [NVRAM_MODULE_GUIDE.md](NVRAM_MODULE_GUIDE.md) |
| **Configure GPIOs** | [GPIO_MODULE_GUIDE.md](GPIO_MODULE_GUIDE.md) |
| **Understand design choices** | [PERSISTENCE_STRATEGY.md](PERSISTENCE_STRATEGY.md) |
| **Check project status** | [PROJECT_SUMMARY.md](PROJECT_SUMMARY.md) |
| **See hardware specs** | [M66/Quectel_M66-OpenCPU_Hardware_Design_V1.1.pdf](M66/Quectel_M66-OpenCPU_Hardware_Design_V1.1.pdf) |
| **Flash firmware** | [Quectel_QFlash_OpenCPU_User_Guide_V1.0.pdf](Quectel_QFlash_OpenCPU_User_Guide_V1.0.pdf) |

---

## 📊 Documentation Structure

```
docs/
│
├── README.md (This file)              # Documentation index
│
├── Custom Documentation (Markdown)    # Your firmware guides
│   ├── QUICK_START.md                 # ⚡ Start here!
│   ├── DOCUMENTATION_INDEX.md         # 📚 Navigation guide
│   ├── NVRAM_MODULE_GUIDE.md          # 💾 Parameter system
│   ├── GPIO_MODULE_GUIDE.md           # 🎛️ GPIO control
│   ├── PERSISTENCE_STRATEGY.md        # 🎯 Design rationale
│   ├── PROJECT_SUMMARY.md             # 🎉 Status & metrics
│   └── README_REFACTORING.md          # 🔄 Migration history
│
├── Quectel PDFs (Official Docs)       # SDK documentation
│   ├── Getting Started Guides
│   ├── Hardware Manuals
│   ├── API References
│   └── Application Notes
│
└── M66/                               # M66-specific documents
    ├── Hardware Design Manual
    ├── User Guide
    ├── AT Commands Manual
    └── Solution Presentation
```

---

## 🎓 Recommended Reading Order

### New to M66 OpenCPU?

```
Day 1:
1. ../README.md - Project overview (10 min)
2. QUICK_START.md - Get hands-on (15 min)
3. M66/Quectel_M66-OpenCPU_User_Guide_V1.1.pdf - SDK basics (skim)

Day 2:
4. NVRAM_MODULE_GUIDE.md - Parameter system (30 min)
5. GPIO_MODULE_GUIDE.md - GPIO control (30 min)
6. Experiment with examples

Day 3+:
7. PERSISTENCE_STRATEGY.md - Deep understanding
8. Application Notes (as needed)
```

### Experienced Embedded Developer?

```
Hour 1:
- ../README.md + QUICK_START.md (fast overview)
- Build and run firmware
- Review custom/main.c

Hour 2:
- Skim NVRAM_MODULE_GUIDE.md (focus on API)
- Skim GPIO_MODULE_GUIDE.md (focus on config)
- Start customizing

As Needed:
- Quectel PDFs for SDK specifics
- Module guides for troubleshooting
```

---

## 🔍 Search Tips

### Finding Information

1. **Start with** [DOCUMENTATION_INDEX.md](DOCUMENTATION_INDEX.md)
2. **Use Ctrl+F** to search within documents
3. **Check Quick Links** tables for common topics
4. **Read file headers** for document purpose

### Common Topics

| Topic | Document | Section |
|-------|----------|---------|
| Building firmware | [QUICK_START.md](QUICK_START.md) | Section 2 |
| Adding parameters | [QUICK_START.md](QUICK_START.md) | Section 2 ("Add a New Parameter") |
| GPIO configuration | [GPIO_MODULE_GUIDE.md](GPIO_MODULE_GUIDE.md) | Section 3 |
| Thread safety | [NVRAM_MODULE_GUIDE.md](NVRAM_MODULE_GUIDE.md) | Section 3 |
| Callbacks | [PERSISTENCE_STRATEGY.md](PERSISTENCE_STRATEGY.md) | Throughout |
| Pin definitions | [M66/Quectel_M66-OpenCPU_Hardware_Design_V1.1.pdf](M66/Quectel_M66-OpenCPU_Hardware_Design_V1.1.pdf) | Pin mapping tables |

---

## 📞 Support Resources

### Custom Firmware Questions
- Check [DOCUMENTATION_INDEX.md](DOCUMENTATION_INDEX.md) first
- Review [QUICK_START.md](QUICK_START.md) troubleshooting section
- Examine example code in `custom/main.c`

### Quectel SDK Questions
- Refer to official PDF documentation
- Check M66 User Guide for API details
- Review Application Notes for specific features

### Hardware Questions
- See M66 Hardware Design Manual
- Check RF Layout Application Note
- Review pin definitions and specifications

---

## 🎉 Documentation Quality

This project includes:

- ✅ **~3,400+ lines** of custom documentation
- ✅ **7 comprehensive guides** covering all modules
- ✅ **Working code examples** in every guide
- ✅ **Progressive learning paths** for all skill levels
- ✅ **Quick reference tables** for fast lookup
- ✅ **Troubleshooting sections** for common issues
- ✅ **Professional Quectel PDFs** for SDK reference

**Documentation/Code Ratio: 1.28:1** (Excellent!)

---

## 📝 Contributing to Documentation

When updating documentation:

1. Keep consistent formatting and style
2. Update [DOCUMENTATION_INDEX.md](DOCUMENTATION_INDEX.md) if adding new docs
3. Add cross-references to related documents
4. Include working code examples
5. Update version numbers and dates
6. Test all markdown links

---

## 🚀 Ready to Build!

**Start with**: [../README.md](../README.md) or [QUICK_START.md](QUICK_START.md)

**Need help?**: Check [DOCUMENTATION_INDEX.md](DOCUMENTATION_INDEX.md)

**Have fun building amazing IoT applications!** 🎊

---

**Organized by Hossein Gholami - November 2025**

