/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "motor.h"
#include "servo.h"
#include "uart_cmd.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define MOTOR_BRAKE_TIME_MS    100U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

ADC_HandleTypeDef hadc1;

TIM_HandleTypeDef htim2;

TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

uint32_t adc_value = 0;

uint8_t rx_byte;

MotorState_t motor_state = MOTOR_IDLE;

MotorDirection_t motor_direction = MOTOR_FORWARD;

MotorDirection_t pending_direction = MOTOR_FORWARD;

uint32_t brake_start_time = 0;

/*
 * TRUE  = braking is part of a direction change
 * FALSE = braking was requested as a standalone action
 */
uint8_t direction_change_pending = 0;

/*
 * Shared between UART interrupt and main loop.
 *
 * UART ISR writes the message pointer.
 * Main loop reads and transmits it.
 */
volatile const char *uart_message = NULL;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/

void SystemClock_Config(void);

static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART1_UART_Init(void);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_USART1_UART_Init();

  /* USER CODE BEGIN 2 */

  /*--------------------------------------------------------------
   * TIM2 CH1 = SG90 Servo PWM
   *--------------------------------------------------------------*/
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);

  /* Start servo at center position */
  Servo_SetPulse(1500);


  /*--------------------------------------------------------------
   * TIM3 CH1 = DC Motor PWM
   *--------------------------------------------------------------*/
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);

  /* Start motor PWM at 0% duty cycle */
  __HAL_TIM_SET_COMPARE(
      &htim3,
      TIM_CHANNEL_1,
      0
  );


  /*--------------------------------------------------------------
   * Safe motor startup
   *--------------------------------------------------------------*/
  Motor_Stop();


  /*--------------------------------------------------------------
   * Start UART reception using interrupt
   *--------------------------------------------------------------*/
  HAL_UART_Receive_IT(
      &huart1,
      &rx_byte,
      1
  );

  /* USER CODE END 2 */

  /* Infinite loop */

  /* USER CODE BEGIN WHILE */
  while (1)
  {
      uint32_t ccr_value = 0;


      /*----------------------------------------------------------
       * UART diagnostic message
       *
       * UART interrupt only sets uart_message.
       * The actual blocking transmission happens here,
       * outside the interrupt context.
       *----------------------------------------------------------*/
      if (uart_message != NULL)
      {
          UART_SendString((const char *)uart_message);
          uart_message = NULL;
      }


      /*----------------------------------------------------------
       * Motor state machine
       *----------------------------------------------------------*/
      switch (motor_state)
      {
          /*======================================================
           * IDLE
           *======================================================*/
          case MOTOR_IDLE:

              Motor_Stop();

              __HAL_TIM_SET_COMPARE(
                  &htim3,
                  TIM_CHANNEL_1,
                  0
              );

              break;


          /*======================================================
           * RUNNING
           *======================================================*/
          case MOTOR_RUNNING:

              /*
               * ADC is required only while the motor is running.
               *
               * PB0 -> ADC1_IN8
               *
               * ADC value controls PWM duty cycle.
               */
              if (HAL_ADC_Start(&hadc1) == HAL_OK)
              {
                  if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
                  {
                      adc_value = HAL_ADC_GetValue(&hadc1);

                      /*
                       * Convert ADC range:
                       *
                       * 0    -> PWM 0
                       * 4095 -> PWM 4199
                       */
                      ccr_value =
                          ((uint32_t)adc_value * 4199U) / 4095U;


                      /*
                       * Apply motor direction.
                       */
                      if (motor_direction == MOTOR_FORWARD)
                      {
                          Motor_Forward();
                      }
                      else
                      {
                          Motor_Reverse();
                      }


                      /*
                       * Apply motor speed.
                       */
                      __HAL_TIM_SET_COMPARE(
                          &htim3,
                          TIM_CHANNEL_1,
                          ccr_value
                      );
                  }
                  else
                  {
                      /*
                       * ADC conversion failed.
                       *
                       * Fail-safe action:
                       *
                       * 1. Remove PWM
                       * 2. Disable motor driver
                       * 3. Enter FAULT
                       */
                      __HAL_TIM_SET_COMPARE(
                          &htim3,
                          TIM_CHANNEL_1,
                          0
                      );

                      Motor_Stop();

                      motor_state = MOTOR_FAULT;

                      direction_change_pending = 0;

                      UART_SetMessage(
                          "FAULT: ADC conversion failed\r\n"
                      );
                  }
              }
              else
              {
                  /*
                   * ADC could not start.
                   *
                   * Fail-safe action.
                   */
                  __HAL_TIM_SET_COMPARE(
                      &htim3,
                      TIM_CHANNEL_1,
                      0
                  );

                  Motor_Stop();

                  motor_state = MOTOR_FAULT;

                  direction_change_pending = 0;

                  UART_SetMessage(
                      "FAULT: ADC start failed\r\n"
                  );
              }

              break;


          /*======================================================
           * BRAKING
           *======================================================*/
          case MOTOR_BRAKING:

              /*
               * Remove PWM before applying brake.
               */
              __HAL_TIM_SET_COMPARE(
                  &htim3,
                  TIM_CHANNEL_1,
                  0
              );

              Motor_Brake();


              /*
               * Wait for the defined braking interval.
               */
              if ((HAL_GetTick() - brake_start_time)
                      >= MOTOR_BRAKE_TIME_MS)
              {
                  if (direction_change_pending)
                  {
                      /*
                       * Braking was requested because the
                       * direction needs to change.
                       */
                      motor_direction = pending_direction;

                      direction_change_pending = 0;

                      motor_state = MOTOR_RUNNING;
                  }
                  else
                  {
                      /*
                       * Standalone brake command.
                       *
                       * Do NOT restart the motor.
                       */
                      motor_state = MOTOR_IDLE;
                  }
              }

              break;


          /*======================================================
           * FAULT
           *======================================================*/
          case MOTOR_FAULT:

              /*
               * Keep motor disabled while fault is active.
               */
              Motor_Stop();

              __HAL_TIM_SET_COMPARE(
                  &htim3,
                  TIM_CHANNEL_1,
                  0
              );

              break;


          /*======================================================
           * Unexpected state
           *======================================================*/
          default:

              /*
               * Unknown state.
               *
               * Enter fail-safe condition.
               */
              Motor_Stop();

              __HAL_TIM_SET_COMPARE(
                  &htim3,
                  TIM_CHANNEL_1,
                  0
              );

              motor_state = MOTOR_FAULT;

              direction_change_pending = 0;

              UART_SetMessage(
                  "FAULT: Invalid motor state\r\n"
              );

              break;
      }


      /*
       * Main loop period.
       *
       * This also allows the 100 ms braking interval
       * to be checked periodically.
       */
      HAL_Delay(20);

  }
  /* USER END WHILE */

  /* USER CODE BEGIN 3 */

  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();

  __HAL_PWR_VOLTAGESCALING_CONFIG(
      PWR_REGULATOR_VOLTAGE_SCALE2
  );


  /** Initializes the RCC Oscillators
  */
  RCC_OscInitStruct.OscillatorType =
      RCC_OSCILLATORTYPE_HSE;

  RCC_OscInitStruct.HSEState =
      RCC_HSE_ON;

  RCC_OscInitStruct.PLL.PLLState =
      RCC_PLL_ON;

  RCC_OscInitStruct.PLL.PLLSource =
      RCC_PLLSOURCE_HSE;

  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 4;

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }


  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType =
      RCC_CLOCKTYPE_HCLK |
      RCC_CLOCKTYPE_SYSCLK |
      RCC_CLOCKTYPE_PCLK1 |
      RCC_CLOCKTYPE_PCLK2;

  RCC_ClkInitStruct.SYSCLKSource =
      RCC_SYSCLKSOURCE_PLLCLK;

  RCC_ClkInitStruct.AHBCLKDivider =
      RCC_SYSCLK_DIV1;

  RCC_ClkInitStruct.APB1CLKDivider =
      RCC_HCLK_DIV2;

  RCC_ClkInitStruct.APB2CLKDivider =
      RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(
          &RCC_ClkInitStruct,
          FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }


  /** Enables the Clock Security System
  */
  HAL_RCC_EnableCSS();
}


/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};


  /** Configure the global features of the ADC
  */
  hadc1.Instance = ADC1;

  hadc1.Init.ClockPrescaler =
      ADC_CLOCK_SYNC_PCLK_DIV4;

  hadc1.Init.Resolution =
      ADC_RESOLUTION_12B;

  hadc1.Init.ScanConvMode =
      DISABLE;

  hadc1.Init.ContinuousConvMode =
      DISABLE;

  hadc1.Init.DiscontinuousConvMode =
      DISABLE;

  hadc1.Init.ExternalTrigConvEdge =
      ADC_EXTERNALTRIGCONVEDGE_NONE;

  hadc1.Init.ExternalTrigConv =
      ADC_SOFTWARE_START;

  hadc1.Init.DataAlign =
      ADC_DATAALIGN_RIGHT;

  hadc1.Init.NbrOfConversion =
      1;

  hadc1.Init.DMAContinuousRequests =
      DISABLE;

  hadc1.Init.EOCSelection =
      ADC_EOC_SINGLE_CONV;

  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }


  /** Configure ADC channel
  */
  sConfig.Channel =
      ADC_CHANNEL_8;

  sConfig.Rank =
      1;

  sConfig.SamplingTime =
      ADC_SAMPLETIME_84CYCLES;

  if (HAL_ADC_ConfigChannel(
          &hadc1,
          &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
}


/**
  * @brief TIM2 Initialization Function
  * @retval None
  *
  * TIM2_CH1 / PA0 = Servo PWM
  */
static void MX_TIM2_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};


  htim2.Instance = TIM2;


  /*
   * TIM2 clock = 84 MHz
   *
   * 84 MHz / (83 + 1) = 1 MHz
   *
   * 1 timer count = 1 us
   */
  htim2.Init.Prescaler = 83;

  htim2.Init.CounterMode =
      TIM_COUNTERMODE_UP;

  htim2.Init.Period =
      19999;

  htim2.Init.ClockDivision =
      TIM_CLOCKDIVISION_DIV2;

  htim2.Init.AutoReloadPreload =
      TIM_AUTORELOAD_PRELOAD_ENABLE;

  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }


  sClockSourceConfig.ClockSource =
      TIM_CLOCKSOURCE_INTERNAL;

  if (HAL_TIM_ConfigClockSource(
          &htim2,
          &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }


  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }


  sMasterConfig.MasterOutputTrigger =
      TIM_TRGO_RESET;

  sMasterConfig.MasterSlaveMode =
      TIM_MASTERSLAVEMODE_DISABLE;

  if (HAL_TIMEx_MasterConfigSynchronization(
          &htim2,
          &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }


  sConfigOC.OCMode =
      TIM_OCMODE_PWM1;

  /* Initial servo pulse = 1 ms */
  sConfigOC.Pulse =
      1000;

  sConfigOC.OCPolarity =
      TIM_OCPOLARITY_HIGH;

  sConfigOC.OCFastMode =
      TIM_OCFAST_DISABLE;

  if (HAL_TIM_PWM_ConfigChannel(
          &htim2,
          &sConfigOC,
          TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_TIM_MspPostInit(&htim2);
}


/**
  * @brief TIM3 Initialization Function
  * @retval None
  *
  * TIM3_CH1 / PA6 = DC motor PWM
  */
static void MX_TIM3_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};


  htim3.Instance = TIM3;


  /*
   * TIM3 clock = 84 MHz
   *
   * Prescaler = 0
   * ARR = 4199
   *
   * PWM frequency:
   *
   * 84 MHz / 4200 = 20 kHz
   */
  htim3.Init.Prescaler =
      0;

  htim3.Init.CounterMode =
      TIM_COUNTERMODE_UP;

  htim3.Init.Period =
      4199;

  htim3.Init.ClockDivision =
      TIM_CLOCKDIVISION_DIV2;

  htim3.Init.AutoReloadPreload =
      TIM_AUTORELOAD_PRELOAD_ENABLE;

  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }


  sClockSourceConfig.ClockSource =
      TIM_CLOCKSOURCE_INTERNAL;

  if (HAL_TIM_ConfigClockSource(
          &htim3,
          &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }


  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }


  sMasterConfig.MasterOutputTrigger =
      TIM_TRGO_RESET;

  sMasterConfig.MasterSlaveMode =
      TIM_MASTERSLAVEMODE_DISABLE;

  if (HAL_TIMEx_MasterConfigSynchronization(
          &htim3,
          &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }


  sConfigOC.OCMode =
      TIM_OCMODE_PWM1;

  /* Start motor PWM at 0% duty */
  sConfigOC.Pulse =
      0;

  sConfigOC.OCPolarity =
      TIM_OCPOLARITY_HIGH;

  sConfigOC.OCFastMode =
      TIM_OCFAST_DISABLE;

  if (HAL_TIM_PWM_ConfigChannel(
          &htim3,
          &sConfigOC,
          TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_TIM_MspPostInit(&htim3);
}


/**
  * @brief USART1 Initialization Function
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{
  huart1.Instance =
      USART1;

  huart1.Init.BaudRate =
      115200;

  huart1.Init.WordLength =
      UART_WORDLENGTH_8B;

  huart1.Init.StopBits =
      UART_STOPBITS_1;

  huart1.Init.Parity =
      UART_PARITY_NONE;

  huart1.Init.Mode =
      UART_MODE_TX_RX;

  huart1.Init.HwFlowCtl =
      UART_HWCONTROL_NONE;

  huart1.Init.OverSampling =
      UART_OVERSAMPLING_16;

  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
}


/**
  * @brief GPIO Initialization Function
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};


  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();


  /* Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(
      GPIOA,
      GPIO_PIN_1 |
      GPIO_PIN_2 |
      GPIO_PIN_3,
      GPIO_PIN_RESET
  );


  /* Configure GPIO pins: PA1 PA2 PA3 */
  GPIO_InitStruct.Pin =
      GPIO_PIN_1 |
      GPIO_PIN_2 |
      GPIO_PIN_3;

  GPIO_InitStruct.Mode =
      GPIO_MODE_OUTPUT_PP;

  GPIO_InitStruct.Pull =
      GPIO_NOPULL;

  GPIO_InitStruct.Speed =
      GPIO_SPEED_FREQ_LOW;

  HAL_GPIO_Init(
      GPIOA,
      &GPIO_InitStruct
  );
}


/* USER CODE BEGIN 4 */

/* USER CODE END 4 */


/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */

  __disable_irq();

  while (1)
  {
  }

  /* USER CODE END Error_Handler_Debug */
}


#ifdef USE_FULL_ASSERT

void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */

  /* User can add implementation here */

  /* USER CODE END 6 */
}

#endif /* USE_FULL_ASSERT */
