#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#include <cstdlib>
#include <string>
#include <iconv.h>

#include "MvCodeReaderParams.h"
#include "MvCodeReaderErrorDefine.h"
#include "MvCodeReaderCtrl.h"
#include "MvCodeReaderPixelType.h"
#include "turbojpeg.h"

#define Camera_Width             "WidthMax"
#define Camera_Height            "HeightMax"
#define Camera_PayloadSize       "PayloadSize"

// ch:中文转换条码长度定义 | en：Chinese coding format len
#define MAX_BCR_LEN  512

#define IMAGE_EX_LEN 4096
#define MAX_PATH     260

bool g_bExit = false;

unsigned int g_nMaxImageSize = 0;
MV_CODEREADER_IMAGE_OUT_INFO_EX2* g_pstImageInfoEx2;
unsigned char* g_pcDataBuf;

pthread_mutex_t mutex;

void DeInitResources(void);

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

void DeInitResources(void)
{
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
}

void SaveRawImage(void* pHandle)
{
	int nRet = MV_CODEREADER_OK;

	pthread_mutex_lock(&mutex);

    // 判断是否有有效数据
    if (NULL == g_pcDataBuf)
    {
		pthread_mutex_unlock(&mutex);
        printf("No valid image data，Save RAW failed!\n");
        return;
    }
    
    // 判断是否可保存Raw图
    if (PixelType_CodeReader_Gvsp_Mono8 != g_pstImageInfoEx2->enPixelType)
    {
        pthread_mutex_unlock(&mutex);
		printf("Unable to save Raw image!\n");
        return;
    }

    // 保存RAW图像
    FILE* pfile = NULL;
    char filename[MAX_PATH] = {0};
    time_t sNow;
    time(&sNow);                                     // 获取系统时间作为保存图片文件名
    tm sTime = *localtime(&sNow);
    sprintf(filename,("%.4d%.2d%.2d%.2d%.2d%.2d.raw"), (sTime.tm_year + 1900), (sTime.tm_mon + 1),
            sTime.tm_mday, sTime.tm_hour, sTime.tm_min, sTime.tm_sec);
    pfile = fopen(filename,"wb+");
    if(pfile == NULL)
    {
		pthread_mutex_unlock(&mutex);
        printf("Open file failed!\n");
        return ;
    }

    fwrite(g_pcDataBuf, 1, g_pstImageInfoEx2->nFrameLen, pfile);
	
	pthread_mutex_unlock(&mutex);
	
    printf("Save RAW image success!\n");

    if (NULL != pfile)
    {
        fclose (pfile);
        pfile = NULL;
    }
}

void SaveJPEGImage(void* pHandle)
{   
    
	int nRet = MV_CODEREADER_OK;
	
	pthread_mutex_lock(&mutex);

    // 判断是否有有效数据
    if (NULL == g_pcDataBuf)
    {
		pthread_mutex_unlock(&mutex);
        printf("No valid image data，Save JPG image failed!\n");
        return;
    }

    // 保存文件
    FILE* pfile = NULL;
    char filename[MAX_PATH] = {0};

    // 判断PixelType格式存图, 若Jpeg格式直接存图, Mono8格式转换存图
    if (PixelType_CodeReader_Gvsp_Jpeg == g_pstImageInfoEx2->enPixelType)
    {
        time_t sNow;
        time(&sNow);                                     // 获取系统时间作为保存图片文件名
        tm sTime = *localtime(&sNow);
        sprintf(filename,("%.4d%.2d%.2d%.2d%.2d%.2d.jpg"), (sTime.tm_year + 1900), (sTime.tm_mon + 1),
            sTime.tm_mday, sTime.tm_hour, sTime.tm_min, sTime.tm_sec);
        pfile = fopen(filename,"wb+");
	
        if(pfile == NULL)
        {
			pthread_mutex_unlock(&mutex);
			printf("Open file failed\n");
            return ;
        }
        fwrite(g_pcDataBuf, 1, g_pstImageInfoEx2->nFrameLen, pfile);
        printf("Save JPG image success!\n");
    }
    else
    {
        // 获取图像转换信息
        MV_CODEREADER_SAVE_IMAGE_PARAM_EX* pstParam = new MV_CODEREADER_SAVE_IMAGE_PARAM_EX;
        memset(pstParam, 0, sizeof(MV_CODEREADER_SAVE_IMAGE_PARAM_EX));
        pstParam->pData = g_pcDataBuf;
        pstParam->nDataLen = g_pstImageInfoEx2->nFrameLen;
        pstParam->nWidth = g_pstImageInfoEx2->nWidth;
        pstParam->nHeight = g_pstImageInfoEx2->nHeight;
        pstParam->enPixelType = g_pstImageInfoEx2->enPixelType;//PixelType_CodeReader_Gvsp_Mono8;
        pstParam->nBufferSize = g_nMaxImageSize;
        pstParam->nImageLen = 0;
        pstParam->enImageType = MV_CODEREADER_Image_Jpeg;
        pstParam->nJpgQuality = 60;

        // 保存JPG图像
        nRet = MV_CODEREADER_SaveImage(pHandle, pstParam);
        if (MV_CODEREADER_OK == nRet)
        {
            time_t sNow;
            time(&sNow);                                     // 获取系统时间作为保存图片文件名
            tm sTime = *localtime(&sNow);
            sprintf(filename,("%.4d%.2d%.2d%.2d%.2d%.2d.jpg"), (sTime.tm_year + 1900), (sTime.tm_mon + 1),
				sTime.tm_mday, sTime.tm_hour, sTime.tm_min, sTime.tm_sec);
            pfile = fopen(filename,"wb+");

            if(pfile == NULL)
            {
				pthread_mutex_unlock(&mutex);
                printf("Open file failed\n");
                return ;
            }

            fwrite(pstParam->pImageBuffer, 1, pstParam->nImageLen, pfile);
            printf("Save JPG image success!\n");
        }
        else
        {
            printf("Save JPG image failed! err code:%#x\n", nRet);
        }
		
		pthread_mutex_unlock(&mutex);
		
        if (NULL != pstParam)
        {
            delete pstParam;
            pstParam = NULL;
        }
    }

    if (NULL != pfile)
    {
        fclose (pfile);
        pfile = NULL;
    }
}

void SaveBMPImage(void* pHandle)
{
    int nRet = MV_CODEREADER_OK;
	
	pthread_mutex_lock(&mutex);

    // 判断是否有有效数据
    if (NULL == g_pcDataBuf)
    {
		pthread_mutex_unlock(&mutex);
		printf("No data, save bmp failed\n");
        return;
    }

    // 判断是否可保存Bmp图
    if (PixelType_CodeReader_Gvsp_Mono8 != g_pstImageInfoEx2->enPixelType)
    {
		pthread_mutex_unlock(&mutex);
        printf("Unable to save BMP image!\n");
        return;
    }

    FILE* pfile = NULL;
    char filename[MAX_PATH] = {0};

    MV_CODEREADER_SAVE_IMAGE_PARAM_EX* pstParam = new MV_CODEREADER_SAVE_IMAGE_PARAM_EX;
    memset(pstParam, 0, sizeof(MV_CODEREADER_SAVE_IMAGE_PARAM_EX));
    pstParam->pData = g_pcDataBuf;
    pstParam->nDataLen = g_pstImageInfoEx2->nFrameLen;
    pstParam->nWidth = g_pstImageInfoEx2->nWidth;
    pstParam->nHeight = g_pstImageInfoEx2->nHeight;
    pstParam->enPixelType = g_pstImageInfoEx2->enPixelType;
    pstParam->nBufferSize = g_nMaxImageSize;
    pstParam->nImageLen = 0;
    pstParam->enImageType = MV_CODEREADER_Image_Bmp;
    pstParam->nJpgQuality = 60;
    nRet = MV_CODEREADER_SaveImage(pHandle, pstParam);
    if (MV_CODEREADER_OK == nRet)
    {
        time_t sNow;
        time(&sNow);                                     // 获取系统时间作为保存图片文件名
        tm sTime = *localtime(&sNow);
        sprintf(filename,("%.4d%.2d%.2d%.2d%.2d%.2d.bmp"), (sTime.tm_year + 1900), (sTime.tm_mon + 1),
            sTime.tm_mday, sTime.tm_hour, sTime.tm_min, sTime.tm_sec);
        pfile = fopen(filename,"wb+");
        if(pfile == NULL)
        {
			pthread_mutex_unlock(&mutex);
			printf("Open file failed\n");
            return ;
        }

        fwrite(pstParam->pImageBuffer, 1, pstParam->nImageLen, pfile);
        printf("Save BMP image success!\n");
    }
    else
    {
        printf("Save BMP image failed! err code:%#x\n", nRet);
    }
	
	pthread_mutex_unlock(&mutex);

    if (NULL != pstParam)
    {
        delete pstParam;
        pstParam = NULL;
    }

    if (NULL != pfile)
    {
        fclose (pfile);
        pfile = NULL;
    }
}

// ch:处理用户按键输入，1保存raw格式图片，2保存jpeg格式图片，3保存bmp格式图片
// en:handle the key press event,key 1 for saving raw image, key 2 for saving jpeg image, key 3 for saving bmp image.
void HandleKeyPress(void* pHandle)
{
    int c;
    while ( (c = getchar()) != '\n' && c != EOF );
    fprintf( stderr, "\nPress 1 to save raw image, press 2 to save jpeg image, press 3 to save bmp image.Press Enter to exit.\n");
    while((c = getchar()) != '\n')
	{
		if('1' == c)
		{
			SaveRawImage(pHandle);
		}
		else if('2' == c)
		{
			SaveJPEGImage(pHandle);
		}
		else if('3' == c)
		{
			SaveBMPImage(pHandle);
		}
	}
    g_bExit = true;
    usleep(1);
}

// ch:等待用户输入enter键来结束取流或结束程序
// en:wait for user to input enter to stop grabbing or end the sample program
void PressEnterToExit(void)
{
    int c;
    while ( (c = getchar()) != '\n' && c != EOF );
    fprintf( stderr, "\nPress Enter to exit.\n");
    while( getchar() != '\n');
    g_bExit = true;
    usleep(1);
}

// ch:打印设备信息 | en:Print device Info
bool PrintDeviceInfo(MV_CODEREADER_DEVICE_INFO* pstMVDevInfo)
{
    if (NULL == pstMVDevInfo)
    {
        printf("The Pointer of pstMVDevInfo is NULL!\r\n");
        return false;
    }

    if (MV_CODEREADER_GIGE_DEVICE == pstMVDevInfo->nTLayerType)
    {
        int nIp1 = ((pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0xff000000) >> 24);
        int nIp2 = ((pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x00ff0000) >> 16);
        int nIp3 = ((pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x0000ff00) >> 8);
        int nIp4 = (pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x000000ff);

        // ch:打印当前相机ip和用户自定义名字 | en:print current ip and user defined name
        printf("CurrentIp: %d.%d.%d.%d\r\n" , nIp1, nIp2, nIp3, nIp4);
        printf("UserDefinedName: %s\r\n\n" , pstMVDevInfo->SpecialInfo.stGigEInfo.chUserDefinedName);
    }
    else if (MV_CODEREADER_USB_DEVICE == pstMVDevInfo->nTLayerType)
    {
        printf("UserDefinedName: %s\r\n\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.chUserDefinedName);
    }
    else
    {
        printf("Not support.\r\n");
    }

    return true;
}

// ch:获取图像线程 | en:Get Image Thread
static void* GrabImageThread(void* pUser)
{
    int nRet = MV_CODEREADER_OK;

    MV_CODEREADER_IMAGE_OUT_INFO_EX2 stImageInfo = {0};
    memset(&stImageInfo, 0, sizeof(MV_CODEREADER_IMAGE_OUT_INFO_EX2));
    unsigned char * pData = NULL;

    while(1)
    {
        if (g_bExit)
        {
            break;
        }

        nRet = MV_CODEREADER_GetOneFrameTimeoutEx2(pUser, &pData, &stImageInfo, 1000);
        if (nRet == MV_CODEREADER_OK)
        {
			pthread_mutex_lock(&mutex);
			
			if(NULL != g_pstImageInfoEx2)
			{
				memcpy(g_pstImageInfoEx2, &stImageInfo, sizeof(MV_CODEREADER_IMAGE_OUT_INFO_EX2));
			}
			
			if(NULL != g_pcDataBuf && stImageInfo.nFrameLen < g_nMaxImageSize)
			{
				memcpy(g_pcDataBuf, pData, stImageInfo.nFrameLen);
			}
			
			pthread_mutex_unlock(&mutex);
        }
    }

    return 0;

}

// ch:主处理函数 | en:main process
int main()
{
    int nRet = MV_CODEREADER_OK;
    void* handle = NULL;
	bool bIsNormalRun = true;

    do
    {
        MV_CODEREADER_DEVICE_INFO_LIST stDeviceList;
        memset(&stDeviceList, 0, sizeof(MV_CODEREADER_DEVICE_INFO_LIST));

        // ch:枚举设备 | Enum device
        nRet = MV_CODEREADER_EnumDevices(&stDeviceList, MV_CODEREADER_GIGE_DEVICE);
        if (MV_CODEREADER_OK != nRet)
        {
            printf("Enum Devices fail! nRet [%#x]\r\n", nRet);
            break;
        }
        else
        {
            printf("Enum Devices succeed!\r\n");
        }

        if (stDeviceList.nDeviceNum > 0)
        {
            for (unsigned int i = 0; i < stDeviceList.nDeviceNum; i++)
            {
                printf("[device %d]:\r\n", i);
                MV_CODEREADER_DEVICE_INFO* pDeviceInfo = stDeviceList.pDeviceInfo[i];
                if (NULL == pDeviceInfo)
                {
                    break;
                }
                PrintDeviceInfo(pDeviceInfo);
            }
        }
        else
        {
            printf("Find No Devices!\r\n");
            break;
        }

        printf("Please Intput camera index:");
        unsigned int nIndex = 0;
        scanf("%d", &nIndex);

        if (nIndex >= stDeviceList.nDeviceNum)
        {
            printf("Intput error!\r\n");
            break;
        }

        // ch:选择设备并创建句柄 | Select device and create handle
        nRet = MV_CODEREADER_CreateHandle(&handle, stDeviceList.pDeviceInfo[nIndex]);
        if (MV_CODEREADER_OK != nRet)
        {
            printf("Create Handle fail! nRet [%#x]\r\n", nRet);
            break;
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
            break;
        }
        else
        {
            printf("Open Device succeed!\r\n");
        }
		
		nRet = InitResource(handle);
		if (MV_CODEREADER_OK != nRet)
        {
            printf("InitResource fail! nRet [%#x]\r\n", nRet);
            break;
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
            break;
        }
        else
        {
            printf("Set Trigger Mode succeed!\r\n");
        }

        // ch:开始取流 | en:Start grab image
        nRet = MV_CODEREADER_StartGrabbing(handle);
        if (MV_CODEREADER_OK != nRet)
        {
            printf("Start Grabbing fail! nRet [%#x]\r\n", nRet);
            break;
        }
        else
        {
            printf("Start Grabbing succeed!\r\n");
        }

        pthread_t nThreadID;
		nRet = pthread_create(&nThreadID, NULL, GrabImageThread, handle);
        if (MV_CODEREADER_OK != nRet)
        {
            printf("Thread create failed! nRet [%d]\r\n", nRet);
            break;
        }

        HandleKeyPress(handle);
		
		nRet = pthread_join(nThreadID, NULL);
		if (MV_CODEREADER_OK != nRet)
        {
            printf("Thread free failed! nRet = [%d]\r\n", nRet);
			bIsNormalRun = false;
            break;
        }

        // ch:停止取流 | en:Stop grab image
        nRet = MV_CODEREADER_StopGrabbing(handle);
        if (MV_CODEREADER_OK != nRet)
        {
            printf("Stop Grabbing fail! nRet [%#x]\r\n", nRet);
			bIsNormalRun = false;
            break;
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
            break;
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
            break;
        }
        else
        {
			handle = NULL;
            printf("MV_CODEREADER_DestroyHandle succeed!\r\n");
        }

    } while (0);

    if (handle != NULL)
    {
        // ch:关闭设备 | en:Close device
        // ch:销毁句柄 | en:Destroy handle
        MV_CODEREADER_CloseDevice(handle);
        MV_CODEREADER_DestroyHandle(handle);
        handle = NULL;
    }
	
	DeInitResources();

	if (false == bIsNormalRun)
	{
		PressEnterToExit();
	}
	
	printf("Exit!\r\n");
	
    return 0;

}
