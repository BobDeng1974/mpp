#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include "sample_comm.h"
#include "mytrace.h"
VIDEO_NORM_E gs_enNorm = VIDEO_ENCODING_MODE_NTSC;
PIC_SIZE_E enSize = PIC_HD1080;
HI_BOOL isSnap = HI_FALSE, isSend = HI_FALSE;
HI_BOOL isThreadStop = HI_FALSE;
HI_BOOL isChangePic = HI_FALSE;

const char *videodir = "/mnt/sd/video/";
const char *snapdir = "/mnt/sd/snap/";
const char *protect = "/mnt/sd/protect/";

void FindLastFile()
{
    DIR *pdir;
    struct dirent *pdirent;
    struct stat statbuf;
    char file[100] = {};
    char f[100] = {};
    // strcpy(f,"/home/video/");
    while (1)
    {
        long temp = 0;
        int sign = 1;
        int num = 0;
        long time = 0;
        pdir = opendir(videodir);
        while ((pdirent = readdir(pdir)) != NULL)
        {
            memset(f, 0, sizeof(file) / sizeof(char));
            strcpy(f, videodir);
           // strcpy(f, "/mnt/sd/video/\0");
            strcat(f, pdirent->d_name);
            stat(f, &statbuf);
            if (S_ISREG(statbuf.st_mode))
            {
                if (sign == 1)
                {
                    sign = 0;
                    temp = statbuf.st_mtime;
                    strcpy(file,f);
                }
                num++;
                if (statbuf.st_mtime < temp)
                {
                    temp = statbuf.st_mtime;
                    memset(file, 0, sizeof(file) / sizeof(char));
                    strcpy(file, f);
                }
            }
        }
        if (num > 10)
        {
            printf("file:%s,%ld\n", file, temp);
            remove(file);
        }
        closedir(pdir);
        sleep(1);
    }
}

void record()
{
    HI_S32 s32Ret;
    VPSS_GRP VpssGrp;
    VPSS_CHN VpssChn;
    VENC_GRP VideoVencGrp;
    VENC_CHN VideoVencChn;
    PAYLOAD_TYPE_E enPayLoad = PT_H264;
    SAMPLE_RC_E enRcMode = SAMPLE_RC_CBR;
    HI_S32 Vencfd, maxfd = 0;
    fd_set read_fds;
    VENC_STREAM_S stStream;
    VENC_CHN_STAT_S stStat;
    struct timeval TimeoutVal;
    HI_S32 i;
    FILE* pFile;
    time_t oldtime;
    HI_CHAR aszFileName[128];
    time_t timep;
    struct tm *timenow;
    char timestr[50];

    HI_S32 s32ChnNum = 1;
    VpssGrp = 0;
    VpssChn = 0;
    VideoVencGrp = 0;
    VideoVencChn = 0;


    printf("now start recording....\n");
    while (!isThreadStop)
    {
        s32Ret = SAMPLE_COMM_VENC_Start(VideoVencGrp, VideoVencChn, enPayLoad,
                                        gs_enNorm, enSize, enRcMode);
        if (HI_SUCCESS != s32Ret)
        {
            printf("SAMPLE_COMM_VENC_Start failed with err code %#x\n", s32Ret);
            goto VENC_STOP;
        }
        s32Ret = SAMPLE_COMM_VENC_BindVpss(VideoVencGrp, VideoVencChn, VpssChn);
        if (HI_SUCCESS != s32Ret)
        {
            printf("SAMPLE_COMM_VENC_BindVpss failed with err code %#x\n", s32Ret);
            goto VENC_STOP;
        }
        /*       s32Ret = SAMPLE_COMM_VENC_StartGetStream(s32ChnNum);
               if (HI_SUCCESS != s32Ret)
               {
                   printf("SAMPLE_COMM_VENC_StartGetStream failed with err code %#x\n", s32Ret);
                   goto VENC_STOP;
               }
               int sret = sleep(60);

               SAMPLE_COMM_VENC_StopGetStream();*/
        Vencfd = HI_MPI_VENC_GetFd(VideoVencChn);
        if (Vencfd < 0)
        {
            printf("HI_MPI_VENC_GetFd failed with %#x!\n", Vencfd);
        }

        oldtime = time(NULL);
        timenow = localtime(&oldtime);
        strftime(timestr,sizeof(timestr),"%b_%d_%H_%M_%S",timenow);
        sprintf(aszFileName,"%s%s.h264",videodir,timestr);
        printf("filename:%s\n",aszFileName);
        pFile = fopen(aszFileName,"wb");
        while (time(NULL) - oldtime < 10)
        {
            FD_ZERO(&read_fds);
            FD_SET(Vencfd, &read_fds);
            TimeoutVal.tv_sec = 20000;
            TimeoutVal.tv_usec = 0;
            s32Ret = select(Vencfd + 1, &read_fds, NULL, NULL, &TimeoutVal);
            if (s32Ret < 0)
            {
                perror("select failed !\n");
                break;
            }
            else if (s32Ret == 0)
            {
                printf("get stream time out, exit thread\n");
                continue;
            }
            else
            {
                if (FD_ISSET(Vencfd, &read_fds))
                {
                    memset(&stStream, 0, sizeof(stStream));
                    s32Ret = HI_MPI_VENC_Query(VideoVencChn, &stStat);
                    if (HI_SUCCESS != s32Ret)
                    {
                        printf("HI_MPI_VENC_Query failed with err code %#x!\n", s32Ret);
                        break;
                    }
                    stStream.pstPack = (VENC_PACK_S*)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
                    if (NULL == stStream.pstPack)
                    {
                        perror("malloc stream pack failed");
                        break;
                    }
                    stStream.u32PackCount = stStat.u32CurPacks;
                    s32Ret = HI_MPI_VENC_GetStream(VideoVencChn, &stStream, HI_TRUE);
                    if (HI_SUCCESS != s32Ret)
                    {
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        printf("HI_MPI_VENC_GetStream failed with %#x!\n", s32Ret);
                        break;
                    }
                    /*      for (i = 0; i < stStream.u32PackCount; i++)
                          {
                              fwrite();
                              if (stStream.pstPack[i].u32Len[i] > 0)
                              {
                                  fwrite();
                              }
                          }*/
                    s32Ret = SAMPLE_COMM_VENC_SaveStream(enPayLoad, pFile, &stStream);
                    if (HI_SUCCESS != s32Ret)
                    {
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        printf("save stream failed !\n");
                        break;
                    }
                    s32Ret = HI_MPI_VENC_ReleaseStream(VideoVencChn, &stStream);
                    if (HI_SUCCESS != s32Ret)
                    {
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                    }
                    free(stStream.pstPack);
                    stStream.pstPack = NULL;
                }
            }
        }
        fclose(pFile);

VENC_STOP:
        VpssGrp = 0;
        VpssChn = 0;
        VideoVencGrp = 0;
        VideoVencChn = 0;
        SAMPLE_COMM_VENC_UnBindVpss(VideoVencGrp, VpssGrp, VpssChn);
        SAMPLE_COMM_VENC_Stop(VideoVencGrp, VideoVencChn);
    }

}

void SendVideoStream(int SendSockfd)
{
    HI_S32 s32Ret;
    VENC_GRP SendVencGrp = 2;
    VENC_CHN SendVencChn = 2;
    VPSS_GRP VpssGrp = 0;
    VPSS_CHN VpssChn = 0;
    HI_S32 Vencfd, maxfd = 0;
    PAYLOAD_TYPE_E enPayLoad = PT_H264;
    SAMPLE_RC_E enRcMode = SAMPLE_RC_CBR;
    struct timeval TimeoutVal;
    fd_set read_fds;
    int connfd;
    VENC_STREAM_S stStream;
    VENC_CHN_STAT_S stStat;
    HI_S32 i;
    while (!isThreadStop)
    {
        if (!isChangePic)
        {
            connfd = accept(SendSockfd, NULL, NULL);
            printf("connect!\n");
        }
        isChangePic = HI_FALSE;
        s32Ret = SAMPLE_COMM_VENC_Start(SendVencGrp, SendVencChn, enPayLoad,
                                        gs_enNorm, enSize, enRcMode);
        if (HI_SUCCESS != s32Ret)
        {
            printf("SAMPLE_COMM_VENC_Start failed with err code %#x\n", s32Ret);
            continue;
        }
        s32Ret = SAMPLE_COMM_VENC_BindVpss(SendVencGrp, VpssGrp, VpssChn);
        if (HI_SUCCESS != s32Ret)
        {
            printf("SAMPLE_COMM_VENC_BindVpss failed with err code %#x\n", s32Ret);
            continue;
        }
        Vencfd = HI_MPI_VENC_GetFd(SendVencChn);
        if (Vencfd < 0)
        {
            printf("HI_MPI_VENC_GetFd faild with%#x!\n", Vencfd);
            return HI_FAILURE;
        }
        while (isSend)
        {

            FD_ZERO(&read_fds);
            FD_SET(Vencfd, &read_fds);
            TimeoutVal.tv_sec = 20000;
            TimeoutVal.tv_usec = 0;
            s32Ret = select(Vencfd + 1, &read_fds, NULL, NULL, &TimeoutVal);

            if (s32Ret < 0)
            {
                perror("select failed!\n");
                break;
            }
            else if (s32Ret == 0)
            {
                printf("get sendvenc stream time out,exit thread\n");
                continue;
            }
            else
            {
                if (FD_ISSET(Vencfd, &read_fds))
                {
                    memset(&stStream, 0, sizeof(stStream));
                    s32Ret = HI_MPI_VENC_Query(2, &stStat);
                    if (HI_SUCCESS != s32Ret)
                    {
                        printf("HI_MPI_VENC_Query failed with err code %#x!\n", s32Ret);
                        break;
                    }
                    stStream.pstPack = (VENC_PACK_S*)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
                    if (NULL == stStream.pstPack)
                    {
                        printf("malloc stream pack failed!\n");
                        break;
                    }
                    stStream.u32PackCount = stStat.u32CurPacks;
                    s32Ret = HI_MPI_VENC_GetStream(2, &stStream, HI_TRUE);
                    if (HI_SUCCESS != s32Ret)
                    {
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        printf("HI_MPI_VENC_GetStream failed with %#x!\n", s32Ret);
                        break;
                    }
                    for (i = 0; i < stStream.u32PackCount; i++)
                    {
                        write(connfd, stStream.pstPack[i].pu8Addr[0],
                              stStream.pstPack[i].u32Len[0]);
                        if (stStream.pstPack[i].u32Len[1] > 0)
                        {
                            write(connfd, stStream.pstPack[i].pu8Addr[1],
                                  stStream.pstPack[i].u32Len[1]);
                        }
                    }
                    s32Ret = HI_MPI_VENC_ReleaseStream(2, &stStream);
                    if (HI_SUCCESS != s32Ret)
                    {
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        break;
                    }
                    free(stStream.pstPack);
                    stStream.pstPack = NULL;
                }
            }
        }
        SAMPLE_COMM_VENC_StopGetStream();
        SAMPLE_COMM_VENC_UnBindVpss(SendVencGrp, VpssGrp, VpssChn);
        SAMPLE_COMM_VENC_Stop(SendVencGrp, SendVencChn);
        usleep(100);
    }
}

int protocol(char buff[], int n)
{
    int i;
    char *s = "***";
    char *e = "###";
    char *p = buff;
    if (strstr(p, s) != 0 && strstr(p, e) != 0)
    {
        for (i = 0; i < n; i++)
            buff[i] = buff[i + 3];
        buff[n - 6] = buff[n];
    }
    else
        return -1;
    return 0;
}

void accept_thread(int sockfd)
{
    char buff[100];
    while (!isThreadStop)
    {
#if 0
        int connfd = accept(sockfd, NULL, NULL);

        if (connfd == -1)
        {
            perror("accept failed");
            continue;
        }
        int n = recv(connfd, buff, sizeof(buff) / sizeof(char), 0);
        buff[n] = '\0';
        close(connfd);
        // printf("buff:%s\n",buff);
        protocol(buff, strlen(buff));
        // printf("buff:%s\n",buff);
        if (!strcmp(buff, "snap"))
        {
            isSnap = HI_TRUE;
            printf("snap!\n");
        }
        else if (!strcmp(buff, "1080P"))
        {
            enSize = PIC_HD1080;
            if (isSend == HI_TRUE)
                isChangePic = HI_TRUE;
            printf("1080P\n");
        }
        else if (!strcmp(buff, "720P"))
        {
            enSize = PIC_HD720;
            if (isSend == HI_TRUE)
                isChangePic = HI_TRUE;
            printf("720P\n");
        }
        else if (!strcmp(buff, "VGA"))
        {
            enSize = PIC_VGA;
            if (isSend == HI_TRUE)
                isChangePic = HI_TRUE;
            printf("VGA\n");
        }
        else if (!strcmp(buff, "send"))
        {
            isSend = HI_TRUE;
            printf("send\n");
        }
#endif
        getchar();
        isSnap = HI_TRUE;
    }
}

HI_VOID SnapPic()
{
    VENC_GRP SnapVencGrp = 1;
    VENC_CHN SnapVencChn = 1;
    while (!isThreadStop)
    {
        if (isSnap)
        {
            HI_S32 s32Ret = SAMPLE_COMM_VENC_SnapProcess(SnapVencGrp, SnapVencChn);
            if (HI_SUCCESS != s32Ret)
            {
                printf("%s: sanp process failed!\n", __FUNCTION__);
                break;
            }
            isSnap = HI_FALSE;
        }
        usleep(100);
    }
}
int main(int argc, char const *argv[])
{
    PAYLOAD_TYPE_E enPayLoad = PT_H264;
    VB_CONF_S stVbConf;
    SAMPLE_VI_CONFIG_S stViConfig;
    VPSS_GRP VpssGrp;
    VPSS_CHN VpssChn;
    VPSS_GRP_ATTR_S stVpssGrpAttr;
    VPSS_CHN_ATTR_S stVpssChnAttr;
    VPSS_CHN_MODE_S stVpssChnMode;
    VPSS_EXT_CHN_ATTR_S stVpssExtChnAttr;
    VENC_GRP VideoVencGrp, SendVencGrp, SnapVencGrp;
    VENC_CHN VideoVencChn, SendVencChn, SnapVencChn;
    SAMPLE_RC_E enRcMode = SAMPLE_RC_CBR;
    HI_S32 s32ChnNum = 1;
    HI_S32 s32Ret = HI_SUCCESS;
    HI_U32 u32BlkSize;
    SIZE_S stSize, JpegSize;
    int sockfd, SendSockfd;
    struct sockaddr_in servaddr, SendServaddr;
    pid_t snappid, acceptpid;
    int ret;
    pthread_t snaptid, accepttid, filetid, sendvideotid, recordtid;

    //配置缓冲池参数
    memset(&stVbConf, 0, sizeof(VB_CONF_S));
    stVbConf.u32MaxPoolCnt = 128;
    u32BlkSize = SAMPLE_COMM_SYS_CalcPicVbBlkSize(gs_enNorm,
                 enSize, SAMPLE_PIXEL_FORMAT, SAMPLE_SYS_ALIGN_WIDTH);
    stVbConf.astCommPool[0].u32BlkSize = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt = 10;

    //初始化系统
    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    if (HI_SUCCESS != s32Ret)
    {
        printf("system init failed with err code %#x!\n", s32Ret );
    }
    stViConfig.enViMode = SENSOR_TYPE;
    stViConfig.enRotate = ROTATE_NONE;
    stViConfig.enNorm = VIDEO_ENCODING_MODE_AUTO;
    stViConfig.enViChnSet = VI_CHN_SET_NORMAL;

    //配置并启动VI
    s32Ret = SAMPLE_COMM_VI_StartVi(&stViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        printf("start vi failed with err code %#x!\n", s32Ret);
        goto END_1;
    }
    s32Ret = SAMPLE_COMM_SYS_GetPicSize(gs_enNorm, enSize, &stSize);
    if (HI_SUCCESS != s32Ret)
    {
        printf("SAMPLE_COMM_SYS_GetPicSize failed with err code %#x!\n", s32Ret);
        goto END_2;
    }

    //配置并启动VPSS组
    VpssGrp = 0;
    stVpssGrpAttr.u32MaxW = stSize.u32Width;
    stVpssGrpAttr.u32MaxH = stSize.u32Height;
    stVpssGrpAttr.bDrEn = HI_FALSE;
    stVpssGrpAttr.bDbEn = HI_FALSE;
    stVpssGrpAttr.bIeEn = HI_TRUE;
    stVpssGrpAttr.bNrEn = HI_TRUE;
    stVpssGrpAttr.bHistEn = HI_TRUE;
    stVpssGrpAttr.enDieMode = VPSS_DIE_MODE_AUTO;
    stVpssGrpAttr.enPixFmt = SAMPLE_PIXEL_FORMAT;
    s32Ret = SAMPLE_COMM_VPSS_StartGroup(VpssGrp, &stVpssGrpAttr);
    if (HI_SUCCESS != s32Ret)
    {
        printf("SAMPLE_COMM_VPSS_StartGroup failed with err code %#x!\n", s32Ret);
        goto END_3;
    }

    //绑定VI和VPSS
    s32Ret = SAMPLE_COMM_VI_BindVpss(stViConfig.enViMode);
    if (HI_SUCCESS != s32Ret)
    {
        printf("SAMPLE_COMM_vi_BindVpss failed with err code %#x\n", s32Ret);
        goto END_4;
    }

    //配置并启动VPSS通道
    VpssChn = 0;
    memset(&stVpssChnAttr, 0, sizeof(stVpssChnAttr));
    stVpssChnAttr.bFrameEn = HI_FALSE;
    stVpssChnAttr.bSpEn = HI_TRUE;

    stVpssChnMode.enChnMode     = VPSS_CHN_MODE_USER;
    stVpssChnMode.bDouble       = HI_FALSE;
    stVpssChnMode.enPixelFormat = SAMPLE_PIXEL_FORMAT;
    stVpssChnMode.u32Width      = 1920;
    stVpssChnMode.u32Height     = 1080;
    s32Ret = SAMPLE_COMM_VPSS_EnableChn(VpssGrp, VpssChn, &stVpssChnAttr, &stVpssChnMode, HI_NULL);
    if (HI_SUCCESS != s32Ret)
    {
        printf("SAMPLE_COMM_VPSS_EnableChn failed with err code %#x\n", s32Ret);
        goto END_5;
    }

    //打开并绑定SnapVideo
    SnapVencGrp = 1;
    SnapVencChn = 1;
    s32Ret = SAMPLE_COMM_VENC_BindVpss(SnapVencGrp, VpssGrp, VpssChn);
    if (HI_SUCCESS != s32Ret)
    {
        printf("SAMPLE_COMM_VENC_BindVpss failed with err code %#x\n", s32Ret);
    }
    s32Ret = SAMPLE_COMM_SYS_GetPicSize(gs_enNorm, enSize, &stSize);
    if (HI_SUCCESS != s32Ret)
    {
        printf("SAMPLE_COMM_SYS_GetPicSize failed with err code %#x\n", s32Ret);
    }

    s32Ret = SAMPLE_COMM_VENC_SnapStart(SnapVencGrp, SnapVencChn, &stSize);
    if (HI_SUCCESS != s32Ret)
    {
        printf("SAMPLE_COMM_VENC_SnapStart failed with err code %#x\n", s32Ret);
    }



    //创建接收命令socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        perror("socket create failed");
        exit(0);
    }
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(8000);
    if (bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1)
    {
        perror("bind failed");
        exit(0);
    }
    if (listen(sockfd, 10) == -1)
    {
        perror("listen failed");
        exit(0);
    }


    //创建发送视频流socket
    SendSockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (SendSockfd == -1)
    {
        perror("send socket error");
        exit(0);
    }
    memset(&SendServaddr, 0, sizeof(SendServaddr));
    SendServaddr.sin_family = AF_INET;
    SendServaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    SendServaddr.sin_port = htons(6666);
    if (bind(SendSockfd, (struct sockaddr *)&SendServaddr, sizeof(SendServaddr)) == -1)
    {
        perror("send bind error");
        exit(0);
    }
    if (listen(SendSockfd, 10) == -1)
    {
        perror("send listen error");
        exit(0);
    }

#if 1
    snappid = pthread_create(&snaptid, 0, (HI_VOID*)SnapPic, HI_NULL);
    if (snappid != 0)
    {
        perror("create snapthread error");
        exit(0);
    }
    printf("create SnapPic thread successfully!\n");
#endif

#if 0
    acceptpid = pthread_create(&accepttid, 0, (HI_VOID*)accept_thread, sockfd);
    if (acceptpid != 0)
    {
        perror("create snapthread error");
        exit(0);
    }
    printf("create accept thread successfully!\n");
#endif

#if 0
    ret = pthread_create(&sendvideotid, 0, (HI_VOID*)SendVideoStream, SendSockfd);
    if (ret != 0)
    {
        perror("create SendVideoStream error");
        exit(0);
    }
    printf("create SendVideoStream thread successfully!\n");
#endif

#if 0
    ret = pthread_create(&filetid, 0, (HI_VOID*)FindLastFile, NULL);
    if (ret != 0)
    {
        perror("create FindLastFile error");
        exit(0);
    }
    printf("create FindLastFile thread successfully!\n");
#endif

#if 0
    ret = pthread_create(&recordtid, 0, (HI_VOID*)record, NULL);
    if (ret != 0)
    {
        perror("create record error");
        exit(0);
    }
    printf("create record thread successfully!\n");
#endif

    while(1)
    {
        sleep(10);
    }
END_5:
    VpssGrp = 0;
    VpssChn = 0;
    SAMPLE_COMM_VPSS_DisableChn(VpssGrp, VpssChn);
END_4:
    SAMPLE_COMM_VI_UnBindVpss(stViConfig.enViMode);
END_3:
    SAMPLE_COMM_VPSS_StopGroup(VpssGrp);
END_2:
    SAMPLE_COMM_VI_StopVi(&stViConfig);
END_1:
    SAMPLE_COMM_SYS_Exit();

    close(sockfd);
    close(SendSockfd);

    pthread_join(snaptid, NULL);
    pthread_join(accepttid, NULL);
    pthread_join(sendvideotid, NULL);
    pthread_join(filetid, NULL);
    pthread_join(recordtid, NULL);
    process(1);
    return 0;
}
