# ESP32-S3 Touch Display Project for JC3248W535

This project is designed to work with the JC3248W535 (AXS15231B, 320×480, QSPI) touch display, powered by the ESP32-S3-N16R8V microcontroller, using LVGL 9.2.2 and Arduino GFX 1.6.0.

## 🎯 Purpose

The system provides a fast and convenient way to run a user interface (UI) created in SquareLine Studio on the ESP32-S3 platform.
For correct integration:

* When creating the SquareLine project, set:
  Project type: CMake / Eclipse / VSCode with SDL (for PC development and simulation).
  LVGL major version: 9.2.2
  Resolution: 320×480
  Color depth: 16-bit
* Enable the “Flat export” option in the SquareLine project settings.
* Use `lvgl.h` as the include path (instead of `lvgl/lvgl.h`).
* Export UI files into the `src/ui/` folder from SquareLine Studio.
* Do not enable multilanguage support.
* After PlatformIO download LVGL library into the project folder::

  ```
  .pio/libdeps/esp32-s3-n16r8v/lvgl/
  ```

  you must:

  1. Rename the file `lv_conf_template.h` → `lv_conf.h`.
  2. Inside `lv_conf.h`, change:

     ```c
     /* clang-format off */
     #if 0 /*Set it to "1" to enable content*/
     ```

     to:

     ```c
     /* clang-format off */
     #if 1 /*Set it to "1" to enable content*/
     ```

## ⚙️ Tech Stack

* Microcontroller: ESP32-S3-N16R8V

  * 240 MHz
  * 16 MB QD Flash
  * 8 MB OPI PSRAM
* Display: JC3248W535 (320×480, QSPI @ 50 MHz)
* Graphics library: LVGL 9.2.2
* Display driver: Arduino GFX Library 1.6.0

