#include "servo.h"
#include "main.h"

void Servo_SetPulse(uint16_t pulse_us)
{
    if (pulse_us < 1000U)
    {
        pulse_us = 1000U;
    }

    if (pulse_us > 2000U)
    {
        pulse_us = 2000U;
    }

    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pulse_us);
}
