#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#include <chrono>

#include <cstdlib>
#include <string>
#include <iconv.h>

#include "MvCodeReaderParams.h"
#include "MvCodeReaderErrorDefine.h"
#include "MvCodeReaderCtrl.h"
#include "MvCodeReaderPixelType.h"
#include "turbojpeg.h"

#include <opencv2/opencv.hpp>
#include <opencv2/highgui.hpp>

#define Camera_Width             "WidthMax"
#define Camera_Height            "HeightMax"
#define Camera_PayloadSize       "PayloadSize"

// ch:中文转换条码长度定义 | en：Chinese coding format len
#define MAX_BCR_LEN  512

#define IMAGE_EX_LEN 4096
#define MAX_PATH     260

const u_int8_t MAX_BCR_INFO = 100;

using namespace std::chrono;

class Camera {
private:
    int nRet = MV_CODEREADER_OK;
    void* handle = NULL;
	bool bIsNormalRun = true;
    bool g_bExit = false;

    unsigned int g_nMaxImageSize = 0;
    MV_CODEREADER_IMAGE_OUT_INFO_EX2* g_pstImageInfoEx2;
    unsigned char* g_pcDataBuf;

    pthread_mutex_t mutex;
        struct BcrInfo {
            struct {
                int xpoint1;
                int xpoint2;
                int xpoint3;
                int xpoint4;
                int ypoint1;
                int ypoint2;
                int ypoint3;
                int ypoint4;
                int center_x;
                int center_y;
                u_int8_t numBrcd;
                char* data;
            } info[MAX_BCR_INFO]; // Array to hold multiple sets of coordinates and numBrcd
            u_int8_t count; // Keep track of the number of elements in the array
        };
        struct imageData
        {
            unsigned int g_nMaxImageSize = 0;
            MV_CODEREADER_IMAGE_OUT_INFO_EX2* g_pstImageInfoEx2;
            unsigned char* g_pcDataBuf;
        };

    int GB2312ToUTF8(char* szSrc, size_t iSrcLen, char* szDst, size_t iDstLen){
    iconv_t cd = iconv_open("utf-8//IGNORE", "gb2312//IGNORE");
	if(0 == cd)
    {
		return -2;  
	}
		  
    memset(szDst, 0, iDstLen);
    char **src = &szSrc;
    char **dst = &szDst;
    if(-1 == (int)iconv(cd, src, &iSrcLen, dst, &iDstLen))
    {
		return -1; 
	}
		  
    iconv_close(cd);
    return 0;
}
   BcrInfo getData(MV_CODEREADER_RESULT_BCR_EX2* stBcrResult, char strChar[MAX_BCR_LEN]){
        BcrInfo BI;
        char *output_string;
        for (unsigned int i = 0; i < stBcrResult->nCodeNum; i++)
        {
            memset(strChar, 0, MAX_BCR_LEN);
            GB2312ToUTF8(stBcrResult->stBcrInfoEx2[i].chCode, strlen(stBcrResult->stBcrInfoEx2[i].chCode), strChar, MAX_BCR_LEN);
            
            BI.info[i].xpoint1 = stBcrResult->stBcrInfoEx2[i].pt[0].x;
            BI.info[i].xpoint2 = stBcrResult->stBcrInfoEx2[i].pt[1].x;
            BI.info[i].xpoint3 = stBcrResult->stBcrInfoEx2[i].pt[2].x;
            BI.info[i].xpoint4 = stBcrResult->stBcrInfoEx2[i].pt[3].x;
            BI.info[i].ypoint1 = stBcrResult->stBcrInfoEx2[i].pt[0].y;
            BI.info[i].ypoint2 = stBcrResult->stBcrInfoEx2[i].pt[1].y;
            BI.info[i].ypoint3 = stBcrResult->stBcrInfoEx2[i].pt[2].y;
            BI.info[i].ypoint4 = stBcrResult->stBcrInfoEx2[i].pt[3].y;
            BI.info[i].center_x = (stBcrResult->stBcrInfoEx2[i].pt[0].x + stBcrResult->stBcrInfoEx2[i].pt[1].x + 
                                stBcrResult->stBcrInfoEx2[i].pt[2].x + stBcrResult->stBcrInfoEx2[i].pt[3].x) / 4;
            BI.info[i].center_y = (stBcrResult->stBcrInfoEx2[i].pt[0].y + stBcrResult->stBcrInfoEx2[i].pt[1].y +
                                stBcrResult->stBcrInfoEx2[i].pt[2].y + stBcrResult->stBcrInfoEx2[i].pt[3].y) / 4;
            BI.info[i].numBrcd = i;
            
            BI.count = stBcrResult->nCodeNum;

            
            
                output_string = (char*)malloc(MAX_BCR_LEN * sizeof(char)); 
                if (output_string == NULL) {
                    printf("Memory allocation failed!\n");
            

                int result = snprintf(output_string, MAX_BCR_LEN, "%d:%s:%d;", i, strChar, stBcrResult->stBcrInfoEx2->nIDRScore);

                // Check if the formatting was successful and the string wasn't truncated
                if (result < 0 || result >= MAX_BCR_LEN) {
                    printf("Formatting error or string truncated!\n");
                    free(output_string);
                }

                printf("%d:%s:%d;\r\n",
                        i, strChar, stBcrResult->stBcrInfoEx2->nIDRScore);
            }
        }
        // free(output_string);

        return BI;
    }
    BcrInfo resetData(){
        BcrInfo BI;
        BI.count = 0;
        return BI;
    }

    // ch:初始化资源用于存储图像信息 | en：Initialize resources to store the image
    int InitResource(void* pHandle)
    {
        int nRet = MV_CODEREADER_OK;

        try
        {
            int nSensorWidth = 0;
            int nSensorHight = 0;

            // 获取Camera_PayloadSize
            MV_CODEREADER_INTVALUE_EX stParam;
            memset(&stParam, 0, sizeof(MV_CODEREADER_INTVALUE_EX));
            nRet = MV_CODEREADER_GetIntValue(pHandle, Camera_PayloadSize, &stParam);
            if (MV_CODEREADER_OK != nRet)
            {
                // 无该节点则适用最大宽高为payloadSize
                memset(&stParam, 0, sizeof(MV_CODEREADER_INTVALUE_EX));
                nRet = MV_CODEREADER_GetIntValue(pHandle, Camera_Width, &stParam);
                if (MV_CODEREADER_OK != nRet)
                {
                    printf("Get width failed! err code:%#x\n", nRet);
                    throw nRet;
                }
                nSensorWidth = stParam.nCurValue;

                memset(&stParam, 0, sizeof(MV_CODEREADER_INTVALUE_EX));
                nRet = MV_CODEREADER_GetIntValue(pHandle, Camera_Height, &stParam);
                if (MV_CODEREADER_OK != nRet)
                {
                    printf("Get hight failed! err code:%#x\n", nRet);
                    throw nRet;
                }
                nSensorHight = stParam.nCurValue;

                g_nMaxImageSize = nSensorWidth * nSensorHight + IMAGE_EX_LEN;
            }
            else
            {
                // 获取payloadSize成功
                g_nMaxImageSize = stParam.nCurValue + IMAGE_EX_LEN;
            }

            g_pcDataBuf =  (unsigned char*)malloc(g_nMaxImageSize);
            if (NULL == g_pcDataBuf)
            {
                nRet = MV_CODEREADER_E_RESOURCE;
                throw nRet;
            }
            memset(g_pcDataBuf, 0, g_nMaxImageSize);

            // 存储图像信息
            g_pstImageInfoEx2 = (MV_CODEREADER_IMAGE_OUT_INFO_EX2*)malloc(sizeof(MV_CODEREADER_IMAGE_OUT_INFO_EX2)); 
            if (NULL == g_pstImageInfoEx2)
            {
                nRet = MV_CODEREADER_E_RESOURCE;
                throw nRet;
            }
            memset(g_pstImageInfoEx2, 0, sizeof(MV_CODEREADER_IMAGE_OUT_INFO_EX2));
            
            pthread_mutex_init(&mutex, NULL);
        }
        catch (...)
        {
            DeInitResources();
            return nRet;
        }

        return nRet;
    }

    
public:
    int init(){
        MV_CODEREADER_DEVICE_INFO_LIST stDeviceList;
        memset(&stDeviceList, 0, sizeof(MV_CODEREADER_DEVICE_INFO_LIST));
        nRet = MV_CODEREADER_EnumDevices(&stDeviceList, MV_CODEREADER_GIGE_DEVICE);
            if (MV_CODEREADER_OK != nRet)
        {
            printf("Enum Devices fail! nRet [%#x]\r\n", nRet);
            ;
            return 0;
        }
        else
        {
            printf("Enum Devices succeed!\r\n");
        }
        nRet = MV_CODEREADER_CreateHandle(&handle, stDeviceList.pDeviceInfo[0]);
        if (MV_CODEREADER_OK != nRet)
        {
            printf("Create Handle fail! nRet [%#x]\r\n", nRet);
            return 0;
        }
        else
        {
            printf("Create Handle succeed!\r\n");
        }

        // ch:打开设备 | Open device
        nRet = MV_CODEREADER_OpenDevice(handle);
        if (MV_CODEREADER_OK != nRet)
        {
            printf("Open Device fail! nRet [%#x]\r\n", nRet);
            
        }
        else
        {
            printf("Open Device succeed!\r\n");
        }
		
		nRet = InitResource(handle);
		if (MV_CODEREADER_OK != nRet)
        {
            printf("InitResource fail! nRet [%#x]\r\n", nRet);
            return 0;
        }
        else
        {
            printf("InitResource succeed!\r\n");
        }

        // ch:设置触发模式为off | eb:Set trigger mode as off
        nRet = MV_CODEREADER_SetEnumValue(handle, "TriggerMode", MV_CODEREADER_TRIGGER_MODE_OFF);
        if (MV_CODEREADER_OK != nRet)
        {
            printf("Set Trigger Mode fail! nRet [%#x]\r\n", nRet);
            return 0;
        }
        else
        {
            printf("Set Trigger Mode succeed!\r\n");
        }
        nRet = MV_CODEREADER_StartGrabbing(handle);
        if (MV_CODEREADER_OK != nRet)
        {
            printf("Start Grabbing fail! nRet [%#x]\r\n", nRet);
            return 0;
        }
        else
        {
            printf("Start Grabbing succeed!\r\n");
        }
        return 1;


    }
    int DeInitResources()
    {
         nRet = MV_CODEREADER_StopGrabbing(handle);
        if (MV_CODEREADER_OK != nRet)
        {
            printf("Stop Grabbing fail! nRet [%#x]\r\n", nRet);
			bIsNormalRun = false;
            return 0;
            
        }
        else
        {
            printf("Stop Grabbing succeed!\r\n");
        }

        // ch:关闭设备 | en:close device
        nRet = MV_CODEREADER_CloseDevice(handle);
        if (MV_CODEREADER_OK != nRet)
        {
            printf("MV_CODEREADER_CloseDevice fail! nRet [%#x]\r\n", nRet);
			bIsNormalRun = false;
            return 0;
            
            
        }
        else
        {
            printf("MV_CODEREADER_CloseDevice succeed!\r\n");
        }

        // ch:销毁句柄 | en:Destroy handle
        nRet = MV_CODEREADER_DestroyHandle(handle);
        if (MV_CODEREADER_OK != nRet)
        {
            printf("MV_CODEREADER_DestroyHandle fail! nRet [%#x]\r\n", nRet);
			bIsNormalRun = false;
            return 0;
        
        }
        else
        {
			handle = NULL;
            printf("MV_CODEREADER_DestroyHandle succeed!\r\n");
        }
        if (NULL != g_pcDataBuf)
        {
            free(g_pcDataBuf);
            g_pcDataBuf = NULL;
        }
        
        if (NULL != g_pstImageInfoEx2)
        {
            free(g_pstImageInfoEx2);
            g_pstImageInfoEx2 = NULL;
        }
        
        pthread_mutex_destroy(&mutex);
        return 1;
    }

    cv::Mat getImage(){
                // ch:开始取流 | en:Start grab image
        cv::Mat scaledImage;
        MV_CODEREADER_IMAGE_OUT_INFO_EX2 stImageInfo = {0};
        memset(&stImageInfo, 0, sizeof(MV_CODEREADER_IMAGE_OUT_INFO_EX2));
        unsigned char * pData = NULL;

        nRet = MV_CODEREADER_GetOneFrameTimeoutEx2(handle, &pData, &stImageInfo, 1000);
        // printf("Get One Frame: nChannelID[%d] Width[%d], Height[%d], nFrameNum[%d], nTriggerIndex[%d]\n", 
                // stImageInfo.nChannelID, stImageInfo.nWidth, stImageInfo.nHeight, stImageInfo.nFrameNum, stImageInfo.nTriggerIndex);


        if (nRet == MV_CODEREADER_OK)
        {
			pthread_mutex_lock(&mutex);
			MV_CODEREADER_RESULT_BCR_EX2* stBcrResult = (MV_CODEREADER_RESULT_BCR_EX2*)stImageInfo.UnparsedBcrList.pstCodeListEx2;
			if(NULL != g_pstImageInfoEx2)
			{
				memcpy(g_pstImageInfoEx2, &stImageInfo, sizeof(MV_CODEREADER_IMAGE_OUT_INFO_EX2));
			}
			
			if(NULL != g_pcDataBuf && stImageInfo.nFrameLen < g_nMaxImageSize)
			{
				memcpy(g_pcDataBuf, pData, stImageInfo.nFrameLen);
			}

            pthread_mutex_unlock(&mutex);
           
        cv::Mat jpegImage;

        float scaleddown = 0.2;
        // Read JPEG image data into OpenCV Mat
        jpegImage = cv::imdecode(cv::Mat(1,g_pstImageInfoEx2->nFrameLen, CV_8UC1, g_pcDataBuf), cv::IMREAD_COLOR);

        // Scale down the image
        cv::resize(jpegImage, scaledImage, cv::Size(), scaleddown, scaleddown); // Scale down by 50% (adjust as needed)
        char strChar[MAX_BCR_LEN] = {0};
        BcrInfo BI;
        BI = getData(stBcrResult, strChar);
        for (u_int8_t i = 0; i < BI.count; i++)
            {
                // Get the points for drawing polygon
                std::vector<cv::Point> points;
                points.push_back(cv::Point(BI.info[i].xpoint1*scaleddown, BI.info[i].ypoint1*scaleddown));
                points.push_back(cv::Point(BI.info[i].xpoint2*scaleddown, BI.info[i].ypoint2*scaleddown));
                points.push_back(cv::Point(BI.info[i].xpoint3*scaleddown, BI.info[i].ypoint3*scaleddown));
                points.push_back(cv::Point(BI.info[i].xpoint4*scaleddown, BI.info[i].ypoint4*scaleddown));

                // Draw the polygon
                cv::polylines(scaledImage, points, true, cv::Scalar(255, 255, 0), 2);
            }

			// //pthread_mutex_unlock(&mutex);
            
        }
        return scaledImage;
        
    }



char* getString() {
    char* result_string = NULL;
    char temp_string[MAX_BCR_LEN] = {0}; // Temporary string buffer
    int total_length = 0; // Total length of the result string
    int nnRet = MV_CODEREADER_OK;
    MV_CODEREADER_IMAGE_OUT_INFO_EX2 stImageInfo = {0};
    memset(&stImageInfo, 0, sizeof(MV_CODEREADER_IMAGE_OUT_INFO_EX));
    unsigned char * pData = NULL;

        nnRet = MV_CODEREADER_GetOneFrameTimeoutEx2(handle, &pData, &stImageInfo, 1000);
        if (nnRet == MV_CODEREADER_OK)
        {
            MV_CODEREADER_RESULT_BCR_EX* stBcrResult = (MV_CODEREADER_RESULT_BCR_EX*)stImageInfo.pstCodeListEx;

            printf("Get One Frame: nChannelID[%d] Width[%d], Height[%d], nFrameNum[%d], nTriggerIndex[%d]\n", 
                stImageInfo.nChannelID, stImageInfo.nWidth, stImageInfo.nHeight, stImageInfo.nFrameNum, stImageInfo.nTriggerIndex);

            printf("CodeNum[%d]\n", stBcrResult->nCodeNum);

			char strChar[MAX_BCR_LEN] = {0};
            

            

            for (int i = 0; i < stBcrResult->nCodeNum; i++) {
                memset(strChar, 0, MAX_BCR_LEN);
                GB2312ToUTF8(stBcrResult->stBcrInfoEx[i].chCode, strlen(stBcrResult->stBcrInfoEx[i].chCode), strChar, MAX_BCR_LEN);

                
                // Allocate memory for output_string
                char* output_string = (char*)malloc(MAX_BCR_LEN * sizeof(char)); 
                if (output_string == NULL) {
                    printf("Memory allocation failed!\n");
                    continue; // Skip to the next iteration
                }

                // Format the output string
                int result = snprintf(output_string, MAX_BCR_LEN, "%d:%s:%d;", i, strChar, stBcrResult->stBcrInfoEx->nIDRScore);
                
                // Check if the formatting was successful and the string wasn't truncated
                if (result < 0 || result >= MAX_BCR_LEN) {
                    printf("Formatting error or string truncated!\n");
                    free(output_string);
                    continue; // Skip to the next iteration
                }

                // Concatenate the output_string with the result_string
                strcat(temp_string, output_string);
                total_length += strlen(output_string);
                free(output_string); // Free the allocated memory for output_string
            }
        }
        else{printf("failed");}
    // Allocate memory for the result_string
    result_string = (char*)malloc((total_length + 1) * sizeof(char));
    if (result_string == NULL) {
        printf("Memory allocation failed for result string!\n");
        return NULL;
    }

    // Copy the content of temp_string to result_string
    strcpy(result_string, temp_string);

    return result_string;
}

};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <iostream>
#include <map>
#include <set>
//#include <opencv2/opencv.hpp>

#include <unistd.h>
#include <websocketpp/common/thread.hpp>

typedef websocketpp::server<websocketpp::config::asio> server;

using websocketpp::connection_hdl;
using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;

using websocketpp::lib::condition_variable;
using websocketpp::lib::lock_guard;
using websocketpp::lib::mutex;
using websocketpp::lib::thread;
using websocketpp::lib::unique_lock;

/* on_open insert connection_hdl into channel
 * on_close remove connection_hdl from channel
 * on_message queue send to all channels
 */

enum action_type
{
    SUBSCRIBE,
    UNSUBSCRIBE,
    MESSAGE
};

struct action
{
    action(action_type t, connection_hdl h) : type(t), hdl(h) {}
    action(action_type t, connection_hdl h, server::message_ptr m)
        : type(t), hdl(h), msg(m) {}

    action_type type;
    websocketpp::connection_hdl hdl;
    server::message_ptr msg;
};
Camera cam1;
class broadcast_server
{
public:
    broadcast_server()
    {
        // Initialize Asio Transport
        m_server.init_asio();

        // Register handler callbacks
        m_server.set_open_handler(bind(&broadcast_server::on_open, this, ::_1));
        m_server.set_close_handler(bind(&broadcast_server::on_close, this, ::_1));
        m_server.set_message_handler(bind(&broadcast_server::on_message, this, ::_1, std::placeholders::_2));
    }

    void run(uint16_t port)
    {
        // listen on specified port
        m_server.listen(port);

        // Start the server accept loop
        m_server.start_accept();

        // Start the ASIO io_service run loop
        try
        {
            m_server.run();
        }
        catch (const std::exception &e)
        {
            std::cout << e.what() << std::endl;
        }
    }

    void on_open(connection_hdl hdl)
    {
        server::connection_ptr con = m_server.get_con_from_hdl(hdl);
        std::string client_id = con->get_request_header("Client-ID");

        {
            lock_guard<mutex> guard(m_action_lock);
            if(client_id.length()>0){
                m_connections[hdl] = client_id; // Associate the client's connection handle with the provided unique identifier
            }else{
                m_connections[hdl] = "client1";
            }
            std::cout << "Client connected with ID: " << client_id << std::endl;
        }

        m_action_cond.notify_one();
    }

    void on_close(connection_hdl hdl)
    {
        {
            
            lock_guard<mutex> guard(m_action_lock);
            m_connections.erase(hdl);
            std::cout << "Client disconnected, connection id : "<< m_connections[hdl] << std::endl;
        }

        m_action_cond.notify_one();
    }

    void on_message(connection_hdl hdl, server::message_ptr msg)
    {
        // queue message up for sending by processing thread
        {
            lock_guard<mutex> guard(m_action_lock);
            // std::cout << "on_message" << std::endl;
            m_actions.push(action(MESSAGE, hdl, msg));
        }
        m_action_cond.notify_one();
    }
    // void looping(){
    //         while(1){
    //         // if (!stop_streaming)
    //         // {
    //             // if (b!=NULL && !b->hdl.expired()){
    //                 if (b!=NULL ){
    //                 con_list::iterator it = m_connections.find(b->hdl);
                    
    //                 if (it != m_connections.end())
    //                 {
    //                     std::string client_id = it->second;
    //                     // if (client_id == "client1" && streaming_to_client1)
    //                     if (stop_streaming==false && get_data==false) 
    //                     {   
    //                         // cv::imshow("test",cam1.getImage());
    //                         // cv::waitKey(0);
    //                         std::vector<unsigned char> buf;
    //                         imencode(".jpeg", cam1.getImage(), buf); 
    //                         // m_server.send(b->hdl, "a", websocketpp::frame::opcode::text);
    //                         m_server.send(b->hdl,  buf.data(), buf.size(), websocketpp::frame::opcode::binary);
    //                     }
    //                     else if (get_data==true){   
    //                             if (client2_data_count < max_client2_data_count){
    //                             m_server.send(b->hdl, cam1.getString(), websocketpp::frame::opcode::text);
    //                             client2_data_count++;
    //                             }else{
    //                                 stop_streaming=false;
    //                                 get_data=false;
    //                                 client2_data_count=0;
    //                             }
                                
    //                     }
    //                     // else if (client_id == "client2" && streaming_to_client2)
    //                     // {
    //                     //     if (client2_data_count < max_client2_data_count)
    //                     //     {
    //                     //         //get string data here
    //                     //         // m_server.send(b->hdl, "b", websocketpp::frame::opcode::text);
    //                     //         m_server.send(b->hdl, cam1.getString(), websocketpp::frame::opcode::text);
    //                     //         client2_data_count++;
    //                     //     }
    //                     //     else
    //                     //     {
    //                     //         streaming_to_client2 = false;
    //                     //         streaming_to_client1 = true;
    //                     //     }
    //                     // }
    //                 }
    //             // }
    //             // else
    //             // {
    //             //     streaming_to_client1 = false;
    //             //     streaming_to_client2 = false;
    //             //     stop_streaming = false;
    //             // }
    //         }
    //         //Rey delay
    //         //usleep(1000000);
    //         //Rizal Delay
    //         usleep(100000);    
    //     }
    // }

    void looping()
    {
        while (1)
        {
            if (!stop_streaming)
            {
                if (b != NULL)
                { 

                    for (auto it = m_connections.begin(); it != m_connections.end(); ++it)
                    {
                        std::string client_id = it->second;
                        if (client_id == "client1" && streaming_to_client1)
                        { // get image data here
                            std::vector<unsigned char> buf;
                            imencode(".jpeg", cam1.getImage(), buf); 
                             // m_server.send(b->hdl, "a", websocketpp::frame::opcode::text);
                            m_server.send(it->first,  buf.data(), buf.size(), websocketpp::frame::opcode::binary);
                            // m_server.send(it->first, "a", websocketpp::frame::opcode::text);
                        }
                        else if (client_id == "client2" && streaming_to_client2)
                        {
                            if (client2_data_count < max_client2_data_count)
                            {
                                // get string data here
                                m_server.send(it->first, cam1.getString(), websocketpp::frame::opcode::text);
                                // m_server.send(it->first, "b", websocketpp::frame::opcode::text);
                                client2_data_count++;
                            }
                            else
                            {
                                streaming_to_client2 = false;
                                streaming_to_client1 = true;
                                client2_data_count = 0;
                            }
                        }
                    }
                }
                else
                {
                    // streaming_to_client1 = false;
                    // streaming_to_client2 = false;
                    // stop_streaming = false;
                }
            }
            usleep(500000);
        }
    }

    void process_messages()
    {
        while (1)
        {

            unique_lock<mutex> lock(m_action_lock);

            while (m_actions.empty())
            {
                m_action_cond.wait(lock);
            }

            action a = m_actions.front();

            m_actions.pop();
            b = &a;
            lock.unlock();

            if (a.type == SUBSCRIBE)
            {
                // Handle client subscription
            }
            else if (a.type == UNSUBSCRIBE)
            {
                // Handle client unsubscription
            }
            else if (a.type == MESSAGE)
            {
                std::string message = a.msg->get_payload();

                // if (message == "stop_streaming")
                // {
                //     stop_streaming = true;
                // }
                // else if (message=="start_stream")
                // {
                //     // streaming_to_client1 = false;
                //     // streaming_to_client2 = true;
                //     // client2_data_count = 0;
                //     stop_streaming = false;
                // }else if(message=="get_data"){
                //     stop_streaming = true;
                //     get_data=true;
                // }
                if (message == "stop_streaming")
                {
                    stop_streaming = true;
                }
                else if (message == "get_data")
                {
                    streaming_to_client1 = false;
                    streaming_to_client2 = true;
                    client2_data_count = 0;
                }else if (message == "start_streaming")
                {
                    stop_streaming=false;
                    streaming_to_client1 = true;
                }
     
            }

            // if (!stop_streaming) // TODO : spllit into another thread from here, to make looping
            // {
            //     con_list::iterator it = m_connections.find(a.hdl);
            //     if (it != m_connections.end())
            //     {
            //         std::string client_id = it->second;
            //         if (client_id == "client1" && streaming_to_client1)
            //         {
            //             m_server.send(a.hdl, "a", websocketpp::frame::opcode::text);
            //         }
            //         else if (client_id == "client2" && streaming_to_client2)
            //         {
            //             if (client2_data_count < max_client2_data_count)
            //             {
            //                 m_server.send(a.hdl, "b", websocketpp::frame::opcode::text);
            //                 client2_data_count++;
            //             }
            //             else
            //             {
            //                 streaming_to_client2 = false;
            //                 streaming_to_client1 = true;
            //             }
            //         }
            //     }
            // }
            // else
            // {
            //     streaming_to_client1 = false;
            //     streaming_to_client2 = false;
            //     stop_streaming = false;
            // }
        }
    }

private:
    typedef std::map<connection_hdl, std::string, std::owner_less<connection_hdl>> con_list;

    server m_server;
    con_list m_connections;
    std::queue<action> m_actions;

    mutex m_action_lock;
    mutex m_connection_lock;
    condition_variable m_action_cond;
    action* b;

    bool streaming_to_client1 = true;
    bool streaming_to_client2 = false;
    bool stop_streaming = false;
    bool get_data=false;
    
    int client2_data_count = 0;
    const int max_client2_data_count = 1;
};

int main()
{
    try
    {   
        cam1.init();
        usleep(5000000);
        broadcast_server server_instance;
        
        // Start a thread to run the processing loop

        thread t(bind(&broadcast_server::process_messages, &server_instance));
        thread l(bind(&broadcast_server::looping, &server_instance));

        // Run the ASIO io_service with the main thread
        server_instance.run(9002);
        std::cout<<"server run"<<std::endl;

        // Wait for the processing thread to finish
        t.join();
        l.join();
        cam1.DeInitResources();
    }
    catch (websocketpp::exception const &e)
    {
        std::cout << e.what() << std::endl;
        cam1.DeInitResources();
    }
    cam1.DeInitResources();
    return 0;
}
