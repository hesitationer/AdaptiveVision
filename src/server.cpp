/*==================================================
==================================================*/


// NEED A METHOD TO COMMUNICATE CAMERA RESOLUTION AT INITIATION
// so that the server knows what size images to expect

/*==================================================
==================================================*/

// Look into compressing image data before client transmits it to the server


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
#include <unordered_map>
#include <getopt.h>
#include <sys/stat.h>
#include <sstream>
#include <thread>
#include <mutex>
#include <cmath>

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
#include "MessageStack.h"
#include "MessageQueue.h"
#include "MessageException.h"
#include "Histogram.h"
#include "CameraDetect.h"
#include "ObjectIdentification.h"
#include "Compressor.h"
#include "Decompressor.h"
#include "Template.h"
#include "MatchMSER.h"
#include "MatchHSV.h"

using namespace std;
using namespace cv;

#define PORT    1234        // Port used by the server service
#define BACKLOG 1           // Number of connections to queue
#define BUFFERLENGTH 3000000    // 300 bytes

#define DEFAULT_HUE 3       // Default Hue
#define DEFAULT_SAT 3       // Default Saturation
#define DEFAULT_VAL 3       // Default Value

void readme();
void write_log();
void writeDB();
void processGet(int, char *, map<string,vector<int> >);
void processPut(int, char *, map<string,vector<int> >);
void processDefault();

inline bool checkPath (const std::string& name) {
	struct stat buffer;   
	return (stat (name.c_str(), &buffer) == 0); 
}

int width;
int height;

MatchMSER match;
Mat imageMatch;
bool matchM = false;
// Mat *img2;

int main(int argc , char *argv[])
{
	// namedWindow("SERVER",WINDOW_NORMAL);


	//=====================================================
	//TESTING CANVAS
	//=====================================================

	// Mat im1 = imread("/home/sparrow/Pictures/Webcam/logoT.jpg");
	// imshow("hello test", im1);

	// waitKey(0);

	struct Params {
		int delta 				= 5;
		int minArea 			= 100;
		int maxArea 			= 14400;
		double maxVariation 	= 0.25;
		double minDiversity 	= 0.2;
		int maxEvolution 		= 200;
		double areaThreshold 	= 1.01;
		double minMargin 		= 0.003;
		int edgeBlurSize 		= 0;
	};

	Params params;

	// cout << params.delta << endl;

	// match.setParams(&params.delta,&params.minArea,&params.maxArea,&params.maxVariation,&params.minDiversity,
							// &params.maxEvolution,&params.areaThreshold,&params.minMargin,&params.edgeBlurSize);


	int c;
    int digit_optind = 0;

    int serverPort;
    string templatePathCheck;
    char* templatePath;
    Mat inputTPL;

    string token;
	vector<int> resolution;

   	while (1) {
        int this_option_optind = optind ? optind : 1;
        int option_index = 0;
        static struct option long_options[] = {
            {"port",     required_argument, 0,  0 },
            {0,         0,                 0,  0 }
        };

       	c = getopt_long(argc, argv, "t:d", long_options, &option_index);
        if (c == -1){
        	serverPort = PORT;
            break;
        }

       	switch (c) {
	        case 0:
	            serverPort = atoi(optarg);
	            break;

	       	case 't':
	            templatePathCheck = string(optarg);

	            if (!checkPath(templatePathCheck)){
	            	cout << "File Not Found\n" << endl;
	            	exit(EXIT_SUCCESS);
	            }
	            templatePath = optarg;
	            cout << "The path was found: " << templatePath << endl;
	            break;

	       	case '?':
	            break;

	       	default:
	            printf("?? getopt returned character code 0%o ??\n", c);
        }
    }

   	if (optind < argc) {
        printf("non-option ARGV-elements: ");
        while (optind < argc)
            printf("%s ", argv[optind++]);
        printf("\n");
    }

  	// const char* testTP = templatePath;

  	inputTPL = imread(templatePath);

  	Template testTPL(inputTPL);
	vector<Point> normalizedMser = testTPL.getTemplateNorm();
	vector<int> paramsTPL = testTPL.getParams();
	match.setTemplate(normalizedMser);
	match.setParams(paramsTPL[0],paramsTPL[1]);
	// match.setImage(img2);
	// imshow("Template", imageTPL);
	// waitKey(0);

	// VideoCapture source(0); // open the default camera
	// if(!source.isOpened())  // check if we succeeded
	//     return -1;

	// Mat image;
	// source >> image;

	// image = (image.reshape(0,1));
	// int  imgSize = image.total()*image.elemSize();

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
	server.sin_port = htons( serverPort );

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
						// cout << "HI SEXY:::::::::"<<dataToSend<< endl;
						// send(socket_desc,&dataLength ,sizeof(uint32_t) ,MSG_CONFIRM); // Send the data length
						send(childSocket,dataToSend.c_str(),dataToSend.size(),MSG_CONFIRM);

						// namedWindow("SERVER",WINDOW_NORMAL);

						while (connectionStatus){

							if (matchM){
								imshow("SERVER",imageMatch);
								// waitKey(0);
								if(waitKey(30) >= 0){
            
						            break;
						        }
							}

							cout<<"the pid of the child is: "<<new_fork<< endl;

							close(parentSocket);

							cout << "CONNECTION ACCEPTED" << endl;

							char *client_ip = inet_ntoa(client.sin_addr);
							int client_port = ntohs(client.sin_port);
							cout << "CLIENT IP: " << client_ip << endl;

							int numbytes;
							char childBuffer[BUFFERLENGTH];

							if((numbytes = recv(childSocket, childBuffer, BUFFERLENGTH-1, 0)) == -1)
							{
								perror("recv()");
								exit(1);
							}
							else
								cout << "Proceeding to retrieve the action from client" << endl;
								childBuffer[numbytes] = '\0';
								cout << "ACTION REQUESTED: " << childBuffer << endl;

								Message initMessage;
								initMessage.toMessage(childBuffer);


							switch (initMessage.getOpCode()){

								case GET: // GET
									{   

										cout << "CASE: GET" << endl;

										// cout<<sampleAnswer[string(client_ip)][1]<<endl;

										processGet(childSocket, client_ip, sampleAnswer);
										// Would you like me to do anything else for you?
										// if yes ==> do another iteration keeping connectionStatus = true
										// if no ==> set connection status = false
										break;
									}

								case PUT: // PUT
									{
										cout<<"CASE: PUT & LABEL: " << initMessage.getInfo() << endl;

										istringstream ss(initMessage.getInfo());

										int i;

										resolution.clear();
										
										while(ss >> i){
											resolution.push_back(i);

									        if (ss.peek() == '&')
									            ss.ignore();
											cout << "===================== TOKEN: " << i << endl;
										}

										width = resolution[0];
										height = resolution[1];

										cout << "WIDTH: " << width << " HEIGHT: " << height << endl;

										Message messageREADY("READY", "hi");

										string readyData = messageREADY.toString();

										send(childSocket,readyData.c_str(),readyData.size(),MSG_CONFIRM);

										processPut(childSocket, client_ip, sampleAnswer);
										// Would you like me to do anything else for you?
										// if yes ==> do another iteration keeping connectionStatus = true
										// if no ==> set connection status = false
										break;
									}

								default:
									{
										processDefault();
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

void readme()
{
	std::cout << "Need additional argument: PORT" << std::endl;
}

void write_log(){

}

// Write the information received from the client to the database

void writeDB(){

}

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


/*=====================================================
============ EXAMPLE OF SENDING OPENCV MAT ============
=======================================================
Mat frame;
frame = (frame.reshape(0,1)); // to make it continuous

int  imgSize = frame.total()*frame.elemSize();

// Send data here
bytes = send(clientSock, frame.data, imgSize, 0))
=====================================================*/

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

		string testStr = "hello world";
		string sample = "asddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddttttttttttttttttttdddddddddddddddddddddddddddddddddddddttttttttttttttttttdddddddddddddddddddddddddddddddddddddttttttttttttttttttttttttttttttttttttttttttyyyyyyyyyyyyyyxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkdddddddddddddssssssssssssssssssssssssssssssssssssssszzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhlllllllllllllllllllllllllllllllllllllllllllllr";
		int testInt = sample.size();

		Message newmessage("LEN",to_string(testInt));
		cout << "LEN: " << testInt << endl;

		string newTest = newmessage.toString();

		send(socket,newTest.c_str(),newTest.size(),MSG_CONFIRM);

		cout<<"New value sent\n";

		char toAnalyze[BUFFERLENGTH];
		int numbytes;

		if((numbytes = recv(socket, toAnalyze, BUFFERLENGTH-1, 0)) == -1){
			
			perror("recv()");
			exit(1);

		}
		else{

			printf("Proceeding to retrieve the information from client\n");
		
		}
		 
		toAnalyze[numbytes] = '\0';
		printf("The following has been received: %s \n", toAnalyze);

		Message switchMessage;
		switchMessage.toMessage(toAnalyze);

		switch(switchMessage.getOpCode()){
			case READY:
			{
				cout << "READY" << endl;
				Message newValueMessage("NEW_VALUE","300");
				string newValueTest = newValueMessage.toString();
				send(socket,newValueTest.c_str(),newValueTest.size(),MSG_CONFIRM);

				break;
			}
			default:
			{
				cout << "DEFAULT" << endl;
				break;
			}
		}

	}

}

// cv::namedWindow("SERVER",WINDOW_NORMAL);

void processPut(int socket, char *client_ip, map<string,vector<int> > sampleAnswer){
	// if() // check if the client_ip is in the map

	// char const *message = "Ready to receive information";
	// write(socket , message , strlen(message));
	// cout<<"Ready to receive information\n";

	char toAnalyze[BUFFERLENGTH];
	int numbytes;

	// if((numbytes = recv(socket, toAnalyze, BUFFERLENGTH-1, 0)) == -1)
	// {
	// 	perror("recv()");
	// 	exit(1);
	// }
	// else
	// 	printf("Proceeding to retrieve the information from client\n");

	cout << "GETTING THE IMAGE" << endl;

	Mat  img = Mat::zeros( width,height, CV_8UC3);

	cout << "DEBUG 1" << endl;
	
	int  imgSize = img.total()*img.elemSize();
	cout << "DEBUG 2" << endl;
	uchar sockData[imgSize];
	cout << "DEBUG 3" << endl;

	int bytes = 0;

	for (int i = 0; i < imgSize; i += bytes) {
	    if ((bytes = recv(socket, sockData +i, imgSize  - i, 0)) == -1)
	        exit(1);
	}
	cout << "DEBUG 4" << endl;

	// change the last loop to below statement

	Mat *img2 = new Mat(Size(width, height), CV_8UC3, sockData);
	cout << "DEBUG 5" << endl;

	if(img2->empty()){
		cout << "DID NOT PASS THE IS EMPTY TEST" << endl;
		return;
	}

	cout << "PASSED THE IS EMPTY TEST" << endl;

	imageMatch = match.findMatch(*img2);
	string hsvToSend;
	cout << ";;;;;;;;;;;;;;;;;;;;;;;;;;+++++++++++++++++;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;" << endl;
	if(match.publicMatch){
		vector<int> updatedHSV = match.getHSV();

		hsvToSend = to_string(updatedHSV[0])+"&"+to_string(updatedHSV[1])+"&"+to_string(updatedHSV[2])+"&"+to_string(updatedHSV[3])+"&"+to_string(updatedHSV[4])+"&"+to_string(updatedHSV[5]);
		cout << "UPDATE HSV = " << hsvToSend << endl;
	}
	else{
		cout << "OOPS SEEMS LIKE NO MATCH WAS FOUND" << endl;
	}
	cout << ";;;;;;;;;;;;;;;;;;;;;;;;;;+++++++++++++++++;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;" << endl;

	matchM = true;

	// Histogram histTest(imageMatch);
	// histTest.calcHis();
	// histTest.showHist();

	// Template testTPL(*img2);

	// Mat imageTPL = testTPL.getTemplate();
	// imshow("SERVER", *img2);
	// imshow("SERVER", imageMatch);
	// waitKey(1);

	delete img2;

	 
	toAnalyze[numbytes] = '\0';
	printf("The following has been received: %s \n", toAnalyze);

	string tests;
	if(match.publicMatch){
		Message testm("ACK",hsvToSend);
		tests = testm.toString();
	}
	else{
		Message testm("ACK","null");
		tests = testm.toString();
	}
	
	cout << "FINAL ACKNOWLEDGEMENT RESULT: " << tests << endl;

	send(socket , tests.c_str() , tests.size(), MSG_CONFIRM);

	// accept all the data from the clients
	// assign the data from clients into arrays based on the clients IP address
	// Communicate the stored information to the application
	// respond with an acknowledgement that data has been stored
	// LOG
}

void processDefault(){
	// attempt to send status of the data, whther it is currently available or not
	// Do not repeat the attempt
	// terminate
	// LOG
}