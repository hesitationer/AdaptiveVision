/*==================================================
==================================================*/

#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <map>
#include <vector>
#include <string>

#include "opencv2/core/core.hpp"
#include "opencv2/flann/miniflann.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/photo/photo.hpp"
#include "opencv2/video/video.hpp"
#include "opencv2/features2d/features2d.hpp"
#include "opencv2/objdetect/objdetect.hpp"
#include "opencv2/calib3d/calib3d.hpp"
#include "opencv2/ml/ml.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/core/core_c.h"
#include "opencv2/highgui/highgui_c.h"
#include "opencv2/imgproc/imgproc_c.h"

#include "MessageOpCode.h"
#include "Message.h"
#include "PID.h"
#include "ActionCode.h"
#include "MessageStack.h"
#include "MessageException.h"

using namespace std;
using namespace cv;

#define PORT    1234        // Port used by the server service
#define BACKLOG 1           // Number of connections to queue
#define BUFFERLENGTH 300    // 300 bytes

#define DEFAULT_HUE 3       // Default Hue
#define DEFAULT_SAT 3       // Default Saturation
#define DEFAULT_VAL 3       // Default Value



/*=====================================================
=========== EXAMPLE OF RECEIVENG OPENCV MAT ===========
=====================================================*/


// Mat  img = Mat::zeros( height,width, CV_8UC3);
// int  imgSize = img.total()*img.elemSize();
// uchar sockData[imgSize];

//  //Receive data here

// for (int i = 0; i < imgSize; i += bytes) {
//     if ((bytes = recv(connectSock, sockData +i, imgSize  - i, 0)) == -1) {
//         quit("recv failed", 1);
//     }
// }

//  // Assign pixel value to img

// int ptr=0;
// for (int i = 0;  i < img.rows; i++) {
//     for (int j = 0; j < img.cols; j++) {                                     
//         img.at<cv::Vec3b>(i,j) = cv::Vec3b(sockData[ptr+ 0],sockData[ptr+1],sockData[ptr+2]);
//         ptr=ptr+3;
//     }
// }


void write_log(){

}

// Write the information received from the client to the database

void writeDB(){

}

// Write a function to pull respective information from the database

void processGet(int socket, char *client_ip, map<string,vector<int> > sampleAnswer){

    if(sampleAnswer.count(string(client_ip)) == 0){

        string clientAddress = string(client_ip);
        vector<int> defaultHSV = {DEFAULT_HUE,DEFAULT_SAT};

        sampleAnswer.insert ( pair<string,vector<int> >(clientAddress,defaultHSV ));

        char const *message = "The client has no previous history of connection";
        write(socket , message , strlen(message));
        cout<<"The client had no previous history of connection. Client was added to the map\n";

    }

    else if(sampleAnswer[string(client_ip)].size() < 2){

        char const *message = "Only Default Settings available at the moment";
        write(socket , message , strlen(message));
        cout<<"Only Default Settings available at the moment\n";

    }

    else if(sampleAnswer[string(client_ip)][0] == sampleAnswer[string(client_ip)][1]){
        
        char const *message = "The Value has already been sent before";
        write(socket , message , strlen(message));
        cout<<"The Value has already been sent before\n";

    }

    else{
        
        // char const *message = "The new Value is ready to be sent";
        // write(socket , message , strlen(message));
        // cout<<"The new Value is ready to be sent\n";
        // string x = to_string(sampleAnswer[string(client_ip)][1]);
        // char const *c = x.c_str();

        Message newmessage("GET","hello world");
        string newTest = newmessage.toString();

        send(socket,newTest.c_str(),newTest.size(),MSG_CONFIRM);

        cout<<"New value sent\n";

    }

}

void processPut(int socket, char *client_ip, map<string,vector<int> > sampleAnswer){
    // if() // check if the client_ip is in the map

    char const *message = "Ready to receive information";
    write(socket , message , strlen(message));
    cout<<"Ready to receive information\n";

    char toAnalyze[BUFFERLENGTH];
    int numbytes;

    if((numbytes = recv(socket, toAnalyze, BUFFERLENGTH-1, 0)) == -1)
    {
        perror("recv()");
        exit(1);
    }
    else
        printf("Proceeding to retrieve the information from client\n");
     
    toAnalyze[numbytes] = '\0';
    printf("The following has been received: %s \n", toAnalyze);

    // accept all the data from the clients
    // assign the data from clients into arrays based on the clients IP address
    // Communicate the stored information to the application
    // respond with an acknowledgement that data has been stored
    // LOG
}

void process_default(){
    // attempt to send status of the data, whther it is currently available or not
    // Do not repeat the attempt
    // terminate
    // LOG
}

int main(int argc , char *argv[])
{
    char parrentBuffer[BUFFERLENGTH];
    int parentSocket, childSocket, addrlen;
    struct sockaddr_in server, client;

    map<string, int> actionMap;
    actionMap.insert ( pair<string,int>("GET",0) );
    actionMap.insert ( pair<string,int>("PUT",1) );

    map<string,vector<int> > sampleAnswer; // string is the client_ip, int[2] is an array that holds the new computed value and the old one

    vector<int> test = {1,2};

    sampleAnswer.insert ( pair<string,vector<int> >("127.0.0.1",test));

    if((parentSocket = socket(AF_INET , SOCK_STREAM , 0)) < 0)
    {
        printf("Could not create socket");
        close(parentSocket);
        exit(-1);
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY; // Autofill with my ip (lcalhost)
    server.sin_port = htons( PORT );

    memset(&(server.sin_zero), 0, 8); // Set zero values to the rest of the server structure
     
    //Bind
    if( bind(parentSocket,(struct sockaddr *)&server , sizeof(server)) < 0)
    {
        puts("bind failed");
        close(parentSocket);
        exit(-1);
    }
    puts("binding successful");
     
    listen(parentSocket , BACKLOG);
    puts("Waiting for incoming connections...");
    addrlen = sizeof(struct sockaddr_in);

    while(true){
        if((childSocket = accept(parentSocket, (struct sockaddr *)&client, (socklen_t*)&addrlen)) < 0){
            close(childSocket);
            exit(-1);
        }

        else{
            switch(pid_t new_fork = fork())
            {
                case -1: // Error
                    {
                        perror("forking failed");
                        close(parentSocket);
                        close(childSocket);
                        exit(-1);
                    }
                case 0: // Child
                    {
                        bool connectionStatus = true;
                        Message message("READY","hi sexy");
                        string dataToSend = message.toString();
                        cout << "HI SEXY:::::::::"<<dataToSend<< endl;
                        // send(socket_desc,&dataLength ,sizeof(uint32_t) ,MSG_CONFIRM); // Send the data length
                        send(childSocket,dataToSend.c_str(),dataToSend.size(),MSG_CONFIRM);

                        while (connectionStatus){

                            cout<<"the pid of the child is: "<<new_fork<< endl;
                            close(parentSocket);
                            puts("Connection accepted");

                            char *client_ip = inet_ntoa(client.sin_addr);
                            int client_port = ntohs(client.sin_port);
                            puts("Client IP is: ");
                            puts(client_ip);

                            cout<<"ready to receive action\n";

                            int numbytes;
                            char childBuffer[BUFFERLENGTH];

                            if((numbytes = recv(childSocket, childBuffer, BUFFERLENGTH-1, 0)) == -1)
                            {
                                perror("recv()");
                                exit(1);
                            }
                            else
                                printf("Proceeding to retrieve the action from client\n");
                             
                            childBuffer[numbytes] = '\0';
                            printf("Action Requested: %s \n", childBuffer);

                            switch (actionMap[string(childBuffer)]){

                                case 0: // GET
                                    {   

                                        cout<<"first process"<<endl;

                                        cout<<sampleAnswer[string(client_ip)][1]<<endl;

                                        processGet(childSocket, client_ip, sampleAnswer);
                                        // Would you like me to do anything else for you?
                                        // if yes ==> do another iteration keeping connectionStatus = true
                                        // if no ==> set connection status = false
                                        break;
                                    }

                                case 1: // PUT
                                    {
                                        cout<<"second process\n";
                                        processPut(childSocket, client_ip, sampleAnswer);
                                        // Would you like me to do anything else for you?
                                        // if yes ==> do another iteration keeping connectionStatus = true
                                        // if no ==> set connection status = false
                                        break;
                                    }

                                default:
                                    {
                                        process_default();
                                        // Would you like me to do anything else for you?
                                        // if yes ==> do another iteration keeping connectionStatus = true
                                        // if no ==> set connection status = false
                                        continue;
                                    }
                            }
                        }

                        close(childSocket);
                        exit(-1);
                    }
                default:    // Parent
                    close(childSocket);
                    continue;
            }
        }
    }
     
    return 0;

}