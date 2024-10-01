#include "stdio.h"
#include "esp_common.h"
#include "freertos/task.h"
#include "gpio.h"
#include "uart.h"
#include "lwip\netifapi.h"
#include "lwip\lwip\tcp.h"

// put definition here:
#define MAX_SSID_LENGTH 32 // Maksymalna długość SSID (nazwy sieci)
#define MAX_NETWORKS 20    // Maksymalna ilość sieci (wyszukiwanych)

#define ESP_AP_SSID "ESP_Iot"  // Definicja SSID
#define ESP_AP_PASS "12345678" // Definicja hasła
#define MAX_CLIENTS 4          // Definicja max ilości klientów
#define CHANNEL 0              // Definicja kanału (not in use)

// put Task declarations here:
void status_LED(void *ignore);
void wifi_scan(void *ignore);
void softap_init(void *ignore);

// put function declarations here:
void scan_done(void *arg, STATUS status);
void start_tcp_server();

// put global variables, etc. here:
struct tcp_pcb *server_pcb;

// Funkcja callback do obsługi przychodzących połączeń i wysłania "Hello World"
err_t tcp_accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    // Tworzymy wiadomość odpowiedzi HTTP
    char *response = "HTTP/1.1 200 OK\r\n"
                     "Content-Type: text/plain\r\n\r\n"
                     "Hello, World from ESP8266!";

    err_t write_err = tcp_write(newpcb, response, strlen(response), TCP_WRITE_FLAG_COPY); // Wyślij odpowiedź do klienta
    if (write_err == ERR_OK)                                                              // Warunek wykonania jeśli zapis się powiódł
    {
        tcp_output(newpcb); // Wyślij dane
        // os_printf("Created TCP server PCB\r\n"); // for debug
    }
    else
    {
        // os_printf("Error during tcp_write: %d\r\n", write_err); // for debug
    }

    tcp_close(newpcb); // Zamknij połączenie

    return ERR_OK; // Zwróć status OK, co oznacza pomyślne zakończenie obsługi połączenia
}
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

    // xTaskCreate(wifi_scan, "WiFi", 1024, NULL, 3, NULL);
    xTaskCreate(softap_init, "WiFi", 4096, NULL, 3, NULL);

    xTaskCreate(status_LED, "Status", 512, NULL, 1, NULL);
}

// put function definitions here:

/******************************************************************************
 * FunctionName : status_LED
 * Description  : on board LED blinking every 1s for 1s.
 * Parameters   : none
 * Returns      : none
 *******************************************************************************/
void status_LED(void *ignore)
{
    while (true)
    {
        GPIO_OUTPUT_SET(2, 0);
        vTaskDelay(1000 / portTICK_RATE_MS);
        GPIO_OUTPUT_SET(2, 1);
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
    vTaskDelete(NULL); // Usunięcie zadania
}
/******************************************************************************
 * FunctionName : scan_done
 * Description  : Callback function invoked after Wi-Fi scanning is completed.
 * Parameters   : none
 * Returns      : none
 *******************************************************************************/
void scan_done(void *arg, STATUS status)
{
    // Sprawdzenie, czy skanowanie zakończyło się sukcesem
    if (status == OK)
    {
        struct bss_info *bss = (struct bss_info *)arg; // Zmienna 'bss' wskazuje na pierwszy wynik skanowania (informacje o pierwszej znalezionej sieci)
        os_printf("Skanowanie zakonczone.\r\n Znaleziono sieci:\r\n");

        while (bss != NULL) // Przechodzimy przez listę wyników, dopóki są znalezione sieci
        {
            os_printf("SSID: %s \r\nRSSI: %d \r\nKanal: %d \r\nUkryta: %s\r\n", // Wyświetlenie informacji o znalezionej sieci:
                      bss->ssid,                                                // - Nazwa sieci (SSID)
                      bss->rssi,                                                // - Moc sygnału (RSSI)
                      bss->channel,                                             // - Numer kanału, na którym pracuje sieć
                      bss->is_hidden ? "Tak" : "Nie");                          // - Informacja, czy sieć jest ukryta (hidden)

            bss = bss->next.stqe_next; // Przejście do następnej sieci w liście wyników skanowania
        }
    }
    else
    {
        os_printf("Blad skanowania\r\n"); // Jeżeli skanowanie zakończyło się błędem, wyświetl komunikat
    }
}
/******************************************************************************
 * FunctionName : wifi_scan
 * Description  : Function that searches for AP's in STA mode (WiFi client) of ESP.
 * Parameters   : none
 * Returns      : none
 *******************************************************************************/
void wifi_scan(void *ignore)
{
    // os_printf("Inicjalizacja...\r\n"); // for debug
    vTaskDelay(10000 / portTICK_RATE_MS); // Inicjacja WiFi po 10s, czas na włączenie podglądu portu szeregowego

    ///////////////////////////////////////////////////////////////////////////////////////
    ///*                                    Note:                                       *//
    // W trakcie zmiany trybu pracy WiFi (konfiguracji) można zaobserwować po przez UART //
    // wyświetlenie komunikatów związanych z konfiguracją WiFi takich jak:               //
    //                                                                                   //
    // bcn 0 - zaprzestanie wysyłania komunikatów "beacon" (rozgłoszeniowych AP od ESP)  //
    // del if1 - usunięcie interfejsu sieciowego num1, wykorzystywanego jako AP          //
    // usl i sul 0 0 - komunikaty związane z obsługą stosu sieciowego                    //
    // mode : sta(adres.MAC) - komunikat o przełączeniu w tryb stacji oraz adres.MAC dla //
    //        urządzenia ESP w trybie stacji                                             //
    // add if0 - dodanie interfejsu sieciowego num0, wykorzystywanego jako STA (STA)     //
    ///////////////////////////////////////////////////////////////////////////////////////
    // (AP) - Access Point (Punkt dostępu - ESP działa jak klient Wi-Fi)                 //
    // (STA) - Station (Stacja - ESP działa jak router Wi-Fi)                            //
    ///////////////////////////////////////////////////////////////////////////////////////
    ///*                                    Note2:                                      *//
    // Nie należy ustawiać trybu pracy WiFi w funkcji user_init(), gdyz grozi to błędami!//
    ///////////////////////////////////////////////////////////////////////////////////////
    wifi_set_opmode_current(STATION_MODE); // Ustaw tryb stacji Wi-Fi

    struct scan_config scanConf;
    scanConf.ssid = NULL;     // Skanuj wszystkie nazwy sieci
    scanConf.bssid = NULL;    // Skanuj wszystkie MAC.addresy sieci
    scanConf.channel = 0;     // Skanuj wszystkie kanały
    scanConf.show_hidden = 1; // Pokaż również ukryte sieci

    wifi_station_scan(&scanConf, scan_done); // Funkcja rozpoczynająca skanowanie sieci
    vTaskDelete(NULL);                       // Usunięcie zadania
}
/******************************************************************************
 * FunctionName : softap_init
 * Description  : Function to initialize ESP in Soft AP Mode.
 * Parameters   : none
 * Returns      : none
 *******************************************************************************/
void softap_init(void *ignore)
{
    // os_printf("Inicjalizacja...\r\n"); // for debug
    vTaskDelay(10000 / portTICK_RATE_MS); // Inicjacja WiFi po 10s, czas na włączenie podglądu portu szeregowego

    wifi_set_opmode(SOFTAP_MODE); // Ustaw tryb stacji Wi-Fi

    struct softap_config ap_config;                   // Zainicjuj strukturę konfiguracji Soft AP
    memset(&ap_config, 0, sizeof(ap_config));         // Wyczyść strukturę
    sprintf((char *)ap_config.ssid, ESP_AP_SSID);     // Ustaw SSID
    sprintf((char *)ap_config.password, ESP_AP_PASS); // Ustaw hasło
    ap_config.authmode = AUTH_WPA_WPA2_PSK;           // Ustaw tryb szyfrowania na WPA/WPA2
    ap_config.max_connection = MAX_CLIENTS;           // Maksymalna liczba połączeń
    ap_config.ssid_len = strlen(ESP_AP_SSID);         // Długość SSID

    wifi_softap_set_config(&ap_config); // Zastosuj konfigurację AP

    struct ip_info ip_info;                       // Ustawienia IP dla trybu AP
    IP4_ADDR(&ip_info.ip, 192, 168, 1, 1);        // Ustaw adres IP dla AP
    IP4_ADDR(&ip_info.gw, 192, 168, 1, 1);        // Ustaw bramę dla AP (adres IP, do którego będą kierowane pakiety spoza podsieci)
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0); // Ustaw maskę podsieci (umożliwia rozróżnienie części sieciowej od części hosta)

    wifi_set_ip_info(SOFTAP_IF, &ip_info); // Zastosuj ustawienia IP

    struct dhcps_lease dhcp_lease;                    // Konfiguracja DHCP - automatyczne przydzielanie klientom IP z puli adresów
    dhcp_lease.enable = true;                         // Włącz leasing DHCP
    IP4_ADDR(&dhcp_lease.start_ip, 192, 168, 1, 100); // Początkowy adres IP dla DHCP
    IP4_ADDR(&dhcp_lease.end_ip, 192, 168, 1, 150);   // Końcowy adres IP dla DHCP

    wifi_softap_set_dhcps_lease(&dhcp_lease); // Zastosuj leasing DHCP
    wifi_softap_dhcps_start();                // Uruchom serwer DHCP

    // for debug
    // os_printf("Access Point Ready\r\n");
    // vTaskDelay(5000 / portTICK_RATE_MS);
    // os_printf("\e[1;1H\e[2J\r\r"); // clear screen in terminal

    start_tcp_server(); // test

    vTaskDelete(NULL); // Usunięcie zadania
}
/******************************************************************************
 * FunctionName : start_tcp_server
 * Description  : Function to initialize the TCP server.
 * Parameters   : none
 * Returns      : none
 *******************************************************************************/
void start_tcp_server()
{
    server_pcb = tcp_new(); // Utwórz nową strukturę PCB (Protocol Control Block)

    if (server_pcb != NULL)
    {
        // Jeśli PCB zostało poprawnie utworzone, serwer przechodzi do nasłuchiwania
        tcp_bind(server_pcb, IP_ADDR_ANY, 80); // Powiąż serwer z dowolnym adresem IP i portem 80 (standardowy port HTTP)

        server_pcb = tcp_listen(server_pcb); // Ustaw serwer w trybie nasłuchiwania na nowe połączenia

        tcp_accept(server_pcb, tcp_accept_callback); // Zarejestruj funkcję callback do obsługi nowych połączeń

        os_printf("Created TCP server PCB\r\n");
    }
    else
    {
        // Jeśli nie udało się utworzyć PCB, wyświetl komunikat o błędzie
        os_printf("Error: Unable to create TCP server PCB\r\n");
    }
}