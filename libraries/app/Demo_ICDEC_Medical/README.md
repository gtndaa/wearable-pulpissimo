# Demo ICDEC Medical Watch Face

This directory contains the main application for the ICDEC Medical Watch Face integration on the PULPissimo platform. It demonstrates a complete wearable application integrating a real-time UI with continuous medical sensor processing.

## Overview
The application continuously polls a **MAX30102** Pulse Oximeter and Heart-Rate sensor, processes the raw IR data through a digital filter (IIR/FIR), and displays the calculated average Heart Rate (BPM) alongside a simulated real-time clock on an **SSD1306 OLED** screen.

## Hardware Setup
- **Target Platform:** PULPissimo (FPGA/Simulation)
- **Sensor:** MAX30102 (I2C Address: `0x57`)
- **Display:** SSD1306 OLED (I2C Address: `0x3C`)
- **Wiring:** Both devices share the same I2C bus (`I2C ID 0`).

## Key Features
1. **Dynamic I2C Multiplexing:** Since both devices share the same I2C peripheral (`id = 0`) but require different configurations (100kHz for MAX30102 vs 400kHz for OLED), the application dynamically switches the `i2c_t` configuration block inside the main loop before communicating with each device. This prevents I2C bus collisions and configuration hijacking.
2. **Robust Sensor FIFO Polling:** The main loop uses a nested `while(1)` structure to completely drain the MAX30102's hardware FIFO on every iteration. This guarantees that no samples are missed even if the OLED UI update takes a significant amount of time.
3. **Continuous Filter Pipeline:** The `ppg_hr_process` algorithm is continuously fed with IR samples to maintain strict temporal continuity, preventing "time jumps" from generating massive artifacts in the BPM average.
4. **Auto-Reset (No Finger Detection):** If the raw IR value drops below 50,000, it indicates the user has removed their finger. If this state persists for **3 seconds continuously**, the algorithm triggers a hard reset (`ppg_hr_init`), clearing the filter state and zeroing the BPM display.
5. **Time-based UI Updates:** The OLED display updates exactly once every 1000ms using `pos_tick_get_counter_ms()`, rather than relying on loop iterations.

## How to Build and Run
Make sure you have sourced your PULPissimo SDK environment (e.g., `sourceme.sh`).

1. **Build the Application**
   ```bash
   ./build.sh
   ```
2. **Start OpenOCD (in a separate terminal)**
   ```bash
   ./run_openocd.sh
   ```
3. **Run GDB to load and execute**
   ```bash
   ./run_gdb.sh
   ```
   *Type `c` (continue) in the GDB prompt to start the program.*

## File Structure
- `main.c` : Main application loop, I2C multiplexing, and UI rendering logic.
- `Makefile` : Configuration for the `pulp-rt` build system, linking sensor and GUI libraries.
- `build.sh` : Script to compile the application for the FPGA platform.
- `run_gdb.sh` : Script to flash the compiled binary and debug via GDB.
- `run_openocd.sh` : Script to establish the JTAG connection to the board.
