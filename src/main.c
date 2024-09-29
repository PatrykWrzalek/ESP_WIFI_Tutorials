#include "esp_common.h"
#include "freertos/task.h"
#include "gpio.h"
#include "uart.h"

// put definition here:
#define MAX_SSID_LENGTH 32 // Maksymalna długość SSID (nazwy sieci)
#define MAX_NETWORKS 20    // Maksymalna ilość sieci (wyszukiwanych)

// put function declarations here:

/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABCCC
 *                A : rf cal
 *                B : rf init data
 *                C : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
 *******************************************************************************/
uint32 user_rf_cal_sector_set(void)
{
    flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;
    switch (size_map)
    {
    case FLASH_SIZE_4M_MAP_256_256:
        rf_cal_sec = 128 - 5;
        break;

    case FLASH_SIZE_8M_MAP_512_512:
        rf_cal_sec = 256 - 5;
        break;

    case FLASH_SIZE_16M_MAP_512_512:
    case FLASH_SIZE_16M_MAP_1024_1024:
        rf_cal_sec = 512 - 5;
        break;

    case FLASH_SIZE_32M_MAP_512_512:
    case FLASH_SIZE_32M_MAP_1024_1024:
        rf_cal_sec = 1024 - 5;
        break;

    default:
        rf_cal_sec = 0;
        break;
    }

    return rf_cal_sec;
}

/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here.
 * Users can use tasks with priorities from 1 to 9
 * (priority of the freeRTOS timer is 2).
 * Parameters   : none
 * Returns      : none
 *******************************************************************************/
void user_init(void) // there is a main() function
{
    // Inicjalizacja GPIO
    GPIO_ConfigTypeDef io_conf;
    io_conf.GPIO_Pin = ((1 << 2));
    io_conf.GPIO_Mode = GPIO_Mode_Output;
    io_conf.GPIO_Pullup = GPIO_PullUp_DIS;
    io_conf.GPIO_IntrType = GPIO_PIN_INTR_DISABLE;

    gpio_config(&io_conf); // Wgranie konfiguracji GPIO

    UART_WaitTxFifoEmpty(UART0);

    // Inicjalizacja UART0 do wyjścia
    UART_ConfigTypeDef uart0_conf;
    uart0_conf.baud_rate = BIT_RATE_115200;
    uart0_conf.data_bits = UART_WordLength_8b;
    uart0_conf.parity = PARITY_DIS;
    uart0_conf.stop_bits = USART_StopBits_1;
    uart0_conf.flow_ctrl = USART_HardwareFlowControl_None;

    UART_ParamConfig(UART0, &uart0_conf); // Wgranie konfiguracji UART0
    UART_SetPrintPort(UART0);             // Wybranie uart do komunikacji przez funkcję os_printf
    os_printf("Inicjalizacja...\n");

    while (1)
    {
        GPIO_OUTPUT_SET(2, 0);
        vTaskDelay(1000 / portTICK_RATE_MS);
        GPIO_OUTPUT_SET(2, 1);
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}

// put function definitions here:
