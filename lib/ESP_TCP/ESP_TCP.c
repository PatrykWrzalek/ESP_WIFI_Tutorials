#include "ESP_TCP.h"

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
        err_t err = tcp_bind(server_pcb, IP_ADDR_ANY, 80); // Powiąż serwer z dowolnym adresem IP i portem 80 (standardowy port HTTP)

        if (err == ERR_OK)
        {
            server_pcb = tcp_listen(server_pcb); // Ustaw serwer w trybie nasłuchiwania na nowe połączenia

            tcp_accept(server_pcb, tcp_accept_callback); // Zarejestruj funkcję callback do obsługi nowych połączeń

            os_printf("Created TCP server PCB.\r\n");
        }
        else
        {
            os_printf("TCP binding error!\r\n");
        }
    }
    else
    {
        // Jeśli nie udało się utworzyć PCB, wyświetl komunikat o błędzie
        os_printf("Error: Unable to create TCP server PCB!\r\n");
    }
}

/******************************************************************************
 * FunctionName : tcp_accept_callback
 * Description  : tcp accept callback functions. Called when a new
 *                connection can be accepted on a listening pcb.
 * Parameters   : arg - Additional argument to pass to the callback function (@see tcp_arg())
 *                newpcb - The new connection pcb
 *                err - An error code if there has been an error accepting.
 * Returns      : none
 *******************************************************************************/
err_t tcp_accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    os_printf("Nawiazano nowe polaczenie!\r\n");

    // Zarejestruj callbacki dla połączenia
    tcp_recv(newpcb, tcp_recv_callback);     // Ustawienie callbacku dla odbioru danych
    tcp_sent(newpcb, tcp_sent_callback);     // Ustawienie callbacku dla potwierdzenia wysyłki danych
    tcp_err(newpcb, tcp_err_callback);       // Ustawienie callbacku dla obsługi błędów
    tcp_poll(newpcb, tcp_poll_callback, 16); // Ustawienie callbacku dla okresowego sprawdzania połączenia co 16 ticki

    return ERR_OK; // Zwracamy OK, by poinformować, że połączenie zostało przyjęte
}

// Funkcja odbierająca dane od klienta TCP
err_t tcp_recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    if (p == NULL) // Zamknięcie połączenia, gdy klient rozłączył się
    {
        tcp_close(tpcb);
        return ERR_OK;
    }

    // Sprawdzenie, czy dane zostały poprawnie odebrane
    if (err == ERR_OK && p != NULL)
    {
        char *data = (char *)malloc(p->tot_len + 1); // Konwertuj bufor do ciągu znaków
        if (data)
        {
            pbuf_copy_partial(p, data, p->tot_len, 0); // Skopiowanie danych z pakietu "p" do bufora "data"
            data[p->tot_len] = '\0';                   // Zakończenie ciągu znakiem null
        }

        // Sprawdzamy, czy to metoda GET (żądanie wyświetlenia formularza)
        if (strstr(data, "GET /") != NULL)
        {
            // Tworzymy odpowiedź HTML z formularzem
            char *response = "HTTP/1.1 200 OK\r\n"
                             "Content-Type: text/html\r\n\r\n"
                             "<!DOCTYPE html>"
                             "<html>"
                             "<body>"
                             "<h1>Enter your name:</h1>"
                             "<form action='/submit-name' method='POST'>"
                             "Name: <input type='text' name='name'><br>"
                             "<input type='submit' value='Submit'>"
                             "</form>"
                             "</body>"
                             "</html>";

            tcp_write(tpcb, response, strlen(response), TCP_WRITE_FLAG_COPY); // Zapisania danych w buforze wyjściowym TCP
            tcp_output(tpcb);                                                 // Wysłanie danych zapisanych w buforze TCP
        }
        // Sprawdzamy, czy to metoda POST (odbiór danych formularza)
        else if (strstr(data, "POST /submit-name") != NULL)
        {
            char *name_pos = strstr(data, "name="); // Szukamy pola "name" w danych POST
            if (name_pos != NULL)
            {
                char name[50];                     // Bufor na imię
                sscanf(name_pos, "name=%s", name); // Wyciągnięcie wartości imienia

                // Wysyłamy odpowiedź HTTP z podziękowaniem za wprowadzenie imienia
                char response[200];
                sprintf(response, "HTTP/1.1 200 OK\r\n"
                                  "Content-Type: text/html\r\n\r\n"
                                  "<html><body>"
                                  "<h1>Thank you, %s!</h1>"
                                  "<p>Your name has been received.</p>"
                                  "</body></html>",
                        name);

                tcp_write(tpcb, response, strlen(response), TCP_WRITE_FLAG_COPY); // Zapisania danych w buforze wyjściowym TCP
                tcp_output(tpcb);                                                 // Wysłanie danych zapisanych w buforze TCP
            }
        }

        free(data);   // Zwolnienie pamięci używanej przez dynamicznie zaalokowany bufor "data"
        pbuf_free(p); // Zwolnienie pamięci używanej przez stos TCP/IP
    }

    return ERR_OK;
}

// Callback wywoływany po wysłaniu danych
err_t tcp_sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
    os_printf("Dane zostaly wyslane!\r\n");
    return ERR_OK;
}

// Callback błędu - obsługa błędów połączenia
void tcp_err_callback(void *arg, err_t err)
{
    os_printf("Wystapil blad polaczenia: %d\r\n", err);
}

// Callback okresowy (poll) - sprawdza stan połączenia
err_t tcp_poll_callback(void *arg, struct tcp_pcb *tpcb)
{
    os_printf("Sprawdzanie stanu polaczenia.\r\n");
    return ERR_OK;
}