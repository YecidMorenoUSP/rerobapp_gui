#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <thread>

#include <nlohmann/json.hpp>

#include <iostream>
#include <chrono>

using json = nlohmann::json;

sockaddr_in recv_addr;
sockaddr_in send_addr;

void send_data(int sockfd, sockaddr_in addr) {
    json j;
    j["name"] = "ExoTAO";
    j["soc"] = addr.sin_port;

    
    std::string s = j.dump();

    struct sockaddr_in sin;
        socklen_t len = sizeof(sin);
        if (getsockname(sockfd, (struct sockaddr *)&sin, &len) == -1)
            perror("getsockname");
        else
            printf("\n>>port number %d\n", ntohs(sin.sin_port));

    for (int i = 0; i < 5; ++i) {       

        if (sendto(sockfd, s.c_str(), s.length(), 0, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("sendto failed");
        }

    std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

void receive_data(int sockfd) {
    char buffer[1024];
    sockaddr_in src_addr;
    socklen_t addr_len = sizeof(src_addr);

    for (int i = 0; i < 100; ++i) {
        int len = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&src_addr, &addr_len);
        if (len > 0) {
            
            if(src_addr.sin_port == send_addr.sin_port ){
                i--;
                continue;
            }

            buffer[len] = '\0';
            std::cout << "Received: " << buffer << " from " << inet_ntoa(src_addr.sin_addr) << std::endl;
            std::cout << "src_addr: " << ntohs(src_addr.sin_port) << std::endl;
            std::cout << "send_addr: " <<ntohs( send_addr.sin_port) << std::endl;
            std::cout << "recv_addr: " <<ntohs( recv_addr.sin_port) << std::endl;
        }
    }
}

int main() {


    int recv_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (recv_sock < 0) {
        perror("Could not create receive socket");
        return -1;
    }

    int enable = 1;
    if (setsockopt(recv_sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        close(recv_sock);
        return -1;
    }

    #ifdef SO_REUSEPORT
    if (setsockopt(recv_sock, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(int)) < 0) {
        perror("setsockopt(SO_REUSEPORT) failed");
        close(recv_sock);
        return -1;
    }
    #endif

    
    memset(&recv_addr, 0, sizeof(recv_addr));
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_port = htons(1234);  // Puerto de escucha
    recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(recv_sock, (struct sockaddr*)&recv_addr, sizeof(recv_addr)) < 0) {
        perror("bind failed on receive socket");
        close(recv_sock);
        return -1;
    }

    // Crear socket de envío
    int send_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sock < 0) {
        perror("Could not create send socket");
        close(recv_sock);
        return -1;
    }

    // Configurar dirección de destino para enviar
    memset(&send_addr, 0, sizeof(send_addr));
    send_addr.sin_family = AF_INET;
    send_addr.sin_port = htons(1234);  // Suponiendo que enviamos al mismo puerto
    send_addr.sin_addr.s_addr = inet_addr("255.255.255.255");  // Dirección de broadcast o específica

    int broadcastEnable = 1;
    int ret = setsockopt(send_sock, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));
    if (ret) {
        perror("setsockopt for SO_BROADCAST failed");
        close(send_sock);
        return -1;
    }

    //std::thread sender(send_data, send_sock, send_addr);
    std::thread receiver(receive_data, recv_sock);

    //sender.join();
    receiver.join();

    //close(send_sock);
    close(recv_sock);
    return 0;
}
