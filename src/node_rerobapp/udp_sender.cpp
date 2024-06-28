#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <thread>
#include <chrono>

#include <atomic>

#include <nlohmann/json.hpp>

#include <utils_network.h>

#include <vector>
#include <map>

using json = nlohmann::json;


int main(int nargin, char *varargin[]) {

    for(int i = 0 ; i < nargin ; i++){
        printf("\narg[%d]:%s",i,varargin[i]);
    }
    printf("\n\n");

    std::string name;
    if(nargin>1){
        name = varargin[1];
    }else{
        name = "No_NAME";
    }

    RerobNetwork rerobNet;
    
    // Adicionar publish
    //rerobNet.publish("H0",RerobSender);


    if(nargin>2){
        for(int i = 2 ; i < nargin ; i++){
            uint32_t fromXsens;
            RerobReceiver* XsensReceiver = new RerobReceiver(&fromXsens,sizeof(fromXsens));
            rerobNet.subscribe(varargin[i],XsensReceiver);
        }
    }else{
        uint32_t fromXsens;
        RerobReceiver* XsensReceiver = new RerobReceiver(&fromXsens,sizeof(fromXsens));
        rerobNet.subscribe("H1",XsensReceiver);
    }
    
    rerobNet.my_name = name;

    rerobNet.startCommunication();
    rerobNet.startThreads();

    std::this_thread::sleep_for(std::chrono::minutes(10));

    return 0;
}
