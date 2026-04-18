# STM32 Weather Station with OLED Display

## Overview

This project is a real-time embedded weather station built using an STM32 Nucleo F446RE microcontroller. It reads environmental data from a BME280 sensor and displays the results on an SSD1306 OLED screen. The system also includes a real-time clock (RTC) and a multi-button interface for user interaction.

---

## Features

* Temperature, humidity, and pressure measurement (BME280)
* Real-time clock display (RTC)
* Multiple display screens:

  * Temperature
  * Humidity
  * Pressure
  * Clock
* Button-controlled navigation
* Time setting mode with blinking active field
* Power (sleep/wake) functionality

---

## Circuit Diagram

<img width="3000" height="2829" alt="circuit_image" src="https://github.com/user-attachments/assets/4a303123-e398-41de-aed3-b8a0f8f81418" />


The system uses a shared I2C bus for both the BME280 sensor and SSD1306 OLED display.
Push buttons are connected with pull-down resistors and handled via external interrupts.

---

## BME280 Sensor Connections

| Pin | STM32 | Description     |
| --- | ----- | --------------- |
| VCC | 3.3V  | Power           |
| GND | GND   | Ground          |
| SCL | PB8   | I2C Clock       |
| SDA | PB9   | I2C Data        |
| CSB | VCC   | I2C mode select |
| SDO | GND   | Address (0x76)  |

---

## OLED Display Connections

| Pin | STM32 | Description |
| --- | ----- | ----------- |
| VCC | 3.3V  | Power       |
| GND | GND   | Ground      |
| SCL | PB8   | I2C Clock   |
| SDA | PB9   | I2C Data    |

---

## Button Mapping

| Function    | Pin | Description                                 |
| ----------- | --- | ------------------------------------------- |
| NEXT / MODE | PA5 | Screen change / long press for time setting |
| UP          | PA1 | Increase value                              |
| DOWN        | PB0 | Decrease value                              |
| POWER       | PA4 | Display ON/OFF                              |

---

## Operating Modes

### Normal Mode

* Switch between screens using NEXT button
* Displays sensor data or clock

### Time Setting Mode

* Enter with long press
* Select field (hour/min/sec)
* Adjust using UP/DOWN
* Active field blinks

---

## Implementation Highlights

* Sensor reading via Bosch BME280 driver
* Interrupt-based button handling (`HAL_GPIO_EXTI_Callback`)
* Screen rendering using `switch-case` structure
* Blinking effect using system tick timing
* OLED reinitialization for power control

---

## File Structure

* `main.c` → Main application logic 
* `bme280.*` → Sensor driver
* `ssd1306.*` → OLED driver

---

## Libraries Used

This project uses the following external libraries:

* **BME280 Sensor API (Bosch Official)**

  * https://github.com/boschsensortec/BME280_SensorAPI

* **SSD1306 OLED Driver (STM32)**

  * https://github.com/afiskon/stm32-ssd1306

These libraries were integrated and adapted for use with STM32 HAL.

---

## Limitations

* RTC is not battery-backed
* UI is text-based only

---

## Possible Improvements

* Add graphical UI (icons, charts)
* Add non-volatile memory for RTC
* Improve power optimization

---

## Conclusion

This project demonstrates a clean embedded system design with sensor integration, interrupt-driven input handling, and structured user interface logic.

---
