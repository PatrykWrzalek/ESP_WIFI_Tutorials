#include "stdio.h"
#include "esp_common.h"
#include "freertos/task.h"
#include "gpio.h"
#include "uart.h"
#include "lwip\netifapi.h"
#include "lwip\lwip\tcp.h"
#include "lwip\lwip\sockets.h"
#include "ESP_TCP.h"

// put definition here:
#define MAX_SSID_LENGTH 32 // Maksymalna długość SSID (nazwy sieci)
#define MAX_NETWORKS 20    // Maksymalna ilość sieci (wyszukiwanych)

#define ESP_AP_SSID "ESP_Iot"      // Definicja SSID dla AP
#define ESP_AP_PASS "12345678"     // Definicja hasła dla AP
#define ESP_AP_CHANNEL 0           // Definicja kanału dla AP [not in use]
#define ESP_AP_MAX_CLIENTS 4       // Definicja max ilości klientów dla AP (max 4)
#define ESP_AP_HIDDEN 0            // Definicja widoczności sieci dla AP (0 - sieć widoczna, 1 - sieć ukryta) [not in use]
#define ESP_AP_beacon_interval 100 // Definicja czasu rozgłoszeniowego dla AP [not in use]

#define WIFI_AP_SSID "WiFi SSID"     // Definicja nazwy sieci z którą zamierzamy się połączyć
#define WIFI_AP_PASSWORD "WiFi PASS" // Definicja hasła dla WiFi z którym zamierzamy się połączyć

// put Task declarations here:
void status_LED(void *ignore);
void wifi_scan(void *ignore);
void softap_init(void *ignore);
void conn_wifi_init(void *ignore);

// put function declarations here:
void scan_done(void *arg, STATUS status);

// put global variables, etc. here:

/*// Funkcja callback do obsługi przychodzących połączeń i wysłania "Hello World"
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
}*/

// Funkcja callback do obsługi zdarzeń WiFi
void wifi_handle_event_cb(System_Event_t *evt)
{
    switch (evt->event_id) // Sprawdź rodzaj zdarzenia
    {
    case EVENT_STAMODE_CONNECTED: // Gdy urządzenie ESP połączy się z siecią WiFi (STATION mode)
        os_printf("connect to ssid %s, channel %d\r\n", evt->event_info.connected.ssid,
                  evt->event_info.connected.channel); // Wypisz SSID sieci oraz kanał
        break;

    case EVENT_STAMODE_DISCONNECTED: // Gdy urządzenie ESP rozłączy się z siecią WiFi
        os_printf("disconnect from ssid %s, reason %d\r\n", evt->event_info.disconnected.ssid,
                  evt->event_info.disconnected.reason); // Wypisz SSID oraz powód rozłączenia
        break;

    case EVENT_STAMODE_AUTHMODE_CHANGE: // Gdy zmieni się tryb uwierzytelniania
        os_printf("mode: %d -> %d\r\n", evt->event_info.auth_change.old_mode, evt->event_info.auth_change.new_mode);
        // Wypisz stary i nowy tryb uwierzytelniania
        break;

    case EVENT_STAMODE_GOT_IP: // Gdy urządzenie ESP otrzyma adres IP od serwera DHCP
        os_printf("ip:" IPSTR ",mask:" IPSTR ",gw:" IPSTR, IP2STR(&evt->event_info.got_ip.ip),
                  IP2STR(&evt->event_info.got_ip.mask), IP2STR(&evt->event_info.got_ip.gw));
        // Wypisz adres IP, maskę podsieci oraz adres bramy
        os_printf("\r\n"); // Zakończ linię
        break;

    case EVENT_SOFTAPMODE_STACONNECTED: // Gdy nowe urządzenie (klient) połączy się z ESP działającym jako AP
        os_printf("station: " MACSTR "join, AID = %d\r\n", MAC2STR(evt->event_info.sta_connected.mac),
                  evt->event_info.sta_connected.aid); // Wypisz adres MAC klienta oraz jego AID (identyfikator)
        break;

    case EVENT_SOFTAPMODE_STADISCONNECTED: // Gdy urządzenie (klient) rozłączy się z ESP działającym jako AP
        os_printf("station: " MACSTR "leave, AID = %d\r\n", MAC2STR(evt->event_info.sta_disconnected.mac),
                  evt->event_info.sta_disconnected.aid); // Wypisz adres MAC klienta oraz jego AID
        break;

    default:
        break; // Jeżeli zdarzenie nie jest rozpoznane, nic nie rób
    }
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

    // xTaskCreate(wifi_scan, "ESP STA", 1024, NULL, 3, NULL);
    // xTaskCreate(softap_init, "ESP AP", 4096, NULL, 3, NULL);
    xTaskCreate(conn_wifi_init, "ESP STA-AP", 4096, NULL, 3, NULL);

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
    ap_config.ssid_len = strlen(ESP_AP_SSID);         // Długość SSID
    // ap_config.channel = ESP_AP_CHANNEL;                 // Ustaw kanał
    ap_config.authmode = AUTH_WPA_WPA2_PSK;        // Ustaw tryb szyfrowania na WPA/WPA2
    ap_config.max_connection = ESP_AP_MAX_CLIENTS; // Ustaw maksymalną liczbe połączeń
    // ap_config.ssid_hidden = ESP_AP_HIDDEN;              // Ustaw widoczność sieci
    // ap_config.beacon_interval = ESP_AP_beacon_interval; // Ustaw czas rozgłoszeniowy dla AP (100 ~ 60000 ms)

    wifi_softap_set_config(&ap_config);                            // Zastosuj konfigurację AP
    struct station_info *station = wifi_softap_get_station_info(); // Pobranie informacji o połączonych klientach
    while (station)
    {
        os_printf("Podlaczone STA (old): \r\nbssid : MACSTR, \r\nip : IPSTR/n\r\n", MAC2STR(station->bssid), IP2STR(&station->ip));
        station = STAILQ_NEXT(station, next);
    }
    wifi_softap_free_station_info(); // Zwolnienie miejsca po przez zwolnienie danych dotyczących podłączonych stacji
    wifi_softap_dhcps_stop();        // Zatrzymanie DHCP w celu nadania statycznego adresu IP dla AP

    struct ip_info ip_info;                       // Ustawienia IP dla trybu AP
    IP4_ADDR(&ip_info.ip, 192, 168, 1, 1);        // Ustaw adres IP dla AP
    IP4_ADDR(&ip_info.gw, 192, 168, 1, 1);        // Ustaw bramę dla AP (adres IP, do którego będą kierowane pakiety spoza podsieci)
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0); // Ustaw maskę podsieci (umożliwia rozróżnienie części sieciowej od części hosta)

    wifi_set_ip_info(SOFTAP_IF, &ip_info); // Zastosuj ustawienia IP

    struct dhcps_lease dhcp_lease;                    // Konfiguracja DHCP - automatyczne przydzielanie klientom IP z puli adresów
    dhcp_lease.enable = true;                         // Włącz leasing DHCP
    IP4_ADDR(&dhcp_lease.start_ip, 192, 168, 1, 100); // Początkowy adres IP dla DHCP
    IP4_ADDR(&dhcp_lease.end_ip, 192, 168, 1, 150);   // Końcowy adres IP dla DHCP

    wifi_softap_set_dhcps_lease(&dhcp_lease);                               // Zastosuj leasing DHCP
    bool esp_ap_offer_router = 1;                                           // Ustawienie na 1, aby włączyć funkcję OFFER_ROUTER
    wifi_softap_set_dhcps_offer_option(OFFER_ROUTER, &esp_ap_offer_router); // Przekazanie klientowi informacji o bramie domyślnej (routerze)
    wifi_softap_dhcps_start();                                              // Uruchom serwer DHCP

    // for debug
    // os_printf("Access Point Ready\r\n");
    // vTaskDelay(5000 / portTICK_RATE_MS);
    // os_printf("\e[1;1H\e[2J\r\r"); // clear screen in terminal

    start_tcp_server(); // Utworzenie TCP serwera

    vTaskDelete(NULL); // Usunięcie zadania
}
/******************************************************************************
 * FunctionName : get_time_from_API
 * Description  : Function to get time from API.
 * Parameters   : none
 * Returns      : none
 *******************************************************************************/
void get_time_from_API()
{
    struct sockaddr_in server_addr; // Struktura do przechowywania adresu serwera
    int sock;                       // Zmienna do przechowywania uchwytu gniazda
    char request[256];              // Bufor do przechowywania żądania HTTP
    char response[512];             // Bufor do przechowywania odpowiedzi serwera
    int bytes_received;             // Zmienna do przechowywania liczby odebranych bajtów

    // Tworzenie gniazda
    sock = socket(AF_INET, SOCK_STREAM, 0); // Tworzenie gniazda TCP
    if (sock < 0)
    {                                           // Sprawdzanie, czy gniazdo zostało poprawnie utworzone
        os_printf("Error creating socket\r\n"); // Wyświetlenie błędu
        return;                                 // Zakończenie funkcji
    }

    // Ustawienia adresu serwera
    server_addr.sin_family = AF_INET;                         // Ustawienie rodziny adresów na IPv4
    server_addr.sin_port = htons(80);                         // Ustawienie portu na 80 (HTTP)
    server_addr.sin_addr.s_addr = ipaddr_addr("20.49.104.6"); // Ustawienie adresu IP serwera

    // Nawiązywanie połączenia z serwerem
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        os_printf("Connection failed\r\n"); // Wyświetlenie błędu, jeśli połączenie się nie powiodło
        close(sock);                        // Zamykanie gniazda
        return;                             // Zakończenie funkcji
    }

    // Utworzenie żądania HTTP GET
    sprintf(request, "GET /api/json/utc/now HTTP/1.1\r\nHost: worldclockapi.com\r\nConnection: close\r\n\r\n");
    send(sock, request, strlen(request), 0); // Wysłanie żądania do serwera

    // Odbieranie odpowiedzi
    while ((bytes_received = recv(sock, response, sizeof(response) - 1, 0)) > 0)
    {
        response[bytes_received] = '\0'; // Zakończenie odpowiedzi NULL, aby móc ją poprawnie wyświetlić
        os_printf("%s", response);       // Wyświetl odpowiedź w terminalu
    }

    // Zamykanie gniazda
    close(sock); // Zamknięcie gniazda po zakończeniu operacji
}
/******************************************************************************
 * FunctionName : conn_wifi_init
 * Description  : Function to initialize connection between ESP and WiFi.
 * Parameters   : none
 * Returns      : none
 *******************************************************************************/
void conn_wifi_init(void *ignore)
{
    // os_printf("Inicjalizacja...\r\n"); // for debug
    vTaskDelay(10000 / portTICK_RATE_MS); // Inicjacja WiFi po 10s, czas na włączenie podglądu portu szeregowego

    // Non-volatile storage
    // struct esp_spiffs_config SPIFFS_conf;
    // SPIFFS_conf.phys_size = 0;
    // SPIFFS_conf.phys_addr = 0;
    // SPIFFS_conf.phys_erase_block = 0;
    // SPIFFS_conf.log_block_size = 0;
    // SPIFFS_conf.log_page_size = 0;
    // SPIFFS_conf.fd_buf_size = 0;
    // SPIFFS_conf.cache_buf_size = 0;
    // if (esp_spiffs_init(SPIFFS_conf))
    // {
    //     os_printf("SPIFFS initialization failed\r\n\r\n");
    //     esp_spiffs_deinit(1); // Sformatowanie i deinit SPIFFS w przypadku błędu
    // }
    netif_init(); // TCP/IP initialization

    wifi_set_opmode(STATIONAP_MODE);                    // Ustaw tryb STATION + AP
    struct station_config config;                       // Zainicjuj strukturę konfiguracji Soft AP
    memset(&config, 0, sizeof(config));                 // Wyczyść strukturę
    sprintf((char *)config.ssid, WIFI_AP_SSID);         // Ustaw SSID WiFi z którym się łączysz
    sprintf((char *)config.password, WIFI_AP_PASSWORD); // Ustaw hasło WiFi z którym się łączysz
    if (wifi_station_set_hostname((char *)ESP_AP_SSID)) // Ustaw nazwę hosta ESP (nazwa urządzenia widoczna przez router)
    {
        os_printf("ESP hostname set to: %s\r\n", wifi_station_get_hostname()); // for debug
    }
    else
    {
        os_printf("Can't set ESP hostname!\r\nESP hostname is: %s\r\n", wifi_station_get_hostname()); // for debug
    }

    wifi_station_set_config(&config);                // Zastosuj konfigurację
    wifi_set_event_handler_cb(wifi_handle_event_cb); // Ustaw callback dla zdarzeń WiFi
    wifi_station_connect();                          // Połącz się z siecią

    vTaskDelay(5000 / portTICK_RATE_MS);
    get_time_from_API();

    vTaskDelete(NULL); // Usunięcie zadania
}