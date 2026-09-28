# STM32F401 Dual-Actuator Motor Control System

## Overview

This project is an STM32F401-based control system for a small DC gear motor and an SG90 servo motor.

The DC motor is driven through a TB6612FNG dual H-bridge. A 10 kΩ potentiometer provides the motor speed command through the STM32 ADC, while motor direction is controlled through UART commands. The SG90 servo is controlled using timer-based PWM and can be positioned at 0°, 90°, or 180° through the same UART interface.

The firmware was developed using STM32CubeMX, STM32CubeIDE, and the STM32 HAL. A basic state machine is used for motor operation, braking, and fault handling.

The DC motor section is open-loop. No encoder or speed feedback is used.

---

## Main Features

* STM32F401CCU6 firmware
* DC gear motor control using TB6612FNG
* Forward and reverse direction control
* PWM-based DC motor speed control
* 10 kΩ potentiometer connected to ADC
* SG90 servo position control
* Timer-based PWM for motor and servo
* UART command interface using interrupt reception
* Motor operating state machine
* Controlled braking before direction reversal
* Basic ADC fault handling
* UART diagnostics
* Separate source modules for motor, servo, and UART functions

---

## Hardware

| Component                  | Purpose                   |
| -------------------------- | ------------------------- |
| STM32F401CCU6              | Microcontroller           |
| TB6612FNG                  | DC motor driver           |
| Yellow DC gear motor       | Motor actuator            |
| SG90                       | Servo actuator            |
| 10 kΩ potentiometer        | Motor speed command       |
| CP2102                     | USB-to-UART interface     |
| 25 MHz external oscillator | HSE clock source          |
| ST-Link V2                 | Programming and debugging |

### Power and Logic

The STM32 operates with 3.3 V GPIO logic.

The TB6612FNG has separate logic and motor supply inputs. VCC is used for the driver logic supply, while VM supplies the motor side.

The STM32, TB6612FNG, CP2102, and other control circuitry use a common ground reference.

---

## Pin Configuration

### STM32F401 Pin Assignment

| STM32 Pin | Peripheral / Signal | Connected To              |
| --------- | ------------------- | ------------------------- |
| PA0       | TIM2_CH1            | SG90 servo PWM            |
| PA1       | GPIO Output         | TB6612 AIN1               |
| PA2       | GPIO Output         | TB6612 AIN2               |
| PA3       | GPIO Output         | TB6612 STBY               |
| PA6       | TIM3_CH1            | TB6612 PWMA               |
| PB0       | ADC1_IN8            | 10 kΩ potentiometer wiper |
| PA9       | USART1_TX           | CP2102 RX                 |
| PA10      | USART1_RX           | CP2102 TX                 |
| PH0       | HSE                 | 25 MHz oscillator         |
| PH1       | HSE                 | 25 MHz oscillator         |

The potentiometer is connected between 3.3 V and ground, with its wiper connected to PB0.

---

## System Architecture

The STM32F401 acts as the main controller for both actuators.

The potentiometer is read through ADC1. The resulting ADC value is converted into a PWM compare value for TIM3. TIM3 drives the PWMA input of the TB6612FNG, while GPIO signals control the motor direction and standby state.

TIM2 generates the 50 Hz PWM signal used by the SG90 servo.

USART1 provides the connection to a PC through the CP2102 USB-to-UART converter. Received commands are handled by the UART module and used to control the motor or servo.

The application state machine is located in main.c, while motor, servo, and UART-specific functions are separated into their own source modules.

### System Architecture Diagram

Insert the project-specific system architecture diagram here.

### Firmware Structure

| File       | Responsibility                                                       |
| ---------- | -------------------------------------------------------------------- |
| main.c     | Peripheral initialization, ADC processing, application state machine |
| motor.c    | TB6612FNG direction, standby, and braking control                    |
| servo.c    | SG90 PWM position control                                            |
| uart_cmd.c | UART reception, command processing, and diagnostics                  |
| main.h     | Shared handles, state definitions, and declarations                  |
| motor.h    | Motor control function declarations                                  |
| servo.h    | Servo control function declaration                                   |
| uart_cmd.h | UART function declarations                                           |

---

## DC Motor Control

The TB6612FNG controls the DC motor using two direction inputs, a PWM input, and a standby input.

### Direction Logic

| STBY | AIN1 | AIN2 | Motor State        |
| ---: | ---: | ---: | ------------------ |
|    0 |    X |    X | Standby / disabled |
|    1 |    0 |    0 | Coast              |
|    1 |    1 |    0 | Forward            |
|    1 |    0 |    1 | Reverse            |
|    1 |    1 |    1 | Brake              |

The firmware uses the standby input to disable the motor driver during the normal stopped state. This is treated as a driver-disabled state rather than the TB6612FNG coast state.

### PWM

The motor PWM is generated using TIM3_CH1 on PA6.

| Parameter     |  Value |
| ------------- | -----: |
| Timer clock   | 84 MHz |
| Prescaler     |      0 |
| Auto-reload   |   4199 |
| PWM frequency | 20 kHz |

The potentiometer is sampled using the STM32 12-bit ADC.

The ADC produces a value from 0 to 4095.

This value is mapped to the TIM3 compare range of 0 to 4199.

Therefore, the potentiometer determines the motor PWM duty cycle.

The potentiometer controls the speed command only. It does not start the motor or determine its direction.

---

## Direction Changes and Braking

When a direction change is requested while the motor is running, the firmware does not immediately switch the H-bridge direction.

The PWM is first set to zero and the motor driver is placed in the brake state.

The braking interval is approximately 100 ms. The main loop checks the elapsed time using HAL_GetTick().

After the braking interval, the requested direction is applied and the motor returns to the running state.

This is a simple open-loop transition mechanism. There is no measurement of motor speed or rotor position.

### Direction Change Diagram

Insert the project-specific braking and direction-change diagram here.

---

## Servo Control

The SG90 is controlled using TIM2_CH1 on PA0.

The timer is configured for a 50 Hz PWM signal.

| Parameter        |  Value |
| ---------------- | -----: |
| Timer clock      | 84 MHz |
| Prescaler        |     83 |
| Auto-reload      |  19999 |
| PWM frequency    |  50 Hz |
| Timer resolution |   1 µs |

The firmware uses pulse widths between approximately 1 ms and 2 ms.

| Command | Position | Pulse Width |
| ------- | -------: | ----------: |
| 1       |       0° |     1000 µs |
| 2       |      90° |     1500 µs |
| 3       |     180° |     2000 µs |

The servo pulse function limits the requested pulse width to the 1000–2000 µs range.

---

## UART Command Interface

USART1 is configured with the following parameters:

| Parameter    | Value  |
| ------------ | ------ |
| Baud rate    | 115200 |
| Data bits    | 8      |
| Parity       | None   |
| Stop bits    | 1      |
| Flow control | None   |

The CP2102 provides the USB-to-UART connection between the STM32 and a PC.

### Commands

| Command | Function          |
| ------- | ----------------- |
| F / f   | Motor forward     |
| R / r   | Motor reverse     |
| S / s   | Stop motor        |
| B / b   | Brake motor       |
| C / c   | Clear motor fault |
| 1       | Servo 0°          |
| 2       | Servo 90°         |
| 3       | Servo 180°        |

Carriage return and line feed characters generated by pressing Enter are ignored.

Unknown commands are reported through the UART diagnostic output.

---

## Motor State Machine

The motor control logic uses four states:

| State         | Function                                                                         |
| ------------- | -------------------------------------------------------------------------------- |
| MOTOR_IDLE    | Motor disabled and PWM set to zero                                               |
| MOTOR_RUNNING | Motor runs in the selected direction using the potentiometer-derived PWM command |
| MOTOR_BRAKING | PWM is disabled and the TB6612FNG is placed in the brake state                   |
| MOTOR_FAULT   | Motor remains disabled after an ADC-related fault                                |

A normal direction change from forward to reverse, or reverse to forward, first enters MOTOR_BRAKING.

After the configured braking interval, the requested direction is applied and the system returns to MOTOR_RUNNING.

The S command returns the system to MOTOR_IDLE.

The B command explicitly enters the braking state. After the braking interval, the system returns to MOTOR_IDLE.

If an ADC operation fails while the motor is running, the firmware sets the PWM to zero, disables the motor driver, and enters MOTOR_FAULT.

The C command clears the fault and returns the system to MOTOR_IDLE.

### Motor State Diagram

Insert the project-specific motor state diagram here.

---

## Fault Handling

ADC conversion is monitored while the motor is running.

If an ADC operation fails:

1. Motor PWM is set to zero.
2. The motor driver is disabled.
3. The state changes to MOTOR_FAULT.
4. The motor remains disabled while the system is in the fault state.
5. The C command clears the fault and returns the system to MOTOR_IDLE.

A new F or R command is required to start the motor again.

The fault mechanism is intentionally simple and provides a basic firmware-level safety path. It is not a complete motor protection system.

---

## Firmware Structure

### main.c

Responsible for:

* HAL and peripheral initialization
* ADC operation
* PWM startup
* Application state machine
* Motor speed command calculation
* Fault-state handling
* Main application loop

### motor.c

Contains the TB6612FNG control functions:

| Function        | Purpose                           |
| --------------- | --------------------------------- |
| Motor_Stop()    | Disables the motor driver         |
| Motor_Forward() | Sets forward motor direction      |
| Motor_Reverse() | Sets reverse motor direction      |
| Motor_Brake()   | Applies the TB6612FNG brake state |

This keeps the driver-specific GPIO operations separate from the application logic.

### servo.c

Contains Servo_SetPulse().

The function limits the requested pulse width and updates the TIM2 compare register.

### uart_cmd.c

Responsible for:

* UART command interpretation
* UART receive interrupt callback
* Command diagnostics
* Servo command handling
* Motor command handling

---

## Clock Configuration

The STM32F401 uses a 25 MHz external HSE oscillator.

The PLL configuration produces an 84 MHz system clock.

| Parameter |  Value |
| --------- | -----: |
| HSE       | 25 MHz |
| PLLM      |     25 |
| PLLN      |    336 |
| PLLP      |     /4 |
| SYSCLK    | 84 MHz |
| AHB       | 84 MHz |
| APB1      | 42 MHz |
| APB2      | 84 MHz |

Because the APB1 prescaler is greater than 1, the TIM2 and TIM3 timer clocks are 84 MHz.

---

## Testing and Verification

The system was tested on the target hardware using the STM32 board, TB6612FNG, DC gear motor, SG90 servo, potentiometer, CP2102, and ST-Link.

### Tested Functions

| Test                                     | Result                                   |
| ---------------------------------------- | ---------------------------------------- |
| STM32 firmware programming and debugging | Passed                                   |
| Servo 0° command                         | Passed                                   |
| Servo 90° command                        | Passed                                   |
| Servo 180° command                       | Passed                                   |
| Motor forward command                    | Passed                                   |
| Motor reverse command                    | Passed                                   |
| Potentiometer speed control              | Passed                                   |
| Motor stop command                       | Passed                                   |
| Explicit brake command                   | Passed                                   |
| Brake before direction reversal          | Passed                                   |
| UART command reception                   | Passed                                   |
| Unknown command handling                 | Passed                                   |
| UART CR/LF handling                      | Passed                                   |
| Fault-state logic                        | Implemented and tested at firmware level |

---

## Limitations

This project is intentionally limited to basic open-loop actuator control.

Current limitations include:

* No motor encoder
* No RPM measurement
* No closed-loop speed control
* No PID controller
* No position feedback for the DC motor
* No current sensing
* No overcurrent protection
* No hardware emergency-stop input
* No three-phase motor control
* No field-oriented control (FOC)

The SG90 position commands are based on servo pulse width. Actual mechanical position can vary with the servo, load, supply voltage, and mechanical setup.

---

## Possible Future Improvements

Possible extensions include:

* Add an encoder for motor speed feedback
* Implement closed-loop RPM control
* Add current sensing and overcurrent protection
* Add a physical emergency-stop input
* Add ADC filtering for the potentiometer
* Add a more robust UART command parser
* Add CAN or RS-485 communication
* Move the control application to FreeRTOS
* Extend the motor-control section toward three-phase inverter and BLDC/PMSM control

These features are not part of the current implementation.

---

## Development Tools

### Software

* STM32CubeMX
* STM32CubeIDE
* STM32 HAL
* Git / GitHub
* Serial terminal

### Hardware

* STM32F401CCU6
* ST-Link V2
* TB6612FNG
* SG90 servo
* DC gear motor
* 10 kΩ potentiometer
* CP2102 USB-to-UART
* 25 MHz external oscillator

---

## Project Status

**Completed**

The current version has been implemented and tested on the target hardware.

The repository contains the STM32CubeIDE project, CubeMX configuration, application source files, HAL/CMSIS dependencies, and linker configuration.
