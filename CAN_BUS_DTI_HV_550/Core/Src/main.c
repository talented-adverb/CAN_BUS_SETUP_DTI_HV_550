/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : CAN message decode + LED blink on STM32
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "CANSPI.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define NEXTION_SEND(ID, val) _Generic((val), \
    int16_t: NEXTION_SendInt, \
    uint16_t: NEXTION_SendInt, \
    int32_t: NEXTION_SendInt, \
    uint32_t: NEXTION_SendInt, \
    float: NEXTION_SendFloat, \
    default: NEXTION_SendString \
)(ID, val)



/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CAN_HandleTypeDef hcan1;

SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
//CAN_RxHeaderTypeDef RxHeader;
//uint8_t RxData[8];
//uint8_t can_msg_received = 0;

char uart_buffer[100];

//1F0F
char test[100];
uint8_t Cmd_End[3] = {0xFF,0xFF,0xFF};  // command end sequence
uint8_t control_mode;
int16_t target_iq_raw;
float target_iq;
uint16_t motor_position_raw;
float motor_position;
uint8_t is_motor_still;

//200F
int32_t erpm;
uint16_t duty_raw;
uint16_t voltage;
float duty;

//210F
uint16_t ac_current_raw;
uint16_t dc_current_raw;
float ac_current;
float dc_current;

//220F
uint16_t ctrl_temp_raw;
uint16_t motor_temp_raw;
uint8_t fault_code;
float ctrl_temp;
float motor_temp;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_CAN1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    return len;
}

void Decode_CAN_Message(CAN_RxHeaderTypeDef *header, uint8_t *data)
{
    switch (header->StdId)
    {
		case 0x1F0F: // General Data 6: Control mode, Target Iq, Motor position, isMotorStill
		{
			control_mode = data[0];

			target_iq_raw = (int16_t)((data[1] << 8) | data[2]);
			target_iq = target_iq_raw / 10.0f;

			motor_position_raw = (data[3] << 8) | data[4];
			motor_position = motor_position_raw / 10.0f;

			is_motor_still = data[5];

			snprintf(uart_buffer,
					 sizeof(uart_buffer),
					 "ID: 0x1F0F | Ctrl Mode: %u | Target Iq: %.1f A | Motor Pos: %.1f deg | Still: %s\r\n",
					 control_mode,
					 target_iq,
					 motor_position,
					 is_motor_still ? "1" : "0");

			HAL_UART_Transmit(&huart2, (uint8_t*)uart_buffer, strlen(uart_buffer), 100);
			break;
		}
        case 0x200F: // ERPM, Duty, Voltage
        {
            erpm = (int32_t)((data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3]);
            duty_raw = (data[4] << 8) | data[5];
            voltage = (data[6] << 8) | data[7];

            duty = duty_raw / 10.0f;

            snprintf(uart_buffer,
                     sizeof(uart_buffer),
                     "ID: 0x200F | ERPM: %ld | Duty: %.1f %% | Voltage: %u V\r\n",
                     erpm, duty, voltage);
            HAL_UART_Transmit(&huart2, (uint8_t*)uart_buffer, strlen(uart_buffer), 100);
            break;
        }

        case 0x210F: // AC Current, DC Current
        {
            ac_current_raw = (data[0] << 8) | data[1];
            dc_current_raw = (data[2] << 8) | data[3];

            ac_current = ac_current_raw * 0.01f;
            dc_current = dc_current_raw * 0.1f;

            snprintf(uart_buffer,
                     sizeof(uart_buffer),
                     "ID: 0x210F | AC Current: %.2f A | DC Current: %.2f A\r\n",
                     ac_current, dc_current);
            HAL_UART_Transmit(&huart2, (uint8_t*)uart_buffer, strlen(uart_buffer), 100);
            break;
        }

        case 0x220F: // Ctrl Temp, Motor Temp, Fault Code
        {
            ctrl_temp_raw = (data[0] << 8) | data[1];
            motor_temp_raw = (data[2] << 8) | data[3];
            fault_code = data[4];

            ctrl_temp = ctrl_temp_raw * 0.1f;
            motor_temp = motor_temp_raw * 0.1f;

            snprintf(uart_buffer,
                     sizeof(uart_buffer),
                     "ID: 0x220F | Ctrl Temp: %.1f °C | Motor Temp: %.1f °C | Fault: 0x%02X\r\n",
                     ctrl_temp, motor_temp, fault_code);
            HAL_UART_Transmit(&huart2, (uint8_t*)uart_buffer, strlen(uart_buffer), 100);
            break;
        }

        default:
        {
            snprintf(uart_buffer,
                     sizeof(uart_buffer),
                     "Unknown CAN ID: 0x%03lX\r\n", (unsigned long)header->StdId);
            HAL_UART_Transmit(&huart2, (uint8_t*)uart_buffer, strlen(uart_buffer), 100);
            break;
        }
    }
}

void NEXTION_SendString(const char *ID, const char *string) {
    char buf[50];
    int len = sprintf(buf, "%s.txt=\"%s\"", ID, string);
    HAL_UART_Transmit(&huart1, (uint8_t *)buf, len, 1000);
    HAL_UART_Transmit(&huart1, (uint8_t *)"\xFF\xFF\xFF", 3, 100);
}

void NEXTION_SendInt(char *ID, int value) {
    char str[20];
    sprintf(str, "%d", value);
    NEXTION_SendString(ID, str);
}

void NEXTION_SendFloat(char *ID, float value) {
    char str[20];
    sprintf(str, "%.2f", value);
    NEXTION_SendString(ID, str);
}

void NEXTION_SendInt32(const char *ID, int32_t value) {
    char buf[50];
    int len = sprintf(buf, "%s.txt=\"%ld\"", ID, (long)value);  // Use %ld for 32-bit int
    HAL_UART_Transmit(&huart1, (uint8_t *)buf, len, 1000);
    HAL_UART_Transmit(&huart1, (uint8_t *)"\xFF\xFF\xFF", 3, 100);
}

void NEXTION_SendUint16(const char *ID, uint16_t value) {
    char buf[50];
    int len = sprintf(buf, "%s.txt=\"%u\"", ID, value);
    HAL_UART_Transmit(&huart1, (uint8_t *)buf, len, 1000);
    HAL_UART_Transmit(&huart1, (uint8_t *)"\xFF\xFF\xFF", 3, 100);
}


//void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
//{
//    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, RxData) == HAL_OK)
//    {
//        Decode_CAN_Message(&RxHeader, RxData);
//        can_msg_received = 1;
//    }
//}
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

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_SPI1_Init();
  MX_USART1_UART_Init();
  MX_CAN1_Init();
  /* USER CODE BEGIN 2 */
  uint16_t readValue;
  uint16_t rxValue;
  uCAN_MSG txMessage;
  uCAN_MSG rxMessage;
//  HAL_ADC_Start(&hadc1);
  CANSPI_Initialize();
  HAL_Delay(1000);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
//	    HAL_ADC_PollForConversion(&hadc1,1000);
//	    readValue = HAL_ADC_GetValue(&hadc1);
//	    txMessage.frame.idType = dSTANDARD_CAN_MSG_ID_2_0B;
//	    txMessage.frame.id = 0x127; // ID can be between Hex1 and Hex7FF (1-2047 decimal)
//	    txMessage.frame.dlc = 8;
//	    txMessage.frame.data0 = 'S';
//	    txMessage.frame.data1 = 'T';
//	    txMessage.frame.data2 = 'M';
//	    txMessage.frame.data3 = '3';
//	    txMessage.frame.data4 = '2';
//	    txMessage.frame.data5 = '-';
//	    txMessage.frame.data6 = readValue & 0xff;
//	    txMessage.frame.data7 = readValue >> 8;
//	    CANSPI_Transmit(&txMessage);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	    if(CANSPI_Receive(&rxMessage))
	    {
//	    	sprintf(test,"Tough%d\n",100);
//	    	HAL_UART_Transmit(&huart2,test,sizeof(test),100);
	    	CAN_RxHeaderTypeDef fakeHeader;
	    	fakeHeader.StdId = rxMessage.frame.id;
	    	Decode_CAN_Message(&fakeHeader, &rxMessage.frame.data0);
	    }
	    HAL_Delay(100);
//	    NEXTION_SEND("t27", voltage);      // uint16_t
	    NEXTION_SEND("t15", erpm / 10);    // int32_t divided, result is int
	    NEXTION_SEND("t24", motor_temp);   // float
		NEXTION_SendString ("t5", "30");
		NEXTION_SendString ("t3", "30");

  }
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 180;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief CAN1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_CAN1_Init(void)
{

  /* USER CODE BEGIN CAN1_Init 0 */

  /* USER CODE END CAN1_Init 0 */

  /* USER CODE BEGIN CAN1_Init 1 */

  /* USER CODE END CAN1_Init 1 */
  hcan1.Instance = CAN1;
  hcan1.Init.Prescaler = 16;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_1TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_1TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  hcan1.Init.AutoBusOff = DISABLE;
  hcan1.Init.AutoWakeUp = DISABLE;
  hcan1.Init.AutoRetransmission = DISABLE;
  hcan1.Init.ReceiveFifoLocked = DISABLE;
  hcan1.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN1_Init 2 */

  /* USER CODE END CAN1_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 9600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(CAN_CS_GPIO_Port, CAN_CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : CAN_CS_Pin */
  GPIO_InitStruct.Pin = CAN_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(CAN_CS_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
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
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
