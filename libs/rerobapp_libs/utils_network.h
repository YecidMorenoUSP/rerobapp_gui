#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <thread>
#include <chrono>
#include <functional>
#include <nlohmann/json.hpp>
#include <atomic>

using json = nlohmann::json;

enum: uint16_t{
    _BC_NONE = 10,
    _BC_INIT,
    _BC_STOP,       // PARAR TODO
    _BC_PREPARE,    // Preparar para iniciar
    _BC_START,      // INICIAR
};

const char * _BC_STATE_TO_STR(uint16_t State){
    switch (State)
    {
case _BC_NONE : return "_BC_NONE";
case _BC_INIT : return "_BC_INIT";
case _BC_STOP : return "_BC_STOP";
case _BC_PREPARE : return "_BC_PREPARE";
case _BC_START : return "_BC_START";
    default: return "Undefined";
    }
}


class Sender {
protected:
    int sockfd;
    sockaddr_in addr;

public:
    Sender(int socket, sockaddr_in address) : sockfd(socket), addr(address) {}
    Sender(){}


    void getSockName(struct sockaddr_in & sin){
        socklen_t len = sizeof(sin);
        if (getsockname(sockfd, (struct sockaddr *)&sin, &len) == -1) {
            perror("getsockname");
        } else {
            std::cout << "\n>>port number " << ntohs(sin.sin_port) << std::endl;
        }
    }

    void sendOnce(void * buff , size_t len){
        if (sendto(sockfd, buff, len, 0, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("sendto failed");
        }
    }

    void sendOnce(void * buff , size_t len,const sockaddr_in & _addr){
        if (sendto(sockfd, buff, len, 0, (struct sockaddr*)&_addr, sizeof(sockaddr_in)) < 0) {
            perror("sendto failed");
        }
    }

    void sendOnce(json& j){
        std::string s = j.dump();
        sendOnce((void*) s.c_str(),s.length());
    }

    Sender& operator=(Sender const& lh)
    {
        this->sockfd = lh.sockfd;
        this->addr = lh.addr;
        return *this;
    }

};

class Receiver {
protected:
    int sockfd;
    sockaddr_in addr;
    sockaddr_in avoid_addr;

    void my_void_callback(const char* buffer,const int len,const sockaddr_in & src_addr){
        (void)len;
        printf("buffer %.*s",len,buffer);
        printf("from %s:%u",inet_ntoa(src_addr.sin_addr),ntohs(src_addr.sin_port));
        fflush(stdout);
    }

public:

    std::function<void(const char*,const int, const sockaddr_in&)> callback;

    void setCallback(std::function<void(const char*,const int, const sockaddr_in&)> _callback){
        this->callback = _callback;
    }
    
    Receiver(int socket, sockaddr_in address) : sockfd(socket), addr(address) {
        memset(&avoid_addr, 0, sizeof(sockaddr_in));
        setCallback([this](const char* buffer, const int len, const sockaddr_in& src_addr) {
            this->my_void_callback(buffer, len, src_addr);
        });
    }
    

    void sendOnce(void * buff , size_t len,const sockaddr_in & _addr){
        if (sendto(sockfd, buff, len, 0, (struct sockaddr*)&_addr, sizeof(addr)) < 0) {
            perror("sendto failed");
        }
    }

    void sendOnce(json& j,const sockaddr_in _addr){
        std::string s = j.dump();
        sendOnce((void*) s.c_str(),s.length(),_addr);
    }


    explicit Receiver(){
        memset(&avoid_addr, 0, sizeof(sockaddr_in));
        setCallback([this](const char* buffer, const int len, const sockaddr_in& src_addr) {
            this->my_void_callback(buffer, len, src_addr);
        });
    }

    void set_avoid_addr(const sockaddr_in& to_avoid_addr){
        memcpy(&avoid_addr,&to_avoid_addr,sizeof(sockaddr_in));
    }

    int recOnce(char * buffer, size_t len_bf,sockaddr_in& src_addr,socklen_t& addr_len){
        int len = recvfrom(sockfd, buffer, len_bf, 0, (struct sockaddr*)&src_addr, &addr_len);
        if (len > 0) {
            if(avoid_addr.sin_port == src_addr.sin_port && avoid_addr.sin_addr.s_addr == src_addr.sin_addr.s_addr){
                return -1;
            }
            //callback(buffer,len,src_addr);
            return len;
        }
        return -1;
    }

    void receiveData() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        char buffer[1024];
        sockaddr_in src_addr;
        socklen_t addr_len = sizeof(src_addr);
        while (true){
            recOnce(buffer,sizeof(buffer),src_addr,addr_len);
        }
        
    }

};


class RerobBroadcast{
    protected:
        Sender bc_sender;
        Receiver bc_receiver;

        int bc_recv_sock, bc_send_sock;
        sockaddr_in bc_recv_addr,bc_send_addr;
        std::atomic<bool> running_threads;

    public:

        std::atomic<uint16_t> STATE;

        RerobBroadcast(){
            STATE = _BC_NONE;
            bc_recv_sock = socket(AF_INET, SOCK_DGRAM, 0);
            bc_send_sock = socket(AF_INET, SOCK_DGRAM, 0);
            setupbcSockets();
        }
        ~RerobBroadcast(){
            STATE = _BC_NONE;
            close(bc_send_sock);
            close(bc_recv_sock);
            waitThreads();
        }

        void setupbcSockets() {

        memset(&bc_recv_addr, 0, sizeof(sockaddr_in));
        memset(&bc_send_addr, 0, sizeof(sockaddr_in));

        /*Configuracion de direcciones para el broad cast*/
        bc_recv_addr.sin_family = AF_INET;
        bc_recv_addr.sin_port = htons(1234);
        bc_recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

        bc_send_addr.sin_family = AF_INET;
        bc_send_addr.sin_port = htons(1234);
        bc_send_addr.sin_addr.s_addr = inet_addr("255.255.255.255");
        
        int enable = 1;
        int broadcastEnable = 1;

        /*Reusar el puerto si está siendo usado por otro proceso*/
        if (setsockopt(bc_recv_sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
            perror("setsockopt(SO_REUSEADDR) failed");
            return;
        }

        /*Escuchar el puerto del socket broadcast*/
        if (bind(bc_recv_sock, (struct sockaddr*)&bc_recv_addr, sizeof(sockaddr_in)) < 0) {
            perror("bind failed on receive socket");
            return;
        }

        /*Definir un tiempo limite para esperar por un mensaje*/
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        if (setsockopt(bc_recv_sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
            perror("setsockopt failed");
            return;
        }
        

        /*Permitir enviar mensajes broadcast*/
        if (setsockopt(bc_send_sock, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable))) {
            perror("setsockopt for SO_BROADCAST failed");
            return;
        }

    }    

    void startCommunication() {
        bc_sender = Sender(bc_send_sock, bc_send_addr);
        bc_receiver = Receiver(bc_recv_sock, bc_recv_addr);

        //bc_receiver.setCallback(my_void_callback);

        json j;
        j["to"] = "ALL";
        j["msg"] = "ExoTAO";
        std::string s = j.dump();

        bc_sender.sendOnce((void*)s.c_str(),s.length());
        sockaddr_in sin_sender;
        bc_sender.getSockName(sin_sender);
        bc_receiver.set_avoid_addr(sin_sender);
    }


    virtual void recv_callback(const json& j, const sockaddr_in& src_addr){
        std::cout <<"from " << inet_ntoa(src_addr.sin_addr) << ":"<< ntohs(src_addr.sin_port) << std::endl;
        std::string res = j.dump();
        std::cout<<"json: "<<res<<std::endl;
    }

    void loop_receiveData() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        char buffer[1024];
        sockaddr_in src_addr;
        socklen_t addr_len = sizeof(src_addr);
        int len = 0;
        json j;
        
        while (running_threads){
            len = bc_receiver.recOnce(buffer,sizeof(buffer),src_addr,addr_len);
        
            if(len<1){
                continue;
            }

            if(buffer[0]=='{'){
                buffer[len] = '\0';
                if(json::accept(buffer)){                            
                    j = json::parse(buffer);
                    recv_callback(j,src_addr);            
                }else{
                    printf("\n[FAIL] json[%d]: %.*s",len,len,buffer);
                    fflush(stdout); 
                }
            }else{
                printf("\nalguna cosa llegó len: %d",len);
                printf("\n%.*s",len,buffer);
                fflush(stdout);  
            }
        
        }        
    }

    virtual void send_callback(){
        json j;
        j["to"] = "ALL";
        j["msg"] = "[DEFAULT] send_callback";

        std::string s = j.dump();
        bc_sender.sendOnce((void*) s.c_str(),s.length());
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    void loop_sendData() {
        while (running_threads){
            send_callback();
        }
    }


    std::thread senderThread;
    std::thread receiverThread;
    void startThreads(){
        running_threads = true;
        senderThread = std::thread(&RerobBroadcast::loop_sendData, this);
        receiverThread= std::thread(&RerobBroadcast::loop_receiveData, this);
    }

    void waitThreads(){
        running_threads = false;
        senderThread.join();
        receiverThread.join();
    }

};



class RerobReceiver: public Receiver{
    
    private:
        void * var_ptr;
        int var_len;
        std::thread recThread;
        std::atomic<bool> running_threads;

    public:
    
    uint16_t my_port = 1234;

    void setVariable(void * _var , int _len){
        var_ptr = _var;
        var_len = _len;
    }

    RerobReceiver(void * _var , int _len){
        setVariable(_var,_len);
        init();
    }

    void init(){
        running_threads = false;
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);

        addr.sin_family = AF_INET;
        addr.sin_port = htons(my_port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);

        while (true){
            if (bind(sockfd, (struct sockaddr*)&addr, sizeof(sockaddr_in)) < 0) {
                addr.sin_port = htons(++my_port);
            }else{
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
            perror("setsockopt failed");
            return;
        }

    }

    void loop_recData(){
        int len;
        char buffer[1024];
        size_t len_buffer = sizeof(buffer);
        socklen_t addr_len = sizeof(addr);

        while (running_threads){
            len = recOnce(buffer,len_buffer,addr,addr_len);
            if(len<1) continue;
            if(len==var_len){
                memcpy(var_ptr,buffer,var_len);
                printf("\nvar Update\n");
                fflush(stdout);
            }
        }
    }

    void startThread(){

        if(recThread.joinable()){
           stopThread();
           recThread.join();
        }

        running_threads = true;
        recThread = std::thread(&RerobReceiver::loop_recData, this);
        printf("Hilo Creado rec");
        fflush(stdout);
    }

    void stopThread(){
        running_threads = false;
    }
    
    ~RerobReceiver(){
        close(this->sockfd);
        stopThread();
        recThread.join();
    }

};


class RerobSender: public Sender{
    public:
    
    std::vector<sockaddr_in> addr_to_send;

    std::thread sendThread;

    std::atomic<bool> running_threads;


    RerobSender(){
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    }

    ~RerobSender(){
        stopThread();
    }

    void loop_sendData(){
        uint32_t var = 1;
        while (running_threads)
        {
            for(auto _addr : addr_to_send){
                sendOnce(&var,sizeof(var),_addr);
            }
            var++;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));        
        }
    }

    void startThread(){
        
        if(sendThread.joinable()){
           stopThread();
           sendThread.join();
        }

        running_threads = true;
        sendThread = std::thread(&RerobSender::loop_sendData, this);
        printf("Hilo Creado send");
        fflush(stdout);
    }

    void stopThread(){
        running_threads = false;
    }
};

class RerobNetwork: public RerobBroadcast{
    public:
    
    std::vector<std::string> subscribe_names;
    std::vector<RerobReceiver*> subscribe_socket;

    std::string my_name;


    RerobSender var_sender;


    RerobNetwork(){
        var_sender.addr_to_send.reserve(20);
        subscribe_names.reserve(20);
        subscribe_socket.reserve(20);

        my_name = "NO_NAME";
    }

    void subscribe(const std::string& _name,RerobReceiver* mr){
        subscribe_names.push_back(_name);
        subscribe_socket.push_back(mr);
    }

    virtual void send_callback(){

        json j;
        
        j["from"] = my_name;
        
        switch (STATE)
        {
        case _BC_NONE: 
            // Quieto papáaa
            break;
        case _BC_INIT : 
            // Limpiar variables, enviar mis puertos para cada nodo y guardar otros puertos
            j["send_me"] = 1;
            for(size_t i = 0 ; i < subscribe_names.size() ; i++){
                j["to"] = subscribe_names[i];
                j["port"] = subscribe_socket[i]->my_port;
                bc_sender.sendOnce(j);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));        
            }

            break;
        case _BC_STOP : 
            // Pausar todo
            break;
        case _BC_PREPARE :
            // inicializar si es necesario
            break;
        case _BC_START : 
            // Comenzar a ejecutar todo
            break;

        default:
            break;
        }
        
        //bc_sender.sendOnce(j);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    void stateChanged(){
        switch (STATE)
        {
        case _BC_INIT : 
            // Limpiar variables, enviar mis puertos para cada nodo y guardar otros puertos
            var_sender.addr_to_send.clear();
            break;
        case _BC_PREPARE :
            for(size_t i = 0 ; i < subscribe_socket.size() ; i++){
                subscribe_socket[i]->startThread();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));        
            }
            break;
        case _BC_START : 
            var_sender.startThread();
            break;

        default:
            break;
        }
    }

    virtual void recv_callback(const json& j, const sockaddr_in& src_addr){

        if(!j.contains("to")) return;

        std::string to = j["to"];

        if(to.compare("ALL") != 0 && to.compare(my_name) != 0) return;

        if(j.contains("state")){
            STATE = j["state"];
            stateChanged();
        }

        switch (STATE)
        {
        case _BC_INIT : 
            // Limpiar variables, enviar mis puertos para cada nodo y guardar otros puertos
            if(j.contains("send_me")&&j.contains("port")){
                sockaddr_in src_add_to_add(src_addr);
                src_add_to_add.sin_port = htons((uint16_t)j["port"]);
                var_sender.addr_to_send.push_back(src_add_to_add);
            }
            break;
        case _BC_STOP : 
            // Pausar todo
            var_sender.stopThread();
            for(size_t i = 0 ; i < subscribe_socket.size() ; i++){
                subscribe_socket[i]->stopThread();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));        
            }
            break;
    
        default:
            break;
        }

        /*std::cout <<"[Custom]\nfrom " << inet_ntoa(src_addr.sin_addr) << ":"<< ntohs(src_addr.sin_port) << std::endl;
        std::string res = j.dump();
        std::cout<<"json: "<<res<<std::endl;
        fflush(stdout);*/

    }

};


