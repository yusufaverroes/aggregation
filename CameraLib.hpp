#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <pthread.h>
#include <opencv2/opencv.hpp>

class Camera {
private:
    int nRet;
    void* handle;
    bool bIsNormalRun;
    bool g_bExit;

    unsigned int g_nMaxImageSize;
    struct MV_CODEREADER_IMAGE_OUT_INFO_EX2* g_pstImageInfoEx2;
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
        } info[MAX_BCR_INFO];
        u_int8_t count;
    };

public:
    Camera();
    ~Camera();

    int init();
    int DeInitResources();
    cv::Mat getImage();
    char* getString();

private:
    int GB2312ToUTF8(char* szSrc, size_t iSrcLen, char* szDst, size_t iDstLen);
    BcrInfo getData(MV_CODEREADER_RESULT_BCR_EX2* stBcrResult, char strChar[MAX_BCR_LEN]);
    BcrInfo resetData();
    int InitResource(void* pHandle);
};

#endif // CAMERALIB_HPP
