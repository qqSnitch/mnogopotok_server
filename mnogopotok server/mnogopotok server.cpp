#include<iostream>
#include <winsock2.h> 
#include <windows.h> 
#include <string> 
#include <thread>   // добавляем заголовок для работы с потоками
#include <atomic>   // добавляем заголовок для работы с атомарными переменными
#include <mutex>    // для блокировки доступа к списку и очереди
#include <vector>
#include <queue>
#pragma comment (lib, "Ws2_32.lib")  
using namespace std;

#define SRV_PORT 1234  // порт сервера (его обязательно должен знать клиент)
#define BUF_SIZE 64  // размер

struct Message {
    char name[20];
    char text[120];
} msg;

// количество клиентов онлайн
atomic<int> count_clients = 0;
// блокировщик доступа к списку клиентов
mutex clients_lock;
// блокировщик доступа к очереди неотправленных сообщений
mutex wait_message_lock;
// список клиентов
vector<SOCKET> clients;
// очередь неотправленных сообщений
queue<Message> wait_message;

const string QUEST = "Enter the data\n"; // первый вопрос для клиента, чтобы начать диалог

void send_connect();
void send_disconnect(Message ara);

void client_geter_thread(SOCKET s_new)
{
    //send_connect();
    Message B;
    while (true) {
        // получаем сообщение от клиента
        recv(s_new, (char*)&B, sizeof(B), 0);
        // если сообщение команда на выход, выходим из цикла
        if (!string(B.text).compare("!exit"))
            break;
        // блокируем доступ к очереди неотправленных сообщений mutex-ом 
        wait_message_lock.lock();
        // добавляем новое сообщение в очередь на отправку
        wait_message.push(B);
        // разблокируем доступ к очереди неотправленных сообщений mutex-ом 
        wait_message_lock.unlock();
    }
    //send_disconnect(B);
    cout << "disconnected" << endl;
    // уменьшаем счетчик клиентов
    count_clients.fetch_sub(1);
    cout << "count clients: " << count_clients.load() << endl;
    closesocket(s_new);
}

void client_sendler_thread()
{
    while (true)
    {
        // блокируем доступ к списку клиентов mutex-ом 
        clients_lock.lock();

        // блокируем доступ к очереди неотправленных сообщений mutex-ом 
        wait_message_lock.lock();
        int ar = wait_message.size();
        Message B;
        if (ar != 0)
        {
            B = wait_message.front();
            wait_message.pop();
        }
        // разблокируем доступ к очереди неотправленных сообщений mutex-ом 
        wait_message_lock.unlock();

        // если очередь не пуста
        if (ar != 0)
        {
            // для отладки выводим сообщения на сервере
            cout << "" << B.name << ": " << B.text << endl;
            for (unsigned i = 0; i < clients.size(); i++)
            {
                // пытемся отправить сообщение
                int size = send(clients[i], (char*)&B, sizeof(B), 0);
                // если не получилось
                if (size < 0)
                {
                    // удаляем из списка клиентов клиента с текущем номером
                    clients.erase(clients.begin() + i);
                    i--;
                }
            }
        }
        // разблокируем доступ к списку клиентов mutex-ом 
        clients_lock.unlock();
    }
}


int main()
{
    setlocale(LC_ALL, "rus");
    char buff[1024];
    if (WSAStartup(0x0202, (WSADATA*)&buff[0]))
    {
        cout << "Error WSAStartup \n" << WSAGetLastError();   // Ошибка!
        return -1;
    }
    SOCKET s;
    int from_len;
    sockaddr_in sin;
    s = socket(AF_INET, SOCK_STREAM, 0);
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = 0;
    sin.sin_port = htons(SRV_PORT);
    bind(s, (sockaddr*)&sin, sizeof(sin));
    string msg, msg1;
    listen(s, 6);

    // создаем поток на отправку новых сообщений
    thread client(client_sendler_thread);
    client.detach();

    while (1)
    {
        sockaddr_in from_sin;
        from_len = sizeof(from_sin);
        SOCKET s_new = accept(s, (sockaddr*)&from_sin, &from_len);
        if (s_new == INVALID_SOCKET) {
            cerr << "Error accepting client connection: " << WSAGetLastError() << endl;
            continue;
        }
        else
        {
            cout << "new connected client! " << endl;
            // увеличиваем счетчик клиентов
            count_clients.fetch_add(1);
            cout << "current connections: " << count_clients.load() << endl;
            // блокируем доступ к списку клиентов mutex-ом 
            clients_lock.lock();
            // добавляем нового клиента в список
            clients.push_back(s_new);
            // разблокируем доступ к списку клиентов mutex-ом 
            clients_lock.unlock();
            // Создаем новый поток для принятия сообщений от клиента s_new
            thread client(client_geter_thread, s_new);
            client.detach();  // отсоединяем поток, чтобы он продолжал работу независимо, т.е. создаем демона
        }
    }
    return 0;
}
