
#include "CameraLib.hpp"

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



    Camera:: nRet = MV_CODEREADER_OK;
    Camera::* handle = NULL;
	Camera:: bIsNormalRun = true;
    Camera:: g_bExit = false;

    int Camera ::g_nMaxImageSize = 0;
    MV_CODEREADER_IMAGE_OUT_INFO_EX2* Camera:: g_pstImageInfoEx2;
    char* Camera::g_pcDataBuf;

    pthread_mutex_t Camera::mutex;
    struct Camera::BcrInfo {
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
        // struct imageData
        // {
        //     unsigned int g_nMaxImageSize = 0;
        //     MV_CODEREADER_IMAGE_OUT_INFO_EX2* g_pstImageInfoEx2;
        //     unsigned char* g_pcDataBuf;
        // };

    int Camera::GB2312ToUTF8(char* szSrc, size_t iSrcLen, char* szDst, size_t iDstLen){
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
   BcrInfo Camera:: getData(MV_CODEREADER_RESULT_BCR_EX2* stBcrResult, char strChar[MAX_BCR_LEN]){
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
    BcrInfo Camera::resetData(){
        BcrInfo BI;
        BI.count = 0;
        return BI;
    }

    // ch:初始化资源用于存储图像信息 | en：Initialize resources to store the image
    int Camera:: InitResource(void* pHandle)
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

    

    int Camera:: init(){
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
    int Camera:: DeInitResources()
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

    cv::Mat Camera:: getImage(){
                // ch:开始取流 | en:Start grab image
        cv::Mat scaledImage;
        MV_CODEREADER_IMAGE_OUT_INFO_EX2 stImageInfo = {0};
        memset(&stImageInfo, 0, sizeof(MV_CODEREADER_IMAGE_OUT_INFO_EX2));
        unsigned char * pData = NULL;

        nRet = MV_CODEREADER_GetOneFrameTimeoutEx2(handle, &pData, &stImageInfo, 1000);
        printf("Get One Frame: nChannelID[%d] Width[%d], Height[%d], nFrameNum[%d], nTriggerIndex[%d]\n", 
                stImageInfo.nChannelID, stImageInfo.nWidth, stImageInfo.nHeight, stImageInfo.nFrameNum, stImageInfo.nTriggerIndex);


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

        float scaleddown = 0.1;
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



char* Camer:: getString() {
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



// int main(){
//     Camera cam1;
    
//     cam1.init();
//     usleep(1000000);
//     // cam1.getImage();
//     // cv::imshow("Image", cam1.getImage());
//     cv::Mat hasil = cam1.getImage();
//     cv::imshow("Image", hasil);
//     cv::waitKey(0); // Wait for a key press indefinitely
//     cv::destroyAllWindows(); // Close all OpenCV windows
    
//     // char* output = cam1.getString();
//     // std::cout<<"results: "<< output <<std::endl;
//     cam1.DeInitResources();
// }

// extern "C" {
//         Camera cam1;
        

//     int init_camera() {
//         return cam1.init();
//     }
//     const char* get_string() {
//         std::string str = cam1.getString();
//         char* cstr = new char[str.length() + 1]; // Allocate memory for C-style string
//         strcpy(cstr, str.c_str()); // Copy the content of std::string to char*
//         return cstr;
//     }
//     Camera::ImageInfo get_image(){
//         return cam1.getImage();
//     }
//     int close_camera(){
//         return cam1.DeInitResources();
//     }
// }