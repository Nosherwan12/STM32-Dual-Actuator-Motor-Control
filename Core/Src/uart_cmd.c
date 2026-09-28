#include "uart_cmd.h"
#include "main.h"
#include "motor.h"
#include "servo.h"

#include <string.h>

void UART_SendString(const char *message)
{
    HAL_UART_Transmit(&huart1,
                      (uint8_t *)message,
                      strlen(message),
                      HAL_MAX_DELAY);
}

void UART_SetMessage(const char *message)
{
    uart_message = message;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        switch (rx_byte)
        {
            case 'F':
            case 'f':

                if (motor_state == MOTOR_FAULT)
                {
                    UART_SetMessage("ERROR: Motor is in FAULT. Send C to clear.\r\n");
                    break;
                }

                if (motor_state == MOTOR_RUNNING)
                {
                    if (motor_direction != MOTOR_FORWARD)
                    {
                        pending_direction = MOTOR_FORWARD;
                        brake_start_time = HAL_GetTick();
                        direction_change_pending = 1;

                        motor_state = MOTOR_BRAKING;

                        __HAL_TIM_SET_COMPARE(&htim3,
                                              TIM_CHANNEL_1,
                                              0);

                        Motor_Brake();

                        UART_SetMessage("Motor Direction : FORWARD\r\n");
                    }
                }
                else
                {
                    motor_direction = MOTOR_FORWARD;
                    motor_state = MOTOR_RUNNING;

                    Motor_Forward();

                    UART_SetMessage("Motor FORWARD\r\n");
                }

                break;


            case 'R':
            case 'r':

                if (motor_state == MOTOR_FAULT)
                {
                    UART_SetMessage("ERROR: Motor is in FAULT. Send C to clear.\r\n");
                    break;
                }

                if (motor_state == MOTOR_RUNNING)
                {
                    if (motor_direction != MOTOR_REVERSE)
                    {
                        pending_direction = MOTOR_REVERSE;
                        brake_start_time = HAL_GetTick();
                        direction_change_pending = 1;

                        motor_state = MOTOR_BRAKING;

                        __HAL_TIM_SET_COMPARE(&htim3,
                                              TIM_CHANNEL_1,
                                              0);

                        Motor_Brake();

                        UART_SetMessage("Motor Direction : REVERSE\r\n");
                    }
                }
                else
                {
                    motor_direction = MOTOR_REVERSE;
                    motor_state = MOTOR_RUNNING;

                    Motor_Reverse();

                    UART_SetMessage("Motor REVERSE\r\n");
                }

                break;


            case 'S':
            case 's':

                direction_change_pending = 0;

                __HAL_TIM_SET_COMPARE(&htim3,
                                      TIM_CHANNEL_1,
                                      0);

                Motor_Stop();

                motor_state = MOTOR_IDLE;

                UART_SetMessage("Motor STOPPED\r\n");

                break;


            case 'B':
            case 'b':

                if (motor_state == MOTOR_FAULT)
                {
                    UART_SetMessage("ERROR: Motor is in FAULT. Send C to clear.\r\n");
                    break;
                }

                direction_change_pending = 0;

                __HAL_TIM_SET_COMPARE(&htim3,
                                      TIM_CHANNEL_1,
                                      0);

                Motor_Brake();

                brake_start_time = HAL_GetTick();
                motor_state = MOTOR_BRAKING;

                UART_SetMessage("Motor BRAKING...\r\n");

                break;


            case 'C':
            case 'c':

                if (motor_state == MOTOR_FAULT)
                {
                    motor_state = MOTOR_IDLE;
                    direction_change_pending = 0;

                    UART_SetMessage("FAULT CLEARED. Motor IDLE.\r\n");
                }
                else
                {
                    UART_SetMessage("No active fault.\r\n");
                }

                break;


            case '1':

                Servo_SetPulse(1000);

                UART_SetMessage("Servo: 0 degrees\r\n");

                break;


            case '2':

                Servo_SetPulse(1500);

                UART_SetMessage("Servo: 90 degrees\r\n");

                break;


            case '3':

                Servo_SetPulse(2000);

                UART_SetMessage("Servo: 180 degrees\r\n");

                break;


            case '\r':
            case '\n':

                /* Ignore Enter / newline characters */

                break;


            default:

                UART_SetMessage("ERROR: Unknown command\r\n");

                break;
        }

        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
    }
}
