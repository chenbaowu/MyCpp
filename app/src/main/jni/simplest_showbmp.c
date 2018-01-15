
#ifdef __ANDROID__
#define __STDC_CONSTANT_MACROS

#ifdef _WIN32
#define snprintf _snprintf
//Windows
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavfilter/avfiltergraph.h"
#include "libavfilter/avcodec.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavutil/avutil.h"
#include "libswscale/swscale.h"
};
#else
//Linux...
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfiltergraph.h>
#include <libavfilter/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/avutil.h>
#include <libavutil/mathematics.h>
#include <libswscale/swscale.h>
#ifdef __cplusplus
};
#endif
#endif

#include <jni.h>
#include <android/log.h>
#include <pthread.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO , "(^_^)", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR , "(=_=)", __VA_ARGS__)
#else
#define LOGE(format, ...)  LOGE("(>_<) " format "\n", ##__VA_ARGS__)
#define LOGI(format, ...)  LOGE("(^_^) " format "\n", ##__VA_ARGS__)
#endif

#include "SDL.h"
#include "SDL_log.h"
#include "SDL_main.h"

//The attributes of the screen
const int SCREEN_WIDTH = 640;
const int SCREEN_HEIGHT = 480;
const int SCREEN_BPP = 32;
SDL_Window *screen = NULL;
SDL_Renderer* sdlRenderer = NULL;
SDL_Texture* sdlTexture = NULL;
SDL_Rect sdlRect;

//Enable SDL?
#define ENABLE_SDL 0
//Output YUV data?
#define ENABLE_YUVFILE 0

/*
FIX: H.264 in some container format (FLV, MP4, MKV etc.) need
"h264_mp4toannexb" bitstream filter (BSF)
  *Add SPS,PPS in front of IDR frame
  *Add start code ("0,0,0,1") in front of NALU
H.264 in some container (MPEG2TS) don't need this BSF.
*/
//'1': Use H.264 Bitstream Filter
#define USE_H264BSF 0

/*
FIX:AAC in some container format (FLV, MP4, MKV etc.) need
"aac_adtstoasc" bitstream filter (BSF)
*/
//'1': Use AAC Bitstream Filter  AAC混合成MP4需要这个
#define USE_AACBSF 1

int video_stream_index1 = -1;
int video_stream_index2 = -1;
static int video_stream_index_pre = -1;
static int audio_stream_index_pre = -1;



//////////////////////////////存放视频帧的容器/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*typedef struct
{
	AVFormatContext* pFormatCtx;
	AVOutputFormat* fmt;
	AVStream* video_st;
	AVCodecContext* pCodecCtx;
	AVCodec* pCodec;
	AVPacket pkt;
	uint8_t* picture_buf;
	AVFrame* pFrame;
	int picture_size;
	int y_size;
	int framecnt;
}encodeh264bean, *Encodeh264BeanPtr;*/
typedef enum { false, true } bool;

int playOneFrame(AVFrame* frame)
{
  /*                       #if ENABLE_SDL
                            #if 0
                                            SDL_UpdateTexture( sdlTexture, NULL, container->pFrameList[i]->data[0], container->pFrameList[i]->linesize[0] );
                            #else*/
                                            SDL_UpdateYUVTexture(sdlTexture, &sdlRect,
                                            frame->data[0], frame->linesize[0],
                                            frame->data[1], frame->linesize[1],
                                            frame->data[2], frame->linesize[2]);
     //                       #endif

                                            SDL_RenderClear( sdlRenderer );
                                            SDL_RenderCopy( sdlRenderer, sdlTexture,  NULL, &sdlRect);
                                            SDL_RenderPresent( sdlRenderer );
                                            //SDL End-----------------------
                                            //Delay 30ms
                                            SDL_Delay(30);
      //                      #endif
}

int initSDL2()             //初始化SDL2
{
#if ENABLE_SDL
	    //SDL---------------------------
        int screen_w=0,screen_h=0;

         if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
                LOGE( "Could not initialize SDL - %s\n", SDL_GetError());
                return -1;
            }
        screen_w = pCodecCtx->width;
            screen_h = pCodecCtx->height;
            //SDL 2.0 Support for multiple windows
            screen = SDL_CreateWindow("Simplest ffmpeg player's Window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                screen_w, screen_h,
                SDL_WINDOW_OPENGL);

            if(!screen) {
                LOGE("SDL: could not create window - exiting:%s\n",SDL_GetError());
                return -1;
            }

            sdlRenderer = SDL_CreateRenderer(screen, -1, 0);
            //IYUV: Y + U + V  (3 planes)
            //YV12: Y + V + U  (3 planes)
            sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,screen_w,screen_h);

            sdlRect.x=0;
            sdlRect.y=0;
            sdlRect.w=screen_w;
            sdlRect.h=screen_h;
#endif
}

JNIEXPORT jint JNICALL Java_cn_poco_video_NativeUtils_checkVideoResolution(JNIEnv *pEnv, jclass object, jstring video1, jstring video2)
{
    AVFormatContext *pFormatCtx1 = NULL, *pFormatCtx2 = NULL;
    const char *filename1 = (*pEnv)->GetStringUTFChars(pEnv, video1, 0);
    const char *filename2 = (*pEnv)->GetStringUTFChars(pEnv, video2, 0);
    int w1=0,w2=0,h1=0,h2=0;
    	int ret = 0;
     	avcodec_register_all();
       av_register_all();
       avfilter_register_all();

    	if ((ret = avformat_open_input(&pFormatCtx1, filename1, NULL, NULL)) < 0) {
    	LOGE("%s", filename1);
    		LOGE("Cannot open input file1 error  code %d\n", ret);
    		return ret;
    	}
        if ((ret = avformat_find_stream_info(pFormatCtx1, NULL)) < 0) {
            		LOGE("Cannot find stream information1\n");
            		return ret;
        }

        if ((ret = avformat_open_input(&pFormatCtx2, filename2, NULL, NULL)) < 0) {
            		LOGE("Cannot open input file2\n");
            		return ret;
            	}

    	if ((ret = avformat_find_stream_info(pFormatCtx2, NULL)) < 0) {
    		printf("Cannot find stream information2\n");
    		return ret;
    	}

        AVCodec *dec;
    	/* select the video stream */
    	ret = av_find_best_stream(pFormatCtx1, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0);
    	if (ret < 0) {
    		LOGE("Cannot find a video stream in the input file1\n");
    		return ret;
    	}
    	w1 = pFormatCtx1->streams[ret]->codec->width;
        h1 = pFormatCtx1->streams[ret]->codec->height;

        ret = av_find_best_stream(pFormatCtx2, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0);
        if (ret < 0) {
            LOGE("Cannot find a video stream in the input file2\n");
            return ret;
	}
	w2 = pFormatCtx2->streams[ret]->codec->width;
    h2 = pFormatCtx2->streams[ret]->codec->height;

    int result = 0;
    if(w1 == w2 && h1 == h2)
        result =  1;
     else
        result =  -1;

     if (pFormatCtx1)
      {
            avformat_close_input(&pFormatCtx1);
            pFormatCtx1 = NULL;
      }
           if (pFormatCtx2)
            {
                  avformat_close_input(&pFormatCtx2);
                  pFormatCtx2 = NULL;
            }

     return result;
}
jobject createBitmap(JNIEnv *pEnv, int pWidth, int pHeight) {
	int i;
	//get Bitmap class and createBitmap method ID
	jclass javaBitmapClass = (jclass)(*pEnv)->FindClass(pEnv, "android/graphics/Bitmap");
	jmethodID mid = (*pEnv)->GetStaticMethodID(pEnv, javaBitmapClass, "createBitmap", "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
	//create Bitmap.Config
	//reference: https://forums.oracle.com/thread/1548728
	const wchar_t* configName = L"ARGB_8888";
	int len = wcslen(configName);
	jstring jConfigName;
	if (sizeof(wchar_t) != sizeof(jchar)) {
		//wchar_t is defined as different length than jchar(2 bytes)
		jchar* str = (jchar*)malloc((len+1)*sizeof(jchar));
		for (i = 0; i < len; ++i) {
			str[i] = (jchar)configName[i];
		}
		str[len] = 0;
		jConfigName = (*pEnv)->NewString(pEnv, (const jchar*)str, len);
	} else {
		//wchar_t is defined same length as jchar(2 bytes)
		jConfigName = (*pEnv)->NewString(pEnv, (const jchar*)configName, len);
	}
	jclass bitmapConfigClass = (*pEnv)->FindClass(pEnv, "android/graphics/Bitmap$Config");
	jobject javaBitmapConfig = (*pEnv)->CallStaticObjectMethod(pEnv, bitmapConfigClass,
			(*pEnv)->GetStaticMethodID(pEnv, bitmapConfigClass, "valueOf", "(Ljava/lang/String;)Landroid/graphics/Bitmap$Config;"), jConfigName);
	//create the bitmap
	return (*pEnv)->CallStaticObjectMethod(pEnv, javaBitmapClass, mid, pWidth, pHeight, javaBitmapConfig);
}

typedef struct
{
    AVFormatContext **pFormatCtx;
    AVCodecContext **pCodecCtx;
    int videocount;
    int *video_stream_index_group;
    int *skipped_frame_group;
}VideoGroup ,*VideoGroupPtr;
VideoGroupPtr pMyVideoGroup = NULL;

static int open_input_file(const char *filename, AVFormatContext **pFormatCtx, AVCodecContext **pCodecCtx, int index)
{
	int ret;
	AVCodec *dec;

	if ((ret = avformat_open_input(pFormatCtx, filename, NULL, NULL)) < 0) {
		printf("Cannot open input file\n");
		return ret;
	}

	if ((ret = avformat_find_stream_info(*pFormatCtx, NULL)) < 0) {
		printf("Cannot find stream information\n");
		return ret;
	}

	/* select the video stream */
	ret = av_find_best_stream(*pFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0);
	if (ret < 0) {
		printf("Cannot find a video stream in the input file\n");
		return ret;
	}
	pMyVideoGroup->video_stream_index_group[index] = ret;
	*pCodecCtx = (*pFormatCtx)->streams[pMyVideoGroup->video_stream_index_group[index]]->codec;

   // (*(pCodecCtx))->pix_fmt = PIX_FMT_RGBA;
	/* init the video decoder */
	if ((ret = avcodec_open2(*pCodecCtx, dec, NULL)) < 0) {
		printf("Cannot open video decoder\n");
		return ret;
	}
	av_dump_format(*pFormatCtx, 0, filename, 0);
	return 0;
}

void initVideoGroup(int count)
{
    pMyVideoGroup = (VideoGroupPtr)malloc(sizeof(VideoGroup));
    pMyVideoGroup->videocount = count;
    pMyVideoGroup->pFormatCtx = (AVFormatContext**)malloc(sizeof(AVFormatContext*) * count);
    pMyVideoGroup->pCodecCtx = (AVCodecContext**)malloc(sizeof(AVCodecContext*) * count);
    pMyVideoGroup->video_stream_index_group = (int*)malloc(sizeof(int)*count);
    pMyVideoGroup->skipped_frame_group = (int*)malloc(sizeof(int)*count);
    int i=0;
    for(i=0; i<count; i++)
    {
        pMyVideoGroup->pFormatCtx[i] = NULL;
        pMyVideoGroup->pCodecCtx[i] = NULL;
        pMyVideoGroup->video_stream_index_group[i] = -1;
        pMyVideoGroup->skipped_frame_group[i] = 0;
    }
}

JNIEXPORT jint JNICALL Java_cn_poco_video_NativeUtils_endDecodeFrameBySeekTime(JNIEnv *pEnv, jclass object)
{
    if(pMyVideoGroup != NULL)
    {
        int i;
        for(i=0; i<pMyVideoGroup->videocount; i++)
        {
            if(pMyVideoGroup->pCodecCtx[i])
            {
                avcodec_close(pMyVideoGroup->pCodecCtx[i]);
                pMyVideoGroup->pCodecCtx[i] = NULL;
                avformat_close_input(&(pMyVideoGroup->pFormatCtx[i]));
                pMyVideoGroup->pFormatCtx[i] = NULL;
            }
        }
        free(pMyVideoGroup->skipped_frame_group);
        free(pMyVideoGroup->video_stream_index_group);
        pMyVideoGroup = NULL;
         return 1;
    }
   return 0;
}

JNIEXPORT jobject JNICALL Java_cn_poco_video_NativeUtils_decodeFrameBySeekTime(JNIEnv *pEnv, jclass object, jstring video_in, jint seektime, jint videoindex)
{
    clock_t start, finish;
    double  duration;
       /* 测量一个事件持续的时间*/
    start = clock();
    AVFormatContext *pFormatCtx = NULL;
    AVCodecContext *pCodecCtx = NULL;
    if(pMyVideoGroup == NULL)
    {
        LOGE("初始化视频组");
        initVideoGroup(6);
    }
    pFormatCtx = pMyVideoGroup->pFormatCtx[videoindex];
    pCodecCtx = pMyVideoGroup->pCodecCtx[videoindex];

    int ret;
    AVPacket packet;
    AVFrame frame;
    int got_frame;
    bool foundFrame = false;
    jobject bitmap = NULL;
    av_register_all();
     char *videofilename = (*pEnv)->GetStringUTFChars(pEnv, video_in, 0);

    if (pCodecCtx == NULL)  //判断解码器是否已经初始化
    {
        LOGE("打开文件");
        if ((ret = open_input_file(videofilename, &pFormatCtx, &pCodecCtx, videoindex)) < 0)
            goto end;
        pMyVideoGroup->pCodecCtx[videoindex] = pCodecCtx;
        pMyVideoGroup->pFormatCtx[videoindex] = pFormatCtx;
    }

    AVFrame	*pFrameYUV;
    pFrameYUV=avcodec_alloc_frame();
    struct SwsContext *img_convert_ctx;
    img_convert_ctx = sws_getContext
                          (
                              pCodecCtx->width,
                              pCodecCtx->height,
                              pCodecCtx->pix_fmt,
                              pCodecCtx->width,
                              pCodecCtx->height,
                              AV_PIX_FMT_RGBA,
                              SWS_BILINEAR,
                              NULL,
                              NULL,
                              NULL
                          );

    /* read all packets */
    //uint8_t *out_buffer = (uint8_t *)av_malloc(avpicture_get_size(AV_PIX_FMT_RGBA, pCodecCtx->width, pCodecCtx->height));  这里无需自己申请内存，AndroidBitmap_lockPixels会申请，  自己再申请了就溢出了
   uint8_t *out_buffer;
    bitmap = createBitmap(pEnv, pCodecCtx->width, pCodecCtx->height);

    ret = AndroidBitmap_lockPixels(pEnv, bitmap, &out_buffer);
    if (ret < 0)
    {
        LOGE("LockPixels error, errorcode:%d", ret);
         goto end;
    }
     avpicture_fill((AVPicture *)pFrameYUV, out_buffer, AV_PIX_FMT_RGBA, pCodecCtx->width, pCodecCtx->height);

   // int SeekTime = av_rescale(frameindex, pFormatCtx->streams[video_stream_index]->time_base.den,  pFormatCtx->streams[video_stream_index]->time_base.num);
   int SeekTime = seektime *　AV_TIME_BASE;
    //跳帧到指定时间附近读帧
    av_seek_frame(pFormatCtx,
                      -1,
                      SeekTime,
                      AVSEEK_FLAG_BACKWARD) ;//AVSEEK_FLAG_ANY AVSEEK_FLAG_BACKWARD

    avcodec_flush_buffers(pFormatCtx->streams[pMyVideoGroup->video_stream_index_group[videoindex]]->codec);
     int skipped_frame = 0;
    while (1) {
		ret = av_read_frame(pFormatCtx, &packet);
		int duration = packet.pts * av_q2d(pFormatCtx->streams[pMyVideoGroup->video_stream_index_group[videoindex]]->time_base);

        if (ret< 0)  //PACKET读取完毕后   还要读取残留在PACKET的帧
        {
            break;
        }
        if (packet.stream_index == pMyVideoGroup->video_stream_index_group[videoindex]) {

            avcodec_get_frame_defaults(&frame);
            got_frame = 0;
            ret = avcodec_decode_video2(pCodecCtx, &frame, &got_frame, &packet);
            if (ret < 0) {
                LOGE( "Error decoding video\n");
                break;
            }
            if (got_frame) {

                frame.pts = av_frame_get_best_effort_timestamp(&frame);
                sws_scale(img_convert_ctx, (const uint8_t* const*)frame.data, frame.linesize, 0, pCodecCtx->height, pFrameYUV->data, pFrameYUV->linesize);  //转为为标准RGBA格式

                LOGE("GOT FRAME!");
                foundFrame = true;
                    break;
            }
            else
                skipped_frame++;
        }
         av_free_packet(&packet);
    }

    if(foundFrame == false)  //如果之前的帧没有定位到 ，  尝试解析剩余的帧
    {
    LOGE("解析剩下帧");
        int i=0;
        for(i=skipped_frame; i>0; i--)  //解码packet中剩下的之前解码失败的帧
        {
            got_frame = 0;
            ret = avcodec_decode_video2(pCodecCtx, &frame, &got_frame, &packet);
            if (got_frame) {
               // packet.pts = av_rescale_q(packet.pts,video_dec_st->time_base,video_enc_st->time_base);
               frame.pts = av_frame_get_best_effort_timestamp(&frame);
                sws_scale(img_convert_ctx, (const uint8_t* const*)frame.data, frame.linesize, 0, pCodecCtx->height, pFrameYUV->data, pFrameYUV->linesize);  //转为为标准420P的YUV格式
                break;
            }
        }
    }
	AndroidBitmap_unlockPixels(pEnv, bitmap);
    av_free_packet(&packet);
 //   av_free(out_buffer);   这里由AndroidBitmap_unlockPixels来释放outbuffer的内存就行了
//    out_buffer = NULL;
    av_free(pFrameYUV);
    pFrameYUV = NULL;
    sws_freeContext(img_convert_ctx);
    img_convert_ctx = NULL;
end:

    if (ret < 0 && ret != AVERROR_EOF) {
        char buf[1024];
        av_strerror(ret, buf, sizeof(buf));
        LOGE("Error occurred: %s\n", buf);
        return -1;
    }
     finish = clock();
     duration = (double)(finish - start) / CLOCKS_PER_SEC;
     LOGE( "%f seconds\n", duration );
    return bitmap;
}

typedef struct
{
    int start;
    int end;
    AVFrame *src;
    AVFrame *dst;
    struct SwsContext *img_convert_ctx;
} ThreadArgument;
void *thread_function(void *arg)
{
 clock_t start, end;
    double  duration;
       /* 测量一个事件持续的时间*/
    start = clock();
    ThreadArgument *info = (ThreadArgument*)arg;
    sws_scale(info->img_convert_ctx, (const uint8_t* const*)info->src->data, info->src->linesize, info->start, info->end, info->dst->data, info->dst->linesize);  //转为为标准RGBA格式
   end = clock();
   duration = (double)(end - start) / CLOCKS_PER_SEC;
    LOGE( "convert to rgb cost %f seconds  \n", duration);
    return "";
//    pthread_exit("Thank you for your CPU time!");
}
JNIEXPORT jbyteArray JNICALL Java_cn_poco_video_NativeUtils_getNextFrameFromFile(JNIEnv *pEnv, jclass object, jstring video_in, jint videoindex, jobject width, jobject height)
{
    jclass IntClass = (*pEnv)->FindClass(pEnv, "java/lang/Integer");
    jclass FloatClass = (*pEnv)->FindClass(pEnv, "java/lang/Float");
    clock_t start, finish;
    double  duration;
       /* 测量一个事件持续的时间*/
    start = clock();

      clock_t start2, finish2;
        double  duration2;
           /* 测量一个事件持续的时间*/
      clock_t start3, finish3;
        double  duration3;
           /* 测量一个事件持续的时间*/
             clock_t start4, finish4;
                   double  duration4;
                      /* 测量一个事件持续的时间*/
    AVFormatContext *pFormatCtx = NULL;
    AVCodecContext *pCodecCtx = NULL;
    if(pMyVideoGroup == NULL)
    {
        LOGE("初始化视频组");
        initVideoGroup(6);
    }
    pFormatCtx = pMyVideoGroup->pFormatCtx[videoindex];
    pCodecCtx = pMyVideoGroup->pCodecCtx[videoindex];

    int ret;
    AVPacket packet;
    AVFrame *pframe;
    int got_frame;
    bool foundFrame = false;
    av_register_all();
    const char *videofilename = (*pEnv)->GetStringUTFChars(pEnv, video_in, 0);

    if (pCodecCtx == NULL)  //判断解码器是否已经初始化
    {
        LOGE("打开文件");
        if ((ret = open_input_file(videofilename, &pFormatCtx, &pCodecCtx, videoindex)) < 0)
            goto end;
        pMyVideoGroup->pCodecCtx[videoindex] = pCodecCtx;
        pMyVideoGroup->pFormatCtx[videoindex] = pFormatCtx;
    }

    AVFrame	*pFrameYUV;
    pFrameYUV=avcodec_alloc_frame();
    pframe = avcodec_alloc_frame();
    struct SwsContext *img_convert_ctx;
    img_convert_ctx = sws_getContext
                          (
                              pCodecCtx->width,
                              pCodecCtx->height,
                              pCodecCtx->pix_fmt,
                              pCodecCtx->width,
                              pCodecCtx->height,
                              AV_PIX_FMT_RGB24,
                              SWS_POINT,
                              NULL,
                              NULL,
                              NULL
                          );

    /* read all packets */
    uint8_t *out_buffer = (uint8_t *)av_malloc(avpicture_get_size(AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height));

     avpicture_fill((AVPicture *)pFrameYUV, out_buffer, AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);
   // int SeekTime = av_rescale(frameindex, pFormatCtx->streams[video_stream_index]->time_base.den,  pFormatCtx->streams[video_stream_index]->time_base.num);
 /*  int SeekTime = seektime *　AV_TIME_BASE;
    //跳帧到指定时间附近读帧
    av_seek_frame(pFormatCtx,
                      -1,
                      SeekTime,
                      AVSEEK_FLAG_BACKWARD) ;//AVSEEK_FLAG_ANY AVSEEK_FLAG_BACKWARD

    //avcodec_flush_buffers(pFormatCtx->streams[video_stream_index_pre]->codec);*/
     int skipped_frame = 0;
    while (1) {
         start2 = clock();
		ret = av_read_frame(pFormatCtx, &packet);
		finish2 = clock();
        duration2 = (double)(finish2 - start2) / CLOCKS_PER_SEC;
        LOGE( "read cost %f seconds  video=%d\n", duration2,videoindex );
        if (ret< 0)  //PACKET读取完毕后   还要读取残留在PACKET的帧
        {
                av_seek_frame(pFormatCtx,
                                  -1,
                                  0,
                                  AVSEEK_FLAG_BACKWARD) ;//AVSEEK_FLAG_ANY AVSEEK_FLAG_BACKWARD
            continue;
        }
        if (packet.stream_index == pMyVideoGroup->video_stream_index_group[videoindex]) {
        //    avcodec_get_frame_defaults(pframe);
            got_frame = 0;

             start3 = clock();
            ret = avcodec_decode_video2(pCodecCtx, pframe, &got_frame, &packet);

            if (ret < 0) {
                LOGE( "Error decoding video\n");
                break;
            }
            if (got_frame) {
               finish3 = clock();
               duration3 = (double)(finish3 - start3) / CLOCKS_PER_SEC;
               LOGE( "decode cost %f seconds  video=%d\n", duration3,videoindex );

               start4 = clock();
               ThreadArgument *arg = (ThreadArgument*)malloc(sizeof(ThreadArgument));
               arg->start = 0;
               arg->end = pCodecCtx->height;
               arg->src = pframe;
               arg->dst = pFrameYUV;
               arg->img_convert_ctx = img_convert_ctx;
//
//               ThreadArgument *arg2 = (ThreadArgument*)malloc(sizeof(ThreadArgument));
//               arg2->start = (pCodecCtx->height / 4 )+ 1;
//               arg2->end = (pCodecCtx->height / 4 * 2);
//               arg2->src = pframe;
//               arg2->dst = pFrameYUV;
//               arg2->img_convert_ctx = img_convert_ctx;
//
//               ThreadArgument *arg3 = (ThreadArgument*)malloc(sizeof(ThreadArgument));
//               arg3->start = (pCodecCtx->height / 4 * 2)+ 1;
//               arg3->end = pCodecCtx->height /4 * 3;
//               arg3->src = pframe;
//               arg3->dst = pFrameYUV;
//               arg3->img_convert_ctx = img_convert_ctx;
//
//               ThreadArgument *arg4 = (ThreadArgument*)malloc(sizeof(ThreadArgument));
//               arg4->start = (pCodecCtx->height / 4 * 3)+ 1;
//               arg4->end = pCodecCtx->height;
//               arg4->src = pframe;
//               arg4->dst = pFrameYUV;
//               arg4->img_convert_ctx = img_convert_ctx;

               pthread_t a_thread, a_thread2, a_thread3, a_thread4;
               void *thread1_return;

               int res = pthread_create(&a_thread, NULL, thread_function, (void *)arg);
//               int res2 = pthread_create(&a_thread2, NULL, thread_function, (void *)arg2);
//               int res3 = pthread_create(&a_thread3, NULL, thread_function, (void *)arg3);
//               int res4 = pthread_create(&a_thread4, NULL, thread_function, (void *)arg4);
//                start2 = clock();
//                sws_scale(img_convert_ctx, (const uint8_t* const*)pframe->data, pframe->linesize, 0, pCodecCtx->height, pFrameYUV->data, pFrameYUV->linesize);  //转为为标准RGBA格式
              int wait_thread_end = pthread_join( a_thread, NULL );
//              int wait_thread_end2 = pthread_join( a_thread2, NULL);
//              int wait_thread_end3 = pthread_join( a_thread3, NULL);
//              int wait_thread_end4 = pthread_join( a_thread4, NULL);
//                finish2 = clock();
//                duration2 = (double)(finish2 - start2) / CLOCKS_PER_SEC;
//                LOGE( "create thread cost %f seconds  video=%d\n", duration2, videoindex );
               finish4 = clock();
               duration4 = (double)(finish4 - start4) / CLOCKS_PER_SEC;
               //LOGE( "convert to rgb cost %f seconds  video=%d\n", duration4, videoindex );
                foundFrame = true;
                    break;

            }
            else
                skipped_frame++;
        }
         av_free_packet(&packet);
    }

    if(foundFrame == false)  //如果之前的帧没有定位到 ，  尝试解析剩余的帧
    {
    LOGE("解析剩下帧");
        int i=0;
        for(i=skipped_frame; i>0; i--)  //解码packet中剩下的之前解码失败的帧
        {
            got_frame = 0;
            ret = avcodec_decode_video2(pCodecCtx, pframe, &got_frame, &packet);
            if (got_frame) {
               // packet.pts = av_rescale_q(packet.pts,video_dec_st->time_base,video_enc_st->time_base);
               pframe->pts = av_frame_get_best_effort_timestamp(pframe);
                sws_scale(img_convert_ctx, (const uint8_t* const*)pframe->data, pframe->linesize, 0, pCodecCtx->height, pFrameYUV->data, pFrameYUV->linesize);  //转为为标准420P的YUV格式
                break;
            }
        }
    }

    jfieldID id = (*pEnv)->GetFieldID(pEnv, IntClass, "value", "I");
    jfieldID id2 = (*pEnv)->GetFieldID(pEnv, FloatClass, "value", "F");
    (*pEnv)->SetIntField(pEnv, width, id, pCodecCtx->width);
    (*pEnv)->SetIntField(pEnv, height, id, pCodecCtx->height);
     jbyteArray  jbarray =  (*pEnv)->NewByteArray(pEnv,avpicture_get_size(AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height));
     jbyte *jy = (jbyte*)out_buffer;
     LOGE("set video = %d", videoindex);
     (*pEnv)->SetByteArrayRegion(pEnv,jbarray, 0, avpicture_get_size(AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height), jy);
//    LOGE("set done video=%d", videoindex);
    av_free_packet(&packet);
    av_free(out_buffer);
    out_buffer = NULL;
    av_free(pFrameYUV);
    av_free(pframe);
    pframe = NULL;
    pFrameYUV = NULL;
    sws_freeContext(img_convert_ctx);
    img_convert_ctx = NULL;
end:

    if (ret < 0 && ret != AVERROR_EOF) {
        char buf[1024];
        av_strerror(ret, buf, sizeof(buf));
        LOGE("Error occurred: %s\n", buf);
        return -1;
    }
     finish = clock();
     duration = (double)(finish - start) / CLOCKS_PER_SEC;
     LOGE( "decode video consume %f seconds ,video = %d\n", duration, videoindex );
    return jbarray;
}


JNIEXPORT jint JNICALL Java_cn_poco_video_NativeUtils_getDurationFromFile(JNIEnv *pEnv, jclass object, jstring video_in)
{
     AVFormatContext *pFormatCtx = NULL;
    int ret;
    AVCodec dec;
     char *filename = (*pEnv)->GetStringUTFChars(pEnv, video_in, 0);

     av_register_all();
    if(pFormatCtx == NULL)
    {
        if ((ret = avformat_open_input(&pFormatCtx, filename, NULL, NULL)) < 0) {
            LOGE( "Cannot open input file\n  path=%s", filename);
            return ret;
        }
    }

    if ((ret = avformat_find_stream_info(pFormatCtx, NULL)) < 0) {
        LOGE( "Cannot find stream information\n");
        return ret;
    }

    /* select the video stream */
    ret = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0);
    if (ret < 0) {
        LOGE( "Cannot find a video stream in the input file, may be input file is not invild, now set video stream to 0");
        ret = 0;
 //       return ret;
    }
    video_stream_index_pre = ret;

    float time = -1;
    if(video_stream_index_pre != -1)
    {
        /* init the video decoder */
        time = (float)(pFormatCtx->duration)/ AV_TIME_BASE; //时长
    }

         if (pFormatCtx)
         {
              avformat_close_input(&pFormatCtx);
              pFormatCtx = NULL;
         }

    return time;
}

JNIEXPORT jfloat JNICALL Java_cn_poco_video_NativeUtils_getFPSFromFile(JNIEnv *pEnv, jclass object, jstring video_in)
{
     AVFormatContext *pFormatCtx = NULL;
    int ret;
    AVCodec dec;
     char *filename = (*pEnv)->GetStringUTFChars(pEnv, video_in, 0);

     av_register_all();
    if(pFormatCtx == NULL)
    {
        if ((ret = avformat_open_input(&pFormatCtx, filename, NULL, NULL)) < 0) {
            LOGE( "Cannot open input file\n  path=%s", filename);
            return ret;
        }
    }

    if ((ret = avformat_find_stream_info(pFormatCtx, NULL)) < 0) {
        LOGE( "Cannot find stream information\n");
        return ret;
    }

    /* select the video stream */
    ret = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0);
    if (ret < 0) {
        LOGE( "Cannot find a video stream in the input file, may be input file is not invild, now set video stream to 0");
        ret = 0;
 //       return ret;
    }
    video_stream_index_pre = ret;

    int framecount = 0;
    float avg = 0;
    if(video_stream_index_pre != -1)
    {
        /* init the video decoder */
        avg = (float)pFormatCtx->streams[video_stream_index_pre]->avg_frame_rate.num / pFormatCtx->streams[video_stream_index_pre]->avg_frame_rate.den;  //帧率
    }
         if (pFormatCtx)
         {
              avformat_close_input(&pFormatCtx);
               pFormatCtx = NULL;
         }

    return avg;
}

JNIEXPORT jint JNICALL Java_cn_poco_video_NativeUtils_getFrameNumFromFile(JNIEnv *pEnv, jclass object, jstring video_in)
{
     AVFormatContext *pFormatCtx = NULL;
    int ret;
    AVCodec dec;
     char *filename = (*pEnv)->GetStringUTFChars(pEnv, video_in, 0);

     av_register_all();
    if(pFormatCtx == NULL)
    {
        if ((ret = avformat_open_input(&pFormatCtx, filename, NULL, NULL)) < 0) {
            LOGE( "Cannot open input file\n  path=%s", filename);
            return ret;
        }
    }

    if ((ret = avformat_find_stream_info(pFormatCtx, NULL)) < 0) {
        LOGE( "Cannot find stream information\n");
        return ret;
    }

    /* select the video stream */
    ret = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0);
    if (ret < 0) {
        LOGE( "Cannot find a video stream in the input file, may be input file is not invild, now set video stream to 0");
        ret = 0;
 //       return ret;
    }
    video_stream_index_pre = ret;

    int framecount = 0;
    if(video_stream_index_pre != -1)
    {
        /* init the video decoder */
        float avg = (float)(pFormatCtx->streams[video_stream_index_pre]->avg_frame_rate.num) / pFormatCtx->streams[video_stream_index_pre]->avg_frame_rate.den;  //帧率
        float time = (float)(pFormatCtx->duration)/ AV_TIME_BASE; //时长
        framecount = avg*time;
    }
         if (pFormatCtx)
         {
              avformat_close_input(&pFormatCtx);
               pFormatCtx = NULL;
         }

    return framecount;
}

//int mixVideoSegment(const char* in_video, const char* out_video, int StartTime, int EndTime)
int audio_lastpts = 0, audio_lastdts=0, video_lastpts = 0, video_lastdts=0;
AVRational *pre_video_timebase = NULL;
AVRational *pre_audio_timebase = NULL;
AVFormatContext *ofmt_ctx = NULL;
AVOutputFormat *ofmt = NULL;
JNIEXPORT jint JNICALL Java_cn_poco_video_NativeUtils_mixVideoSegment(JNIEnv *pEnv, jclass object, jstring video_in, jstring video_out, jint startTime, jint targetTime)
{
    char *in_filename = (*pEnv)->GetStringUTFChars(pEnv, video_in, 0);
    char *out_filename = (*pEnv)->GetStringUTFChars(pEnv, video_out, 0);
	//Input AVFormatContext and Output AVFormatContext
		AVFormatContext *ifmt_ctx = NULL;
    	AVPacket pkt;
    	int ret, i;
    	int frame_index=0;

    	av_register_all();


    	//Input
    	if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
    		LOGE( "Could not open input file.");
    		goto end;
    	}
    	if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
    		LOGE( "Failed to retrieve input stream information");
    		goto end;
    	}
    	AVCodec *dec,*dec2;
    	ret = av_find_best_stream(ifmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot find a video stream in the input file\n");
            return ret;
        }
        int video_stream_index = ret;
    	ret = av_find_best_stream(ifmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &dec2, 0);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot find a audio stream in the input file\n");
     //       return ret;
        }
        int audio_stream_index = ret;
    	av_dump_format(ifmt_ctx, 0, in_filename, 0);
    	LOGE("\n");
    	if(ofmt_ctx == NULL) //第一次mix需要打开输出文件了
    	{
    		//Output
    		avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);
    		if (!ofmt_ctx) {
    			LOGE( "Could not create output context\n");
    			ret = AVERROR_UNKNOWN;
    			goto end;
    		}
    		ofmt = ofmt_ctx->oformat;
    		for (i = 0; i < ifmt_ctx->nb_streams; i++) {
    			//Create output AVStream according to input AVStream
    			AVStream *in_stream = ifmt_ctx->streams[i];
    			AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
    			if (!out_stream) {
    				LOGE( "Failed allocating output stream\n");
    				ret = AVERROR_UNKNOWN;
    				goto end;
    			}
    			//Copy the settings of AVCodecContext
    			if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
    				LOGE( "Failed to copy context from input to output stream codec context\n");
    				goto end;
    			}
    			out_stream->codec->codec_tag = 0;
    		//	if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
    				out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
    		}
    		//Output information------------------
    		av_dump_format(ofmt_ctx, 0, out_filename, 1);
    		//Open output file
    		if (!(ofmt->flags & AVFMT_NOFILE)) {
    			ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
    			if (ret < 0) {
    				LOGE( "Could not open output file '%s'", out_filename);
    				goto end;
    			}
    		}
    		//Write file header
    		if (avformat_write_header(ofmt_ctx, NULL) < 0) {
    			LOGE( "Error occurred when opening output file\n");
    			goto end;
    		}
    	}

    //	int startTime = StartTime;
   // 	int targetTime = EndTime;
        //跳帧到指定时间附近读帧
        av_seek_frame(ifmt_ctx,
                          -1,
    					  startTime * AV_TIME_BASE,
                          AVSEEK_FLAG_BACKWARD) ;//AVSEEK_FLAG_ANY AVSEEK_FLAG_BACKWARD

    	float duration = 0;
    	while (1) {
    		AVStream *in_stream, *out_stream;
    		//Get an AVPacket
    		ret = av_read_frame(ifmt_ctx, &pkt);
    		if (ret < 0)
    			break;
    		in_stream  = ifmt_ctx->streams[pkt.stream_index];
    		out_stream = ofmt_ctx->streams[pkt.stream_index];
    		if(pkt.stream_index == video_stream_index)
    		{
    			int index = pkt.pos;
    			if(pre_video_timebase == NULL)  //记录第一个片段视频的时间基  统一一下pts  再转化为output流的时间基下的pts
    			{
    				pre_video_timebase = (AVRational*)malloc(sizeof(AVRational));
    				pre_video_timebase->num = ifmt_ctx->streams[video_stream_index]->time_base.num;
    				pre_video_timebase->den = ifmt_ctx->streams[video_stream_index]->time_base.den;
    			}
    			duration = pkt.pts * av_q2d(ifmt_ctx->streams[video_stream_index]->time_base);

    			//if(ceil(duration) < startTime-1 || duration > targetTime)
    			if(duration > targetTime)
    			{
    				av_free_packet(&pkt);
    				continue;
    			}
    			//Convert PTS/DTS

    			pkt.pts = av_rescale_q_rnd(video_lastpts, *pre_video_timebase, out_stream->time_base, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));

    		//	float currentsecond  = pkt.pts * av_q2d(out_stream->time_base);
    			pkt.dts = av_rescale_q_rnd(video_lastdts, *pre_video_timebase, out_stream->time_base, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
    			pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, *pre_video_timebase);
    			video_lastdts += pkt.duration;
    			video_lastpts +=pkt.duration;
    			pkt.duration = av_rescale_q(pkt.duration, *pre_video_timebase, out_stream->time_base);
    			pkt.pos = -1;
    		}
    		else
    		{
    			int index = pkt.pos;
    			if(pre_audio_timebase == NULL)  //记录第一个写入片段的音频的时间基  统一一下pts  再转化为output流的时间基下的pts
    			{
    				pre_audio_timebase = (AVRational*)malloc(sizeof(AVRational));
    				pre_audio_timebase->num = ifmt_ctx->streams[audio_stream_index]->time_base.num;
    				pre_audio_timebase->den = ifmt_ctx->streams[audio_stream_index]->time_base.den;
    			}
    			duration = pkt.pts * av_q2d(ifmt_ctx->streams[audio_stream_index]->time_base);


    			if(duration > targetTime)
    			{
    				av_free_packet(&pkt);
    				continue;
    			}

    			//Convert PTS/DTS
    			pkt.pts = av_rescale_q_rnd(audio_lastpts, *pre_audio_timebase, out_stream->time_base, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
    			float currentsecond = pkt.pts * av_q2d(out_stream->time_base);

    			pkt.dts = av_rescale_q_rnd(audio_lastdts, *pre_audio_timebase, out_stream->time_base, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
    			pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, *pre_audio_timebase);
    			audio_lastdts += pkt.duration;
    			audio_lastpts +=pkt.duration;
    			pkt.duration = av_rescale_q(pkt.duration, *pre_audio_timebase, out_stream->time_base);
    			pkt.pos = -1;
    		}
    			//Write
    		if (av_interleaved_write_frame(ofmt_ctx, &pkt) < 0) {
    			LOGE( "Error muxing packet\n");
    			break;
    		}
    	//	LOGE("Write %8d frames to output file\n",frame_index);

    		av_free_packet(&pkt);
    		frame_index++;
    	}

    end:
    	avformat_close_input(&ifmt_ctx);
    	/* close output */

    	return 0;
}

JNIEXPORT jint JNICALL Java_cn_poco_video_NativeUtils_endMixing(JNIEnv *pEnv, jclass object)
{
	audio_lastpts = 0;audio_lastdts=0;video_lastpts = 0;video_lastdts=0;
	if(pre_video_timebase != NULL)
	{
		free(pre_video_timebase);
		pre_video_timebase = NULL;
	}
	if(pre_audio_timebase != NULL)
	{
		free(pre_audio_timebase);
		pre_audio_timebase = NULL;
	}
	if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
	{
		av_write_trailer(ofmt_ctx);
		avio_close(ofmt_ctx->pb);
		avformat_free_context(ofmt_ctx);
		ofmt = NULL;
		ofmt_ctx = NULL;

	}
	return 0;
}

JNIEXPORT jint JNICALL Java_cn_poco_video_NativeUtils_changeVideoSpeed(JNIEnv *pEnv, jclass object, jstring video_in, jstring video_out, jfloat radio)
{
	AVOutputFormat *ofmt = NULL;
	//Input AVFormatContext and Output AVFormatContext
	AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
	AVPacket pkt;
	const char *in_filename, *out_filename;
	int ret, i;
	int frame_index=0;
	float speedratio = radio;
	in_filename  =(*pEnv)->GetStringUTFChars(pEnv, video_in, 0);//Input file URL
	out_filename = (*pEnv)->GetStringUTFChars(pEnv, video_out, 0);//Output file URL

	av_register_all();
	//Input
	if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
		LOGE( "Could not open input file.");
		goto end;
	}
	if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
		LOGE( "Failed to retrieve input stream information");
		goto end;
	}
	av_dump_format(ifmt_ctx, 0, in_filename, 0);
	//Output
	avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);
	if (!ofmt_ctx) {
		LOGE( "Could not create output context\n");
		ret = AVERROR_UNKNOWN;
		goto end;
	}
	ofmt = ofmt_ctx->oformat;
	for (i = 0; i < ifmt_ctx->nb_streams; i++) {
		//Create output AVStream according to input AVStream
		AVStream *in_stream = ifmt_ctx->streams[i];
		AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
		if (!out_stream) {
			LOGE( "Failed allocating output stream\n");
			ret = AVERROR_UNKNOWN;
			goto end;
		}
		//Copy the settings of AVCodecContext
		if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
			LOGE( "Failed to copy context from input to output stream codec context\n");
			goto end;
		}
		out_stream->codec->codec_tag = 0;
		if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
			out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}
	//Output information------------------
	av_dump_format(ofmt_ctx, 0, out_filename, 1);
	//Open output file
	if (!(ofmt->flags & AVFMT_NOFILE)) {
		ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
		if (ret < 0) {
			LOGE( "Could not open output file '%s'", out_filename);
			goto end;
		}
	}
	//Write file header
	if (avformat_write_header(ofmt_ctx, NULL) < 0) {
		LOGE( "Error occurred when opening output file\n");
		goto end;
	}

	AVRational newtimebase;
	while (1) {
		AVStream *in_stream, *out_stream;
		//Get an AVPacket
		ret = av_read_frame(ifmt_ctx, &pkt);
		if (ret < 0)
			break;
		in_stream  = ifmt_ctx->streams[pkt.stream_index];
		out_stream = ofmt_ctx->streams[pkt.stream_index];

		newtimebase.den = out_stream->time_base.den;
		newtimebase.num = out_stream->time_base.num;

		newtimebase.den *= speedratio;
		//Convert PTS/DTS
		pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, newtimebase, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, newtimebase, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, newtimebase);
		pkt.pos = -1;

		//Write
		if (av_interleaved_write_frame(ofmt_ctx, &pkt) < 0) {
			LOGE( "Error muxing packet\n");
			break;
		}

		//LOGE("Write %8d frames to output file\n",frame_index);
		av_free_packet(&pkt);
		frame_index++;
	}
	//Write file trailer
	av_write_trailer(ofmt_ctx);
end:
	avformat_close_input(&ifmt_ctx);
	/* close output */
	if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
		avio_close(ofmt_ctx->pb);
	avformat_free_context(ofmt_ctx);
    return 1;
}

JNIEXPORT jint JNICALL Java_cn_poco_video_NativeUtils_getAACFromVideo(JNIEnv *pEnv, jclass object, jstring video_in, jstring aac_out)
{
   char *in_filename = (*pEnv)->GetStringUTFChars(pEnv, video_in, 0);
    char *out_filename_a = (*pEnv)->GetStringUTFChars(pEnv, aac_out, 0);
	AVOutputFormat *ofmt_a = NULL;
	//（Input AVFormatContext and Output AVFormatContext）
	AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx_a = NULL;
	AVPacket pkt;
	int ret, i;
	int audioindex=-1;
	int frame_index=0;

//	const char *in_filename  = "cuc_ieschool.flv";//Input file URL
	//char *in_filename  = "cuc_ieschool.mkv";
	//char *out_filename_a = "cuc_ieschool.mp3";
//	const char *out_filename_a = "cuc_ieschool.aac";

	av_register_all();
	//Input
	if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
		LOGE( "Could not open input file.");
		goto end;
	}
	if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
		LOGE( "Failed to retrieve input stream information");
		goto end;
	}


	avformat_alloc_output_context2(&ofmt_ctx_a, NULL, NULL, out_filename_a);
	if (!ofmt_ctx_a) {
		LOGE( "Could not create output context\n");
		ret = AVERROR_UNKNOWN;
		goto end;
	}
	ofmt_a = ofmt_ctx_a->oformat;

	for (i = 0; i < ifmt_ctx->nb_streams; i++) {
			//Create output AVStream according to input AVStream
			AVFormatContext *ofmt_ctx;
			AVStream *in_stream = ifmt_ctx->streams[i];
			AVStream *out_stream = NULL;


			if(ifmt_ctx->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO){
				audioindex=i;
				out_stream=avformat_new_stream(ofmt_ctx_a, in_stream->codec->codec);
				ofmt_ctx=ofmt_ctx_a;
			}
			else
				continue;

			if (!out_stream) {
				LOGE( "Failed allocating output stream\n");
				ret = AVERROR_UNKNOWN;
				goto end;
			}
			//Copy the settings of AVCodecContext
			if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
				LOGE( "Failed to copy context from input to output stream codec context\n");
				goto end;
			}
			out_stream->codec->codec_tag = 0;

			if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
				out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}

	//Dump Format------------------
	LOGE("\n==============Input Video=============\n");
	av_dump_format(ifmt_ctx, 0, in_filename, 0);
	LOGE("\n==============Output Audio============\n");
	av_dump_format(ofmt_ctx_a, 0, out_filename_a, 1);
	LOGE("\n======================================\n");

	//Open output file
	if (!(ofmt_a->flags & AVFMT_NOFILE)) {
		if (avio_open(&ofmt_ctx_a->pb, out_filename_a, AVIO_FLAG_WRITE) < 0) {
			LOGE( "Could not open output file '%s'", out_filename_a);
			goto end;
		}
	}

	//Write file header
	if (avformat_write_header(ofmt_ctx_a, NULL) < 0) {
		LOGE( "Error occurred when opening audio output file\n");
		goto end;
	}


	while (1) {
		AVFormatContext *ofmt_ctx;
		AVStream *in_stream, *out_stream;
		//Get an AVPacket
		if (av_read_frame(ifmt_ctx, &pkt) < 0)
			break;
		in_stream  = ifmt_ctx->streams[pkt.stream_index];


		if(pkt.stream_index==audioindex){
			out_stream = ofmt_ctx_a->streams[0];
			ofmt_ctx=ofmt_ctx_a;
		//	LOGE("Write Audio Packet. size:%d\tpts:%lld\n",pkt.size,pkt.pts);
		}else{
			continue;
		}


		//Convert PTS/DTS
		pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
		pkt.pos = -1;
		pkt.stream_index=0;
		//Write
		if (av_interleaved_write_frame(ofmt_ctx, &pkt) < 0) {
			LOGE( "Error muxing packet\n");
			break;
		}
		//LOGE("Write %8d frames to output file\n",frame_index);
		av_free_packet(&pkt);
		frame_index++;
	}

	//Write file trailer
	av_write_trailer(ofmt_ctx_a);
end:
	avformat_close_input(&ifmt_ctx);
	/* close output */
	if (ofmt_ctx_a && !(ofmt_a->flags & AVFMT_NOFILE))
		avio_close(ofmt_ctx_a->pb);

	avformat_free_context(ofmt_ctx_a);


	if (ret < 0 && ret != AVERROR_EOF) {
		LOGE( "Error occurred.\n");
		return -1;
	}
	return 0;
}

JNIEXPORT jint JNICALL Java_cn_poco_video_NativeUtils_muxerMp4(JNIEnv *pEnv, jclass object, jstring MP4PATH, jstring AACPATH, jstring OUTPUTMP4)
{
    char *in_filename_v = (*pEnv)->GetStringUTFChars(pEnv, MP4PATH, 0);
    char *in_filename_a = (*pEnv)->GetStringUTFChars(pEnv, AACPATH, 0);
    char *out_filename = (*pEnv)->GetStringUTFChars(pEnv, OUTPUTMP4, 0);
	AVOutputFormat *ofmt = NULL;
	int haveaacdata = 1;
	//Input AVFormatContext and Output AVFormatContext
	AVFormatContext *ifmt_ctx_v = NULL, *ifmt_ctx_a = NULL,*ofmt_ctx = NULL;
	AVPacket pkt;
	int ret, i;
	int videoindex_v=-1,videoindex_out=-1;
	int audioindex_a=-1,audioindex_out=-1;
	int frame_index=0;
	int64_t cur_pts_v=0,cur_pts_a=0;

	av_register_all();
	//Input
	if ((ret = avformat_open_input(&ifmt_ctx_v, in_filename_v, 0, 0)) < 0) {
		LOGE("Could not open input file.");
		goto end;
	}
	if ((ret = avformat_find_stream_info(ifmt_ctx_v, 0)) < 0) {
		LOGE("Failed to retrieve input stream information");
		goto end;
	}
	if ((ret = avformat_open_input(&ifmt_ctx_a, in_filename_a, 0, 0)) < 0) { //如果文件为空，则表示音频轨为空
		LOGE("Could not open input audio file.");
		haveaacdata = 0;
		ret = 0;//goto end;
	}
	if(haveaacdata == 1)
	if ((ret = avformat_find_stream_info(ifmt_ctx_a, 0)) < 0) {//如果找不到音频流，则不写入音频
		LOGE("Failed to retrieve input stream information");
		haveaacdata = 0;
		ret = 0;
//		goto end;
	}
	LOGE("===========Input Information==========\n");
//	av_dump_format(ifmt_ctx_v, 0, in_filename_v, 0);
//	av_dump_format(ifmt_ctx_a, 0, in_filename_a, 0);
	LOGE("======================================\n");
	//Output
	avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);
	if (!ofmt_ctx) {
		LOGE("Could not create output context\n");
		ret = AVERROR_UNKNOWN;
		goto end;
	}
	ofmt = ofmt_ctx->oformat;
	for (i = 0; i < ifmt_ctx_v->nb_streams; i++) {
		//Create output AVStream according to input AVStream
		if(ifmt_ctx_v->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO){
		AVStream *in_stream = ifmt_ctx_v->streams[i];
		AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
		videoindex_v=i;
		if (!out_stream) {
			LOGE("Failed allocating output stream\n");
			ret = AVERROR_UNKNOWN;
			goto end;
		}
		videoindex_out=out_stream->index;
		//Copy the settings of AVCodecContext
		if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
			LOGE("Failed to copy context from input to output stream codec context\n");
			goto end;
		}
		out_stream->codec->codec_tag = 0;
		if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
			out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
		break;
		}
	}
	if(haveaacdata == 1)
	for (i = 0; i < ifmt_ctx_a->nb_streams; i++) {
		//Create output AVStream according to input AVStream
		if(ifmt_ctx_a->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO){
			AVStream *in_stream = ifmt_ctx_a->streams[i];
			AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
			audioindex_a=i;
			if (!out_stream) {
				LOGE("Failed allocating output stream\n");
				ret = AVERROR_UNKNOWN;
				goto end;
			}
			audioindex_out=out_stream->index;
			//Copy the settings of AVCodecContext
			if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
				LOGE("Failed to copy context from input to output stream codec context\n");
				goto end;
			}
			out_stream->codec->codec_tag = 0;
			if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
				out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;

			break;
		}
	}

	LOGE("==========Output Information==========\n");
	av_dump_format(ofmt_ctx, 0, out_filename, 1);
	LOGE("======================================\n");
	//Open output file
	if (!(ofmt->flags & AVFMT_NOFILE)) {
		if (avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE) < 0) {
			LOGE("Could not open output file '%s'", out_filename);
			goto end;
		}
	}
	//Write file header
	if (avformat_write_header(ofmt_ctx, NULL) < 0) {
		LOGE("Error occurred when opening output file\n");
		goto end;
	}

	//FIX
#if USE_H264BSF
	AVBitStreamFilterContext* h264bsfc =  av_bitstream_filter_init("h264_mp4toannexb");
#endif
#if USE_AACBSF
	AVBitStreamFilterContext* aacbsfc =  av_bitstream_filter_init("aac_adtstoasc");
#endif
	while (1) {
		AVFormatContext *ifmt_ctx;
		int stream_index=0;
		AVStream *in_stream, *out_stream;

		if(audioindex_a == -1 || haveaacdata==0) //如果没有音频直接写入视频
		{
			ifmt_ctx=ifmt_ctx_v;
			stream_index=videoindex_out;

			if(av_read_frame(ifmt_ctx, &pkt) >= 0){
				do{
					in_stream  = ifmt_ctx->streams[pkt.stream_index];
					out_stream = ofmt_ctx->streams[stream_index];

					if(pkt.stream_index==videoindex_v){
						//FIX：No PTS (Example: Raw H.264)
						//Simple Write PTS
						if(pkt.pts==AV_NOPTS_VALUE){
							//Write PTS
							AVRational time_base1=in_stream->time_base;
							//Duration between 2 frames (us)
							int64_t calc_duration=(double)AV_TIME_BASE/av_q2d(in_stream->r_frame_rate);
							//Parameters
							pkt.pts=(double)(frame_index*calc_duration)/(double)(av_q2d(time_base1)*AV_TIME_BASE);
							pkt.dts=pkt.pts;
							pkt.duration=(double)calc_duration/(double)(av_q2d(time_base1)*AV_TIME_BASE);
							frame_index++;
						}

						cur_pts_v=pkt.pts;
						break;
					}
				}while(av_read_frame(ifmt_ctx, &pkt) >= 0);
			}else{
				break;
			}
		}
		//Get an AVPacket
		else if(av_compare_ts(cur_pts_v,ifmt_ctx_v->streams[videoindex_v]->time_base,cur_pts_a,ifmt_ctx_a->streams[audioindex_a]->time_base) <= 0){
			ifmt_ctx=ifmt_ctx_v;
			stream_index=videoindex_out;
			if(av_read_frame(ifmt_ctx, &pkt) >= 0){
				do{
					in_stream  = ifmt_ctx->streams[pkt.stream_index];
					out_stream = ofmt_ctx->streams[stream_index];

					if(pkt.stream_index==videoindex_v){
						//FIX：No PTS (Example: Raw H.264)
						//Simple Write PTS
						if(pkt.pts==AV_NOPTS_VALUE){
							//Write PTS
							AVRational time_base1=in_stream->time_base;
							//Duration between 2 frames (us)
							int64_t calc_duration=(double)AV_TIME_BASE/av_q2d(in_stream->r_frame_rate);
							//Parameters
							pkt.pts=(double)(frame_index*calc_duration)/(double)(av_q2d(time_base1)*AV_TIME_BASE);
							pkt.dts=pkt.pts;
							pkt.duration=(double)calc_duration/(double)(av_q2d(time_base1)*AV_TIME_BASE);
							frame_index++;
						}

						cur_pts_v=pkt.pts;
						break;
					}
				}while(av_read_frame(ifmt_ctx, &pkt) >= 0);
			}else{
				break;
			}
		}else{
			ifmt_ctx=ifmt_ctx_a;
			stream_index=audioindex_out;
			if(av_read_frame(ifmt_ctx, &pkt) >= 0){
				do{
					in_stream  = ifmt_ctx->streams[pkt.stream_index];
					out_stream = ofmt_ctx->streams[stream_index];

					if(pkt.stream_index==audioindex_a){

						//FIX：No PTS
						//Simple Write PTS
						if(pkt.pts==AV_NOPTS_VALUE){
							//Write PTS
							AVRational time_base1=in_stream->time_base;
							//Duration between 2 frames (us)
							int64_t calc_duration=(double)AV_TIME_BASE/av_q2d(in_stream->r_frame_rate);
							//Parameters
							pkt.pts=(double)(frame_index*calc_duration)/(double)(av_q2d(time_base1)*AV_TIME_BASE);
							pkt.dts=pkt.pts;
							pkt.duration=(double)calc_duration/(double)(av_q2d(time_base1)*AV_TIME_BASE);
							frame_index++;
						}
						cur_pts_a=pkt.pts;

						break;
					}
				}while(av_read_frame(ifmt_ctx, &pkt) >= 0);
			}else{
				break;
			}

		}
		//FIX:Bitstream Filter
#if USE_H264BSF
		av_bitstream_filter_filter(h264bsfc, in_stream->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);
#endif
#if USE_AACBSF
		av_bitstream_filter_filter(aacbsfc, out_stream->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);
#endif

		//Convert PTS/DTS
		pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
		pkt.pos = -1;
		pkt.stream_index=stream_index;

		//LOGE("Write 1 Packet. size:%5d\tpts:%lld\n",pkt.size,pkt.pts);
		//Write
		if (av_interleaved_write_frame(ofmt_ctx, &pkt) < 0) {
			LOGE( "Error muxing packet\n");
			break;
		}
		av_free_packet(&pkt);

	}
	//Write file trailer
	av_write_trailer(ofmt_ctx);

#if USE_H264BSF
	av_bitstream_filter_close(h264bsfc);
#endif
#if USE_AACBSF
	av_bitstream_filter_close(aacbsfc);
#endif

end:
	avformat_close_input(&ifmt_ctx_v);
	avformat_close_input(&ifmt_ctx_a);
	/* close output */
	if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
		avio_close(ofmt_ctx->pb);
	avformat_free_context(ofmt_ctx);
	if (ret < 0 && ret != AVERROR_EOF) {
		LOGE("Error occurred.\n");
		return -1;
	}
	return 1;
}

typedef struct
{
	AVFormatContext* pFormatCtx;
	AVOutputFormat* fmt;
	AVStream* video_st;
	AVCodecContext* pCodecCtx;
	AVCodec* pCodec;
	AVPacket pkt;
	uint8_t* picture_buf;
	AVFrame* pFrame;
	int picture_size;
	int y_size;
	int framecnt;
}encodeh264bean, *Encodeh264BeanPtr;

static int open_input_file2(const char *filename, AVFormatContext **pFormatCtx, AVCodecContext **pCodecCtx, int index)
{
	int ret;
	AVCodec *dec;

	if ((ret = avformat_open_input(pFormatCtx, filename, NULL, NULL)) < 0) {
		printf("Cannot open input file\n");
		return ret;
	}

	if ((ret = avformat_find_stream_info(*pFormatCtx, NULL)) < 0) {
		printf("Cannot find stream information\n");
		return ret;
	}

	/* select the video stream */
	ret = av_find_best_stream(*pFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0);
	if (ret < 0) {
		printf("Cannot find a video stream in the input file\n");
		return ret;
	}
	if(index == 1)
	{
		video_stream_index1 = ret;
		*pCodecCtx = (*pFormatCtx)->streams[video_stream_index1]->codec;
	}
	else if (index == 2)
	{
		video_stream_index2 = ret;
		*pCodecCtx = (*pFormatCtx)->streams[video_stream_index2]->codec;
	}


	/* init the video decoder */
	if ((ret = avcodec_open2(*pCodecCtx, dec, NULL)) < 0) {
		printf("Cannot open video decoder\n");
		return ret;
	}
	av_dump_format(*pFormatCtx, 0, filename, 0);
	return 0;
}

int skipped_frame = 0;
int getNextFrame(AVFormatContext* pFormatCtx, AVCodecContext* pCodecCtx, AVFrame* pFrame, int index)
{
	int video_stream_index;
	if(index == 1)
		video_stream_index = video_stream_index1;
	else if(index == 2)
		video_stream_index = video_stream_index2;
	AVPacket packet;
	 while (1) {
		//read one packet.

		int ret = av_read_frame(pFormatCtx, &packet);
		if (ret< 0)
			return -1;
		if (packet.stream_index == video_stream_index) {
			int got_frame = 0;
			//decode
			//LOGE("packet size = %d   data[4] = %d\n", packet.size, packet.data[4]);
			 avcodec_get_frame_defaults(pFrame);
			ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_frame, &packet);
			if (ret < 0) {
				LOGE("Error decoding video\n");
				break;
			}
			//success
			if (got_frame) {
				pFrame->pts = av_frame_get_best_effort_timestamp(pFrame);
				break;
			}
		}
		av_free_packet(&packet);
	 }

	return 1;
}


int initEncodeh264Bean(Encodeh264BeanPtr pbean, const char* out_file, AVFormatContext *pFormatCtx, AVCodecContext *pCodecCtx, int index)
{

	int video_stream_index;
	if(index == 1)
		video_stream_index = video_stream_index1;
	else if(index ==2)
		video_stream_index = video_stream_index2;

	pbean->framecnt=0;

	//Method1.
	pbean->pFormatCtx = avformat_alloc_context();
	//Guess Format
	pbean->fmt = av_guess_format(NULL, out_file, NULL);
	pbean->pFormatCtx->oformat = pbean->fmt;

	//Method 2.
	//avformat_alloc_output_context2(&pFormatCtx, NULL, NULL, out_file);
	//fmt = pFormatCtx->oformat;


	//Open output URL
	if (avio_open(&pbean->pFormatCtx->pb,out_file, AVIO_FLAG_READ_WRITE) < 0){
		printf("Failed to open output file! \n");
		return -1;
	}

	pbean->video_st = avformat_new_stream(pbean->pFormatCtx, 0);
//	pbean->video_st->time_base.num = pFormatCtx->streams[video_stream_index]->time_base.num;
//	pbean->video_st->time_base.den = pFormatCtx->streams[video_stream_index]->time_base.den;

	if (pbean->video_st==NULL){
		return -1;
	}
	//Param that must set
	pbean->pCodecCtx = pbean->video_st->codec;
	//pCodecCtx->codec_id =AV_CODEC_ID_HEVC;
	pbean->pCodecCtx->codec_id = pbean->fmt->video_codec;
	pbean->pCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
	pbean->pCodecCtx->pix_fmt = PIX_FMT_YUV420P;
	pbean->pCodecCtx->width = pCodecCtx->width;
	pbean->pCodecCtx->height = pCodecCtx->height;
	pbean->pCodecCtx->time_base.num = 1;
	pbean->pCodecCtx->time_base.den = pFormatCtx->streams[video_stream_index]->avg_frame_rate.num / pFormatCtx->streams[video_stream_index]->avg_frame_rate.den;
	pbean->pCodecCtx->bit_rate = 1024*1024;
	pbean->pCodecCtx->gop_size=3;
	//H264
	pbean->pCodecCtx->me_range = 16;
	pbean->pCodecCtx->max_qdiff = 4;
	//pbean->pCodecCtx->qcompress = 0.6;
	pbean->pCodecCtx->qmin = 10;
	pbean->pCodecCtx->qmax = 51;

	//Optional Param
	pbean->pCodecCtx->max_b_frames=0;




	// Set Option
	AVDictionary *param = 0;
	//H.264
	if(pbean->pCodecCtx->codec_id == AV_CODEC_ID_H264) {
		av_dict_set(&param, "preset", "ultrafast", 0);
		av_dict_set(&param, "tune", "zerolatency", 0);
		//av_dict_set(&param, "profile", "baseline", 0);
		//av_dict_set(&param, "profile", "main", 0);
	}
	//H.265
	if(pbean->pCodecCtx->codec_id == AV_CODEC_ID_H265){
		av_dict_set(&param, "preset", "ultrafast", 0);
		av_dict_set(&param, "tune", "zero-latency", 0);
	}

	//Show some Information
	av_dump_format(pbean->pFormatCtx, 0, out_file, 1);

	pbean->pCodec = avcodec_find_encoder(pbean->pCodecCtx->codec_id);
	if (!pbean->pCodec){
		LOGE("Can not find encoder! \n");
		return -1;
	}
	if (avcodec_open2(pbean->pCodecCtx, pbean->pCodec,&param) < 0){
		LOGE("Failed to open encoder! \n");
		return -1;
	}


	pbean->pFrame = av_frame_alloc();
	pbean->pFrame->pts = 0;
	pbean->picture_size = avpicture_get_size(pbean->pCodecCtx->pix_fmt, pbean->pCodecCtx->width, pbean->pCodecCtx->height);
	pbean->picture_buf = (uint8_t *)av_malloc(pbean->picture_size);
	avpicture_fill((AVPicture *)(pbean->pFrame), pbean->picture_buf, pbean->pCodecCtx->pix_fmt, pbean->pCodecCtx->width, pbean->pCodecCtx->height);

	//Write File Header
	avformat_write_header(pbean->pFormatCtx,NULL);

	//av_new_packet(&pbean->pkt,pbean->picture_size);

	pbean->y_size = pbean->pCodecCtx->width * pbean->pCodecCtx->height;

	return 1;
}

int flush_encoder(AVFormatContext *fmt_ctx, unsigned int stream_index){
	int ret;
	int got_frame;
	AVPacket enc_pkt;
	if (!(fmt_ctx->streams[stream_index]->codec->codec->capabilities &
		CODEC_CAP_DELAY))
		return 0;
	while (1) {
		enc_pkt.data = NULL;
		enc_pkt.size = 0;
		av_init_packet(&enc_pkt);
		ret = avcodec_encode_video2 (fmt_ctx->streams[stream_index]->codec, &enc_pkt,
			NULL, &got_frame);
		av_frame_free(NULL);
		if (ret < 0)
			break;
		if (!got_frame){
			ret=0;
			break;
		}
	//	LOGE("Flush Encoder: Succeed to encode 1 frame!\tsize:%5d\n",enc_pkt.size);
		//LOGE("pts = %lld   dts = %lld", enc_pkt.pts, enc_pkt.dts);
		/* mux encoded frame */
		ret = av_write_frame(fmt_ctx, &enc_pkt);
		if (ret < 0)
			break;
	}
	return ret;
}

int encodeH264(Encodeh264BeanPtr pbean)
{
	/*pbean->pFrame->data[0] = picref->data[0];              // Y
	pbean->pFrame->data[1] = picref->data[1];
	pbean->pFrame->data[2] = picref->data[2];*/

	int got_picture=0;
	//Encode
	av_new_packet(&pbean->pkt,pbean->picture_size);
	//pbean->pFrame->pts = picref->pts;
	LOGE("start avcodec_encode_video2");
	int ret = avcodec_encode_video2(pbean->pCodecCtx, &pbean->pkt,pbean->pFrame, &got_picture);
	LOGE("end avcodec_encode_video2");
	if(ret < 0){
		LOGE("Failed to encode! \n");
		return -1;
	}
	if (got_picture==1){
//		LOGE("Succeed to encode frame: %5d\tsize:%5d\n",pbean->framecnt,pbean->pkt.size);
		pbean->framecnt++;
		pbean->pkt.stream_index = pbean->video_st->index;
	//	int64_t calc_duration=(double)AV_TIME_BASE/av_q2d(pFormatCtx->streams[video_stream_index]->r_frame_rate);
	//	pbean->pkt.pts = (double)(pbean->framecnt*calc_duration)/(double)(av_q2d(pFormatCtx->streams[video_stream_index]->time_base)*AV_TIME_BASE);
		ret = av_write_frame(pbean->pFormatCtx, &pbean->pkt);
		av_free_packet(&pbean->pkt);
	//	LOGE("编码一帧，  pts = %lld \n", pbean->pFrame->pts);

		}


    pbean->pFrame->pts++;
	return 0;
}

static int init_filter_graph(AVFilterGraph **pGraph, AVFilterContext **pInput1, AVFilterContext **pInput2, AVFilterContext **pOutput, AVFormatContext *pFormatCtx, AVCodecContext *pCodecCtx, int time)
{
	AVFilterGraph* tFilterGraph;
	AVFilterContext* tBufferContext1;
	AVFilter* tBuffer1;
	AVFilterContext* tBufferContext2;
	AVFilter* tBuffer2;
	AVFilterContext* tOverlayContext;
	AVFilter* tOverlay;
	AVFilterContext* tBufferSinkContext;
	AVFilter* tBufferSink;
	AVFilter* tFormat;
	AVFilterContext* tFormatContext;

	int tError;

	/* Create a new filtergraph, which will contain all the filters. */
	tFilterGraph = avfilter_graph_alloc();
	    char args[512];
    snprintf(args, sizeof(args),
		"video_size=%dx%d:pix_fmt=0:time_base=%d/%d:pixel_aspect=1/1",pCodecCtx->width,pCodecCtx->height, 1,
		pFormatCtx->streams[video_stream_index1]->avg_frame_rate.num/pFormatCtx->streams[video_stream_index1]->avg_frame_rate.den);

	if (!tFilterGraph) {
		return -1;
	}

	{ // BUFFER FILTER 1
		tBuffer1 = avfilter_get_by_name("buffer");
		if (!tBuffer1) {
			return -1;
		}
			int ret = avfilter_graph_create_filter(&tBufferContext1, tBuffer1, "top",
				args, NULL, tFilterGraph);
			if (ret < 0) {
				LOGE("Cannot create buffer source\n");
				return ret;
			}
	}

	{ // COLOR FILTER
		tBuffer2 = avfilter_get_by_name("buffer");
		if (!tBuffer2) {
			return -1;
		}
		if (1) {

			int ret = avfilter_graph_create_filter(&tBufferContext2, tBuffer2, "bottom",
				args, NULL, tFilterGraph);
			if (ret < 0) {
				LOGE("Cannot create buffer source\n");
				return ret;
			}
		}
	}

	{ // OVERLAY FILTER
		tOverlay = avfilter_get_by_name("blend");
		if (!tOverlay) {
			return -1;
		}
		tOverlayContext = avfilter_graph_alloc_filter(tFilterGraph, tOverlay, "blend");
		if (!tOverlayContext) {
			return -1;
		}

		char blendargs[512];
		 snprintf(blendargs, sizeof(blendargs),
		"A*(if(gte(T,%d),1,T/%d))+B*(1-(if(gte(T,%d),1,T/%d)))",time*10000, time*10000, time*10000, time*10000);
		AVDictionary *tOptionsDict = NULL;
		av_dict_set(&tOptionsDict, "all_mode", "normal", 0);
		av_dict_set(&tOptionsDict, "all_opacity", "1", 0);
	//	av_dict_set(&tOptionsDict, "all_expr", "if(eq(mod(X,2),mod(Y,2)),A,B)", 0);
		av_dict_set(&tOptionsDict, "all_expr", blendargs,0);
		tError = avfilter_init_dict(tOverlayContext, &tOptionsDict);
		av_dict_free(&tOptionsDict);
		if (tError < 0) {
			return tError;
		}
	}

	{ // FORMAT FILTER
		tFormat = avfilter_get_by_name("format");
		if (!tFormat) {
			// Could not find the tFormat filter.
			return -1;
		}

		tFormatContext = avfilter_graph_alloc_filter(tFilterGraph, tFormat, "format");
		if (!tFormatContext) {
			// Could not allocate the tFormat instance.
			return -1;
		}
		AVDictionary *tOptionsDict = NULL;
		av_dict_set(&tOptionsDict, "pix_fmts", "yuv420p", 0);
		tError = avfilter_init_dict(tFormatContext, &tOptionsDict);
		av_dict_free(&tOptionsDict);
		if (tError < 0) {
			// Could not initialize the tFormat filter.
			return tError;
		}
	}

	{ // BUFFERSINK FILTER
		tBufferSink = avfilter_get_by_name("buffersink");
		if (!tBufferSink) {
			return -1;
		}

		tBufferSinkContext = avfilter_graph_alloc_filter(tFilterGraph, tBufferSink, "sink");
		if (!tBufferSinkContext) {
			return -1;
		}

		tError = avfilter_init_str(tBufferSinkContext, NULL);
		if (tError < 0) {
			return tError;
		}
	}

	// Linking graph
	tError = avfilter_link(tBufferContext1, 0, tOverlayContext, 0);
	if (tError >= 0)
	{
		tError = avfilter_link(tBufferContext2, 0, tOverlayContext, 1);
	}
	if (tError >= 0)
	{
		tError = avfilter_link(tOverlayContext, 0, tFormatContext, 0);
	}
	if (tError >= 0)
	{
		tError = avfilter_link(tFormatContext, 0, tBufferSinkContext, 0);
	}
	if (tError < 0) { // Error connecting filters.
		return tError;
	}

	tError = avfilter_graph_config(tFilterGraph, NULL);
	if (tError < 0) {
		return tError;
	}

	*pGraph = tFilterGraph;
	*pInput1 = tBufferContext1;
	*pInput2 = tBufferContext2;
	*pOutput = tBufferSinkContext;

	return 0;
}

JNIEXPORT jint JNICALL Java_cn_poco_video_NativeUtils_getH264FromFile(JNIEnv *pEnv, jclass object, jstring MP4PATH, jstring OUTH264PATH)
{
	AVOutputFormat *ofmt_v = NULL;
	//（Input AVFormatContext and Output AVFormatContext）
	AVFormatContext *ifmt_ctx = NULL,*ofmt_ctx_v = NULL;
	AVPacket pkt;
	int ret, i;
	int videoindex=-1;
	int frame_index=0;

	const char *in_filename  = (*pEnv)->GetStringUTFChars(pEnv, MP4PATH, 0);
	//char *in_filename  = "cuc_ieschool.mkv";
	const char *out_filename_v = (*pEnv)->GetStringUTFChars(pEnv, OUTH264PATH, 0);

	av_register_all();
	//Input
	if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
		LOGE( "Could not open input file.");
		goto end;
	}
	if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
		LOGE( "Failed to retrieve input stream information");
		goto end;
	}

	//Output
	avformat_alloc_output_context2(&ofmt_ctx_v, NULL, NULL, out_filename_v);
	if (!ofmt_ctx_v) {
		LOGE( "Could not create output context\n");
		ret = AVERROR_UNKNOWN;
		goto end;
	}
	ofmt_v = ofmt_ctx_v->oformat;

    AVStream *out_stream = NULL;
	for (i = 0; i < ifmt_ctx->nb_streams; i++) {
			//Create output AVStream according to input AVStream
			AVFormatContext *ofmt_ctx;
			AVStream *in_stream = ifmt_ctx->streams[i];


			if(ifmt_ctx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO){
				videoindex=i;
				out_stream=avformat_new_stream(ofmt_ctx_v, in_stream->codec->codec);
				ofmt_ctx=ofmt_ctx_v;
			}
			else{
				continue;
			}

			if (!out_stream) {
				LOGE( "Failed allocating output stream\n");
				ret = AVERROR_UNKNOWN;
				goto end;
			}
			//Copy the settings of AVCodecContext
			if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
				LOGE( "Failed to copy context from input to output stream codec context\n");
				goto end;
			}
			out_stream->codec->codec_tag = 0;

			if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
				out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}

	//Dump Format------------------
	LOGE("\n==============Input Video=============\n");
	av_dump_format(ifmt_ctx, 0, in_filename, 0);
	LOGE("\n==============Output Video============\n");
	av_dump_format(ofmt_ctx_v, 0, out_filename_v, 1);

	//Open output file
	if (!(ofmt_v->flags & AVFMT_NOFILE)) {
		if (avio_open(&ofmt_ctx_v->pb, out_filename_v, AVIO_FLAG_WRITE) < 0) {
			LOGE( "Could not open output file '%s'", out_filename_v);
			goto end;
		}
	}

	//Write file header
	if (avformat_write_header(ofmt_ctx_v, NULL) < 0) {
		LOGE( "Error occurred when opening video output file\n");
		goto end;
	}

//#if USE_H264BSF
	AVBitStreamFilterContext* h264bsfc =  av_bitstream_filter_init("h264_mp4toannexb");
//#endif

	while (1) {
		AVFormatContext *ofmt_ctx;
		AVStream *in_stream, *out_stream;
		//Get an AVPacket
		if (av_read_frame(ifmt_ctx, &pkt) < 0)
			break;
		in_stream  = ifmt_ctx->streams[pkt.stream_index];


		if(pkt.stream_index==videoindex){
			out_stream = ofmt_ctx_v->streams[0];
			ofmt_ctx=ofmt_ctx_v;
	//		LOGE("Write Video Packet. size:%d\tpts:%lld\n",pkt.size,pkt.pts);
//#if USE_H264BSF
			av_bitstream_filter_filter(h264bsfc, in_stream->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);
//#endif
		}
		else{
			continue;
		}


		//Convert PTS/DTS
		pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
		pkt.pos = -1;
		pkt.stream_index=0;
		//Write
		if (av_interleaved_write_frame(ofmt_ctx, &pkt) < 0) {
			LOGE( "Error muxing packet\n");
			break;
		}
		//LOGE("Write %8d frames to output file\n",frame_index);
		av_free_packet(&pkt);
		frame_index++;
	}

//#if USE_H264BSF
	av_bitstream_filter_close(h264bsfc);
//#endif

	//Write file trailer
	av_write_trailer(ofmt_ctx_v);
end:
	avformat_close_input(&ifmt_ctx);
	/* close output */

	if (ofmt_ctx_v && !(ofmt_v->flags & AVFMT_NOFILE))
		avio_close(ofmt_ctx_v->pb);

	avformat_free_context(ofmt_ctx_v);


	if (ret < 0 && ret != AVERROR_EOF) {
		LOGE( "Error occurred.\n");
		return -1;
	}
    return 1;
}

 void copy(FILE *src, FILE *dst)
{
	int r;
	unsigned char buf[1024*10];
	while( (r=fread(buf, sizeof(unsigned char), 1024*10, src)) > 0 )
	{
		fwrite(buf, r, 1, dst);
	//	LOGE("write %d \n", r);
	}
}

JNIEXPORT jint JNICALL Java_cn_poco_video_NativeUtils_mixH264(JNIEnv *pEnv, jclass object, jstring H264, jstring DSTH264)
{
    const char *src  = (*pEnv)->GetStringUTFChars(pEnv, H264, 0);
	//char *in_filename  = "cuc_ieschool.mkv";
	const char *dst = (*pEnv)->GetStringUTFChars(pEnv, DSTH264, 0);
	FILE *f1 = fopen(src, "rb+");
	FILE *f2 = fopen(dst, "ab+");

	copy(f1, f2);

	fclose(f1);
	fflush(f2);
	fclose(f2);
    return 1;
}

JNIEXPORT jbyteArray JNICALL Java_cn_poco_video_NativeUtils_getNextFrameYUVFromFile(JNIEnv *pEnv, jclass object, jstring video_in, jint videoindex, jobject width, jobject height, jobject pts, jobject len)
{
    jclass IntClass = (*pEnv)->FindClass(pEnv, "java/lang/Integer");

       clock_t start, finish;
        double  duration;
           /* 测量一个事件持续的时间*/
        start = clock();
        AVFormatContext *pFormatCtx = NULL;
        AVCodecContext *pCodecCtx = NULL;
        if(pMyVideoGroup == NULL)
        {
            LOGE("初始化视频组");
            initVideoGroup(6);
        }
        pFormatCtx = pMyVideoGroup->pFormatCtx[videoindex];
        pCodecCtx = pMyVideoGroup->pCodecCtx[videoindex];

        int ret;
        AVPacket packet;
        AVFrame *pframe;
        int got_frame;
        bool foundFrame = false;
    //    jobject bitmap = NULL;
        av_register_all();
        const char *videofilename = (*pEnv)->GetStringUTFChars(pEnv, video_in, 0);

        if (pCodecCtx == NULL)  //判断解码器是否已经初始化
        {
            LOGE("打开文件");
            if ((ret = open_input_file(videofilename, &pFormatCtx, &pCodecCtx, videoindex)) < 0)
                goto end;
            pMyVideoGroup->pCodecCtx[videoindex] = pCodecCtx;
            pMyVideoGroup->pFormatCtx[videoindex] = pFormatCtx;
        }

        AVFrame	*pFrameYUV;
        pFrameYUV=avcodec_alloc_frame();
        pframe = avcodec_alloc_frame();
        struct SwsContext *img_convert_ctx;
        img_convert_ctx = sws_getContext
                              (
                                  pCodecCtx->width,
                                  pCodecCtx->height,
                                  pCodecCtx->pix_fmt,
                                  pCodecCtx->width,
                                  pCodecCtx->height,
                                  AV_PIX_FMT_RGBA,
                                  SWS_BILINEAR,
                                  NULL,
                                  NULL,
                                  NULL
                              );

        /* read all packets */
        uint8_t *out_buffer = (uint8_t *)av_malloc(avpicture_get_size(AV_PIX_FMT_RGBA, pCodecCtx->width, pCodecCtx->height));
         avpicture_fill((AVPicture *)pFrameYUV, out_buffer, AV_PIX_FMT_RGBA, pCodecCtx->width, pCodecCtx->height);


         int skipped_frame = 0;
        while (1) {
    		ret = av_read_frame(pFormatCtx, &packet);

            if (ret< 0)  //PACKET读取完毕后   还要读取残留在PACKET的帧
            {
                av_seek_frame(pFormatCtx, -1, 0, AVSEEK_FLAG_BACKWARD);
                break;
            }
            if (packet.stream_index == pMyVideoGroup->video_stream_index_group[videoindex]) {

                avcodec_get_frame_defaults(pframe);
                got_frame = 0;
                ret = avcodec_decode_video2(pCodecCtx, pframe, &got_frame, &packet);
                if (ret < 0) {
                    LOGE( "Error decoding video\n");
                    break;
                }
                if (got_frame) {

                    pframe->pts = av_frame_get_best_effort_timestamp(pframe);
                    sws_scale(img_convert_ctx, (const uint8_t* const*)pframe->data, pframe->linesize, 0, pCodecCtx->height, pFrameYUV->data, pFrameYUV->linesize);  //转为为标准RGBA格式

   //                 LOGE("GOT FRAME!");
                    foundFrame = true;
                        break;

                }
                else
                    pMyVideoGroup->skipped_frame_group[videoindex]++;
            }
             av_free_packet(&packet);
        }

        if(foundFrame == false && pMyVideoGroup->skipped_frame_group[videoindex])  //如果之前的帧没有定位到 ，  尝试解析剩余的帧
        {
        LOGE("解析剩下帧");
            int i=0;
            for(i=pMyVideoGroup->skipped_frame_group[videoindex]; i>0; i--)  //解码packet中剩下的之前解码失败的帧
            {
                got_frame = 0;
                ret = avcodec_decode_video2(pCodecCtx, pframe, &got_frame, &packet);
                if (got_frame) {
                   // packet.pts = av_rescale_q(packet.pts,video_dec_st->time_base,video_enc_st->time_base);
                   LOGE("pre_pts = %lld", pframe->pts);
                   pframe->pts = av_frame_get_best_effort_timestamp(pframe);
                   LOGE("pts = %lld", pframe->pts);
                   LOGE("packet_pts = %lld", packet.pts);
                    sws_scale(img_convert_ctx, (const uint8_t* const*)pframe->data, pframe->linesize, 0, pCodecCtx->height, pFrameYUV->data, pFrameYUV->linesize);  //转为为标准420P的YUV格式
                   --pMyVideoGroup->skipped_frame_group[videoindex];
                    break;
                }
            }
        }


       jfieldID id = (*pEnv)->GetFieldID(pEnv, IntClass, "value", "I");
       jclass FloatClass = (*pEnv)->FindClass(pEnv, "java/lang/Float");
        jfieldID id2 = (*pEnv)->GetFieldID(pEnv, FloatClass, "value", "F");
        (*pEnv)->SetIntField(pEnv, width, id, pCodecCtx->width);
        (*pEnv)->SetIntField(pEnv, height, id, pCodecCtx->height);
        float jnipts =  pframe->pts * av_q2d(pFormatCtx->streams[pMyVideoGroup->video_stream_index_group[videoindex]]->time_base);
      //  LOGE("jni pts = %f", jnipts);
        (*pEnv)->SetFloatField(pEnv, pts, id2, jnipts);
         (*pEnv)->SetIntField(pEnv, len, id, avpicture_get_size(PIX_FMT_NV12, pCodecCtx->width, pCodecCtx->height));
       jbyteArray  jbarray =  (*pEnv)->NewByteArray(pEnv,avpicture_get_size(PIX_FMT_NV12, pCodecCtx->width, pCodecCtx->height));
       jbyte *jy = (jbyte*)out_buffer;
         (*pEnv)->SetByteArrayRegion(pEnv,jbarray, 0, avpicture_get_size(PIX_FMT_NV12, pCodecCtx->width, pCodecCtx->height), jy);

        if(ret<0)
        {
            LOGE("已經讀取完視頻！");
             (*pEnv)->SetIntField(pEnv, len, id, -1);
        }
        av_free_packet(&packet);
        av_free(out_buffer);
      //  AndroidBitmap_unlockPixels(pEnv, bitmap);
        out_buffer = NULL;
        av_free(pFrameYUV);
        av_free(pframe);
        pframe = NULL;
        pFrameYUV = NULL;
        sws_freeContext(img_convert_ctx);
        img_convert_ctx = NULL;
    end:

        if (ret < 0 && ret != AVERROR_EOF) {
            char buf[1024];
            av_strerror(ret, buf, sizeof(buf));
            LOGE("Error occurred: %s\n", buf);
            return -1;
        }
         finish = clock();
         duration = (double)(finish - start) / CLOCKS_PER_SEC;
         LOGE( "%f seconds\n", duration );
        return jbarray;

}

typedef struct
{
    //保存视频信息变量
    AVFormatContext *pFormatCtx1;
    AVFormatContext *pFormatCtx2;
    AVCodecContext *pCodecCtx1;
    AVCodecContext *pCodecCtx2;

    //保存AVFILTER有用的变量
    AVFilterContext *inputContexts1;
    AVFilterContext *inputContexts2;
    AVFilterContext *outputContext;
    AVFilterGraph *filter_graph;

    //保存index
    int video_stream_index1，video_stream_index2;
    int pts;
    int *skipped_frame_group;
} blendInfo ,*blendInfoPtr;

blendInfoPtr m_blendInfoPtr = NULL;
JNIEXPORT jbyteArray JNICALL Java_cn_poco_video_NativeUtils_getBlendVideoFrameYUV(JNIEnv *pEnv, jclass object, jstring MP4PATH1, jstring MP4PATH2, jint type, jint time, jobject width, jobject height, jobject pts, jobject len)
{
    LOGE("开始融合视频...");
    jclass IntClass = (*pEnv)->FindClass(pEnv, "java/lang/Integer");
    char *in_vFilename1 = (*pEnv)->GetStringUTFChars(pEnv, MP4PATH1, 0);
    char *in_vFilename2 = (*pEnv)->GetStringUTFChars(pEnv, MP4PATH2, 0);
    int ret;
    AVPacket packet;
    int got_frame;
   	avcodec_register_all();
    av_register_all();
    avfilter_register_all();

    AVFilterContext *inputContexts1, *inputContexts2;
    AVFilterContext *outputContext;
    AVFilterGraph *filter_graph;
   	AVFormatContext *pFormatCtx = NULL, *pFormatCtx2 = NULL;
   	AVCodecContext *pCodecCtx = NULL, *pCodecCtx2 = NULL;
    if(m_blendInfoPtr == NULL)
    {
        m_blendInfoPtr = (blendInfoPtr)malloc(sizeof(blendInfo));
        m_blendInfoPtr->pFormatCtx1 = NULL;
        m_blendInfoPtr->pFormatCtx2 = NULL;
        m_blendInfoPtr->pCodecCtx1 = NULL;
        m_blendInfoPtr->pCodecCtx2 = NULL;
        m_blendInfoPtr->inputContexts1 = NULL;
        m_blendInfoPtr->inputContexts2 = NULL;
        m_blendInfoPtr->outputContext = NULL;
        m_blendInfoPtr->filter_graph = NULL;
        if ((ret = open_input_file2(in_vFilename1, &pFormatCtx, &pCodecCtx, 1)) < 0)
               goto end;
        if ((ret = open_input_file2(in_vFilename2, &pFormatCtx2, &pCodecCtx2, 2)) < 0)
               goto end;

        if ((ret = init_filter_graph(&filter_graph, &inputContexts1, &inputContexts2 ,&outputContext, pFormatCtx2, pCodecCtx2, time)) < 0)
               goto end;

        m_blendInfoPtr->pFormatCtx1 =  pFormatCtx;
        m_blendInfoPtr->pFormatCtx2 =  pFormatCtx2;
        m_blendInfoPtr->pCodecCtx1 =  pCodecCtx;
        m_blendInfoPtr->pCodecCtx2 =  pCodecCtx2;

        m_blendInfoPtr->outputContext = outputContext;
        m_blendInfoPtr->filter_graph = filter_graph;
        m_blendInfoPtr->inputContexts1 = inputContexts1;
        m_blendInfoPtr->inputContexts2 = inputContexts2;

        m_blendInfoPtr->pts = 0;
    }
    else
    {
        pFormatCtx =  m_blendInfoPtr->pFormatCtx1;
        pFormatCtx2 =  m_blendInfoPtr->pFormatCtx2;
        pCodecCtx =  m_blendInfoPtr->pCodecCtx1;
        pCodecCtx2 =  m_blendInfoPtr->pCodecCtx2;

        outputContext = m_blendInfoPtr->outputContext;
        filter_graph = m_blendInfoPtr->filter_graph;
        inputContexts1 = m_blendInfoPtr->inputContexts1;
        inputContexts2 = m_blendInfoPtr->inputContexts2;
    }

   	// 开始解码两个视频并推送到Avfilter处理
   	int in_width=pCodecCtx->width;
   	int in_height=pCodecCtx->height;
   	AVFrame *frame_in;
   	frame_in=av_frame_alloc();

   	struct SwsContext *img_convert_ctx;
   	img_convert_ctx = sws_getContext(in_width, in_height, pCodecCtx->pix_fmt, in_width, in_height, PIX_FMT_NV12, SWS_BICUBIC, NULL, NULL, NULL);

   	frame_in->width=in_width;
   	frame_in->height=in_height;
   	frame_in->format=AV_PIX_FMT_YUV420P;

   	AVFrame *Frame = av_frame_alloc();
   	AVFilterBufferRef *picref;
    uint8_t *out_buffer = (uint8_t *)av_malloc(avpicture_get_size(PIX_FMT_NV12, pCodecCtx->width, pCodecCtx->height));
    bool HaveGotFrame = false;
    AVFrame *pFrame_out = av_frame_alloc();
   	while((ret = getNextFrame(pFormatCtx,pCodecCtx,Frame, 1)) > 0)
   	{

   		if((ret = getNextFrame(pFormatCtx2, pCodecCtx2,frame_in, 2))>0)
   		{
            LOGE("正在塞bottom层数据. pts = %lld             ", Frame->pts);
            if (av_buffersrc_add_frame(inputContexts2, Frame) < 0) {
                LOGE("Error while feeding the filtergraph\n");
                break;
            }

   			frame_in->pts = Frame->pts;//av_rescale_q_rnd(frame_in->pts, pCodecCtx2->time_base, pCodecCtx->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
   			LOGE("正在塞top层数剧. pts = %lld             ", frame_in->pts);
   			if ((ret = av_buffersrc_add_frame(inputContexts1, frame_in)) < 0)
   			{
   				LOGE("Error while add frame.\n");
   				break;
   			}
   			av_frame_unref(frame_in);
   		}
   		else
   		    break;

   			// pull filtered pictures from the filtergraph
   			while (1)
   			{
   				ret = av_buffersink_get_buffer_ref(outputContext, &picref, 0);
                      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                          break;
                      if (ret < 0)
                          break;

                       if (picref) {
   						//avfilter_copy_buf_props(pFrame_out,picref);
   						avpicture_fill((AVPicture *)pFrame_out, out_buffer, PIX_FMT_NV12, in_width, in_height);
   						sws_scale(img_convert_ctx, (const uint8_t* const*)picref->data, picref->linesize, 0, in_height, pFrame_out->data, pFrame_out->linesize);

   						LOGE("最终取出数据.   pts = %lld             \n", picref->pts);

   						avfilter_unref_bufferp(&picref);
   						HaveGotFrame = true;
   						break;
   					}
   				}
   		av_frame_unref(Frame);
   		if(HaveGotFrame == true)
   		    break;
   	}

    //avfilter_graph_free(&filter_graph);

    jfieldID id = (*pEnv)->GetFieldID(pEnv, IntClass, "value", "I");
    jclass FloatClass = (*pEnv)->FindClass(pEnv, "java/lang/Float");
    jfieldID id2 = (*pEnv)->GetFieldID(pEnv, FloatClass, "value", "F");
    (*pEnv)->SetIntField(pEnv, width, id, pCodecCtx->width);
    (*pEnv)->SetIntField(pEnv, height, id, pCodecCtx->height);
    float jnipts =  (m_blendInfoPtr->pts) * (float)1/30;
    m_blendInfoPtr->pts++;
    LOGE("jni pts = %f", jnipts);
    (*pEnv)->SetFloatField(pEnv, pts, id2, jnipts);
    (*pEnv)->SetIntField(pEnv, len, id, avpicture_get_size(PIX_FMT_NV12, pCodecCtx->width, pCodecCtx->height));
    jbyteArray  jbarray =  (*pEnv)->NewByteArray(pEnv,avpicture_get_size(PIX_FMT_NV12, pCodecCtx->width, pCodecCtx->height));
    jbyte *jy = (jbyte*)out_buffer;
    (*pEnv)->SetByteArrayRegion(pEnv,jbarray, 0, avpicture_get_size(PIX_FMT_NV12, pCodecCtx->width, pCodecCtx->height), jy);

    if(ret<0)
    {
         LOGE("已經讀取完視頻！");
        (*pEnv)->SetIntField(pEnv, len, id, -1);
    }
   end:
//   	//回收解码和水印的容器
//       avfilter_graph_free(&filter_graph);
//       if (pCodecCtx)
//           avcodec_close(pCodecCtx);
//   	 if (pCodecCtx2)
//           avcodec_close(pCodecCtx2);
//       avformat_close_input(&pFormatCtx);
//   	avformat_close_input(&pFormatCtx2);
//       if (ret < 0 && ret != AVERROR_EOF) {
//           char buf[1024];
//           av_strerror(ret, buf, sizeof(buf));
//           printf("Error occurred: %s\n", buf);
//           return -1;
//       }
       sws_freeContext(img_convert_ctx);
       av_free(out_buffer);
       out_buffer = NULL;
       return jbarray;
}

JNIEXPORT jint JNICALL Java_cn_poco_video_NativeUtils_blendVideo(JNIEnv *pEnv, jclass object, jstring MP4PATH1, jstring MP4PATH2, jstring OUTPUTH264, jint type, jint time)
{
    LOGE("开始融合视频...");
    char *in_vFilename1 = (*pEnv)->GetStringUTFChars(pEnv, MP4PATH1, 0);
    char *in_vFilename2 = (*pEnv)->GetStringUTFChars(pEnv, MP4PATH2, 0);
    char *out_vFilename = (*pEnv)->GetStringUTFChars(pEnv, OUTPUTH264, 0);
       int ret;
       AVPacket packet;
       int got_frame;

   	AVFilterContext *inputContexts1, *inputContexts2;
   	AVFilterContext *outputContext;
   	AVFilterGraph *filter_graph;

   	AVFormatContext *pFormatCtx = NULL, *pFormatCtx2 = NULL;
   	AVCodecContext *pCodecCtx = NULL, *pCodecCtx2 = NULL;

   	avcodec_register_all();
       av_register_all();
       avfilter_register_all();


   //    char *out_videofilename = (*pEnv)->GetStringUTFChars(pEnv, pFileName, 0);
   	if ((ret = open_input_file2(in_vFilename1, &pFormatCtx, &pCodecCtx, 1)) < 0)
           goto end;
   	if ((ret = open_input_file2(in_vFilename2, &pFormatCtx2, &pCodecCtx2, 2)) < 0)
           goto end;

   	if ((ret = init_filter_graph(&filter_graph,&inputContexts1,&inputContexts2,&outputContext, pFormatCtx2, pCodecCtx2, time)) < 0)
           goto end;


       ///初始化H264编码器
       Encodeh264BeanPtr pbean = (Encodeh264BeanPtr)malloc(sizeof(encodeh264bean));
   	initEncodeh264Bean(pbean, out_vFilename, pFormatCtx2, pCodecCtx2, 2);

   	// 开始解码两个视频并推送到Avfilter处理
   	int in_width=pCodecCtx->width;
   	int in_height=pCodecCtx->height;
   	AVFrame *frame_in;
   	frame_in=av_frame_alloc();

   	struct SwsContext *img_convert_ctx;
   	img_convert_ctx = sws_getContext(in_width, in_height, pCodecCtx->pix_fmt, in_width, in_height, PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);

   	frame_in->width=in_width;
   	frame_in->height=in_height;
   	frame_in->format=AV_PIX_FMT_YUV420P;

   	AVFrame *Frame = av_frame_alloc();
   	AVFilterBufferRef *picref;


   	while(getNextFrame(pFormatCtx,pCodecCtx,Frame, 1) > 0)
   	{

   		LOGE("正在塞bottom层数据. pts = %lld             ", Frame->pts);
   		if (av_buffersrc_add_frame(inputContexts2, Frame) < 0) {
   			printf("Error while feeding the filtergraph\n");
   			break;
   		}
   				if(getNextFrame(pFormatCtx2, pCodecCtx2,frame_in, 2)>0)
   				{

   					frame_in->pts = Frame->pts;//av_rescale_q_rnd(frame_in->pts, pCodecCtx2->time_base, pCodecCtx->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
   					LOGE("正在塞top层数剧. pts = %lld             ", frame_in->pts);
   					if (av_buffersrc_add_frame(inputContexts1, frame_in) < 0)
   					{
   						printf("Error while add frame.\n");
   						break;
   					}
   				}

   				// pull filtered pictures from the filtergraph
   				while (1)
   				{
   					ret = av_buffersink_get_buffer_ref(outputContext, &picref, 0);
                       if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                           break;
                       if (ret < 0)
                           break;

                       if (picref) {
   						//avfilter_copy_buf_props(pFrame_out,picref);
   						//avpicture_fill((AVPicture *)pFrame_out, out_buffer, PIX_FMT_YUV420P, in_width, in_height);
   						sws_scale(img_convert_ctx, (const uint8_t* const*)picref->data, picref->linesize, 0, in_height, pbean->pFrame->data, pbean->pFrame->linesize);

   						LOGE("最终取出数据.   pts = %lld             \n", picref->pts);
   						encodeH264(pbean);
   						LOGE("编码成功");
   						avfilter_unref_bufferp(&picref);
   					}
   				}
   		av_frame_unref(Frame);
   	}

   	//av_frame_free(&pFrame);
   	av_frame_free(&frame_in);
       avfilter_graph_free(&filter_graph);


   	//刷新编码器以及回收编码器所用到的容器
   	ret = flush_encoder(pbean->pFormatCtx, 0);
   	if (ret < 0) {
   		printf("Flushing video encoder failed\n");
   		return -1;
   	}
   	//Write file trailer
   	av_write_trailer(pbean->pFormatCtx);
   	//Clean video encoder
   	if (pbean->video_st){
   		avcodec_close(pbean->video_st->codec);
   		av_free(pbean->pFrame);
   		av_free(pbean->picture_buf);
   	}
   	avio_close(pbean->pFormatCtx->pb);
   	avformat_free_context(pbean->pFormatCtx);

   end:
   	//回收解码和水印的容器
       avfilter_graph_free(&filter_graph);
       if (pCodecCtx)
           avcodec_close(pCodecCtx);
   	 if (pCodecCtx2)
           avcodec_close(pCodecCtx2);
       avformat_close_input(&pFormatCtx);
   	avformat_close_input(&pFormatCtx2);
       if (ret < 0 && ret != AVERROR_EOF) {
           char buf[1024];
           av_strerror(ret, buf, sizeof(buf));
           printf("Error occurred: %s\n", buf);
           return -1;
       }

       return 1;
}

JNIEXPORT jint JNICALL Java_cn_poco_video_NativeUtils_speedFilter(JNIEnv *pEnv, jclass object, jstring INPUTPATH, jstring OUTPUTMP4, jint startTime, jint endTime, jfloat speedratio)
{
	AVOutputFormat *ofmt = NULL;
	//Input AVFormatContext and Output AVFormatContext
	AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
	AVPacket pkt;
	const char *in_filename, *out_filename;
	int ret, i;
	int frame_index=0;
	in_filename  = (*pEnv)->GetStringUTFChars(pEnv, INPUTPATH, 0);//Input file URL
	out_filename = (*pEnv)->GetStringUTFChars(pEnv, OUTPUTMP4, 0);//Output file URL
	av_register_all();
	//Input
	if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
		LOGE( "Could not open input file.");
		goto end;
	}
	if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
		LOGE( "Failed to retrieve input stream information");
		goto end;
	}
	av_dump_format(ifmt_ctx, 0, in_filename, 0);
	//Output
	avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);
	if (!ofmt_ctx) {
		LOGE( "Could not create output context\n");
		ret = AVERROR_UNKNOWN;
		goto end;
	}
	ofmt = ofmt_ctx->oformat;
	for (i = 0; i < ifmt_ctx->nb_streams; i++) {
		//Create output AVStream according to input AVStream
		AVStream *in_stream = ifmt_ctx->streams[i];
		AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
		if (!out_stream) {
			LOGE( "Failed allocating output stream\n");
			ret = AVERROR_UNKNOWN;
			goto end;
		}
		//Copy the settings of AVCodecContext
		if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
			LOGE( "Failed to copy context from input to output stream codec context\n");
			goto end;
		}
		out_stream->codec->codec_tag = 0;
		if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
			out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}
	//Output information------------------
	av_dump_format(ofmt_ctx, 0, out_filename, 1);
	//Open output file
	if (!(ofmt->flags & AVFMT_NOFILE)) {
		ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
		if (ret < 0) {
			LOGE( "Could not open output file '%s'", out_filename);
			goto end;
		}
	}
	//Write file header
	if (avformat_write_header(ofmt_ctx, NULL) < 0) {
		LOGE( "Error occurred when opening output file\n");
		goto end;
	}

	AVRational newtimebase;
	int audio_lastpts = 0, audio_lastdts=0, video_lastpts = 0, video_lastdts=0;
	while (1) {
		AVStream *in_stream, *out_stream;
		//Get an AVPacket
		ret = av_read_frame(ifmt_ctx, &pkt);
		if (ret < 0)
			break;
		in_stream  = ifmt_ctx->streams[pkt.stream_index];
		out_stream = ofmt_ctx->streams[pkt.stream_index];

		newtimebase.den = out_stream->time_base.den;
		newtimebase.num = out_stream->time_base.num;
		//newtimebase.den *= speedratio;
		if(pkt.stream_index == 0)
		{
			int index = pkt.pos;

			//Convert PTS/DTS

			if(pkt.pts * av_q2d(in_stream->time_base) >= startTime && pkt.pts * av_q2d(in_stream->time_base) <= endTime)
			{

				pkt.pts = av_rescale_q_rnd(video_lastpts , in_stream->time_base, newtimebase, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
				pkt.dts = av_rescale_q_rnd(video_lastdts, in_stream->time_base, newtimebase, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
				video_lastdts += pkt.duration * speedratio;
				video_lastpts +=pkt.duration * speedratio;
				pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, newtimebase);
				pkt.pos = -1;
			}
			else
			{
				pkt.pts = av_rescale_q_rnd(video_lastpts, in_stream->time_base, newtimebase, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
				pkt.dts = av_rescale_q_rnd(video_lastdts, in_stream->time_base, newtimebase, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
				video_lastdts += pkt.duration;
				video_lastpts +=pkt.duration;
				pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, newtimebase);
				pkt.pos = -1;
			}
		}
		else
		{
			int index = pkt.pos;
			//Convert PTS/DTS
			if(pkt.pts * av_q2d(in_stream->time_base) >= startTime && pkt.pts * av_q2d(in_stream->time_base)<= endTime)
			{

				pkt.pts = av_rescale_q_rnd(audio_lastpts, in_stream->time_base, newtimebase, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
				pkt.dts = av_rescale_q_rnd(audio_lastdts, in_stream->time_base, newtimebase, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
				audio_lastdts += pkt.duration * speedratio;
				audio_lastpts +=pkt.duration * speedratio;
				pkt.duration = av_rescale_q(pkt.duration*speedratio, in_stream->time_base, newtimebase);
				pkt.pos = -1;
			}
			else
			{
				pkt.pts = av_rescale_q_rnd(audio_lastpts, in_stream->time_base, newtimebase, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
				pkt.dts = av_rescale_q_rnd(audio_lastdts, in_stream->time_base, newtimebase, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
				audio_lastdts += pkt.duration;
				audio_lastpts +=pkt.duration;
				pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, newtimebase);
				pkt.pos = -1;
			}

		}

		//Write
		if (av_interleaved_write_frame(ofmt_ctx, &pkt) < 0) {
			LOGE( "Error muxing packet\n");
			break;
		}


		//LOGE("Write %8d frames to output file\n",frame_index);
		av_free_packet(&pkt);
		frame_index++;
	}
	//Write file trailer
	av_write_trailer(ofmt_ctx);
end:
	avformat_close_input(&ifmt_ctx);
	/* close output */
	if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
		avio_close(ofmt_ctx->pb);
	avformat_free_context(ofmt_ctx);

	return 1;
}

void SaveFrameToYUV(AVFrame* frame_out,int width, int height, FILE *fp_out) {

	int y_size=width*height;
	fwrite(frame_out->data[0],1,y_size,fp_out);
	fwrite(frame_out->data[1],1,y_size/4,fp_out);
	fwrite(frame_out->data[2],1,y_size/4,fp_out);
//	//output Y,U,V
//    		if(frame_out->format==PIX_FMT_YUV420P)
//    		{
//    			for(int i=0;i<frame_out->height;i++){
//    				fwrite(frame_out->data[0]+frame_out->linesize[0]*i,1,frame_out->width,fp_out);
//    			}
//    			for(int i=0;i<frame_out->height/2;i++){
//    				fwrite(frame_out->data[1]+frame_out->linesize[1]*i,1,frame_out->width/2,fp_out);
//    			}
//    			for(int i=0;i<frame_out->height/2;i++){
//    				fwrite(frame_out->data[2]+frame_out->linesize[2]*i,1,frame_out->width/2,fp_out);
//    			}
//    		}
}

typedef struct
{
int64_t currentTimeOfGOP;
int iGop;
}GopInfo;

GopInfo *pGopInfo = NULL;
JNIEXPORT jint JNICALL Java_cn_poco_video_NativeUtils_getPreviousGOPtoYUVFILE(JNIEnv *pEnv, jobject pObj, jobject gopsize, jstring pFileName, jstring tempdicretory, jstring yuvOutput)
 {
	AVFormatContext *pFormatCtx = NULL;
	int             i, videoStream;
	AVCodecContext  *pCodecCtx = NULL;
	AVCodec         *pCodec = NULL;
	AVFrame         *pFrame = NULL;
	AVFrame         *pFrameYUV420 = NULL;
	AVPacket        packet;
	int             frameFinished;
	jobject			bitmap;
	void* 			buffer;

	AVDictionary    *optionsDict = NULL;
	struct SwsContext      *sws_ctx = NULL;
	char *videoFileName;

	// Register all formats and codecs
	av_register_all();

	//get C string from JNI jstring
	videoFileName = (char *)(*pEnv)->GetStringUTFChars(pEnv, pFileName, NULL);
    char* tempDicrectory = (char *)(*pEnv)->GetStringUTFChars(pEnv, tempdicretory, NULL);
    char* yuv_out_name = (char *)(*pEnv)->GetStringUTFChars(pEnv, yuvOutput, NULL);
	// Open video file
	if(avformat_open_input(&pFormatCtx, videoFileName, NULL, NULL)!=0)
	{
	    LOGE("文件不存在！");
		return -1; // Couldn't open file
    }
	// Retrieve stream information
	if(avformat_find_stream_info(pFormatCtx, NULL)<0)
	{
	    LOGE("查找流信息失败！");
		return -1; // Couldn't find stream information
    }
	if(pGopInfo == NULL)
	{
	    pGopInfo = (GopInfo*)malloc(sizeof(GopInfo));
	    pGopInfo->currentTimeOfGOP = pFormatCtx->duration; //时长;
	    pGopInfo->iGop = -1;
	}

	// Dump information about file onto standard error
	av_dump_format(pFormatCtx, 0, videoFileName, 0);

	// Find the first video stream
	videoStream=-1;
	for(i=0; i<pFormatCtx->nb_streams; i++) {
		if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) {
			videoStream=i;
			break;
		}
	}
	if(videoStream==-1)
	{
	    LOGE("找不到视频流！");
		return -1; // Didn't find a video stream
    }
	// Get a pointer to the codec context for the video stream
	pCodecCtx=pFormatCtx->streams[videoStream]->codec;

	// Find the decoder for the video stream
	pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
	if(pCodec==NULL) {
		LOGE(stderr, "Unsupported codec!\n");
		return -1; // Codec not found
	}
	// Open codec
	if(avcodec_open2(pCodecCtx, pCodec, &optionsDict)<0)
		return -1; // Could not open codec

	// Allocate video frame
	pFrame=avcodec_alloc_frame();

	// Allocate an AVFrame structure
	pFrameYUV420=avcodec_alloc_frame();
	if(pFrameYUV420==NULL)
		return -1;

	//create a bitmap as the buffer for pFrameYUV420
	buffer = av_malloc(avpicture_get_size(PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height));
	//get the scaling context
	sws_ctx = sws_getContext
    (
        pCodecCtx->width,
        pCodecCtx->height,
        pCodecCtx->pix_fmt,
        pCodecCtx->width,
        pCodecCtx->height,
        PIX_FMT_YUV420P,
        SWS_BILINEAR,
        NULL,
        NULL,
        NULL
    );

	// Assign appropriate parts of bitmap to image planes in pFrameYUV420
	// Note that pFrameYUV420 is an AVFrame, but AVFrame is a superset
	// of AVPicture
	avpicture_fill((AVPicture *)pFrameYUV420, buffer, PIX_FMT_YUV420P,
		 pCodecCtx->width, pCodecCtx->height);

	// Read frames and save first five frames to disk
	i=0;
	char szFilename[200];
    sprintf(szFilename, "%s%s", tempDicrectory, yuv_out_name);
    //Output YUV
    FILE *fp_out=fopen(szFilename,"wb+");
    if(fp_out==NULL){
    	LOGE("Error open output file.\n");
    	return -1;
    }

    int ret = 0;
   int64_t SeekTime = pGopInfo->currentTimeOfGOP;// *　AV_TIME_BASE;
   LOGE("seektime = %lld", SeekTime);
    //跳帧到指定时间附近读帧
    av_seek_frame(pFormatCtx,
                      videoStream,
                      SeekTime,
                      AVSEEK_FLAG_BACKWARD) ;//AVSEEK_FLAG_ANY AVSEEK_FLAG_BACKWARD
   // avcodec_flush_buffers(pFormatCtx->streams[videoStream]->codec);
    pGopInfo->currentTimeOfGOP = -1;
    int skipped_frame = 0;
	while(1)
	{
	    if((ret = av_read_frame(pFormatCtx, &packet)) < 0)
	    {

	            if(skipped_frame>0)
	            {
	                LOGE("解析剩余帧");
                    for(; skipped_frame>0; skipped_frame--)  //解码packet中剩下的之前解码失败的帧
                    {
                        int got_frame = 0;
                        ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_frame, &packet);
                        if (got_frame) {
                           // packet.pts = av_rescale_q(packet.pts,video_dec_st->time_base,video_enc_st->time_base);
                           pFrame->pts = av_frame_get_best_effort_timestamp(pFrame);

                           if(pGopInfo->currentTimeOfGOP == -1)
                           {
                                 pGopInfo->currentTimeOfGOP = (pFrame->pts * av_q2d(pFormatCtx->streams[videoStream]->time_base)) * AV_TIME_BASE - 1 ;
                                  LOGE("pts = %lld", pGopInfo->currentTimeOfGOP);
                           }
                            sws_scale(sws_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameYUV420->data, pFrameYUV420->linesize);  //转为为标准420P的YUV格式
         				// Save the frame to disk
         				    SaveFrameToYUV(pFrameYUV420, pCodecCtx->width, pCodecCtx->height, fp_out);

//         				   if(++i>=pCodecCtx->gop_size)
//                          {
//                             av_free_packet(&packet);
//                             skipped_frame = 0;
//                             break;
//                           }
                         LOGI("save frame %d", skipped_frame);
                        }
                    }
                    break;
                }
                else
                    break;
	    }
		// Is this a packet from the video stream
		if(packet.stream_index==videoStream) {
			// Decode video frame
			LOGE("packet flag = %d", packet.flags & AV_PKT_FLAG_KEY);
			avcodec_get_frame_defaults(pFrame);
			avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished,
			   &packet);
			// Did we get a video frame?
			if(frameFinished) {
				// Convert the image from its native format to RGBA
                pFrame->pts = av_frame_get_best_effort_timestamp(pFrame);
                if(pGopInfo->currentTimeOfGOP == -1)
                {
                      pGopInfo->currentTimeOfGOP = (pFrame->pts * av_q2d(pFormatCtx->streams[videoStream]->time_base)) * AV_TIME_BASE - 1 ;
                      LOGE("pts = %lld", pGopInfo->currentTimeOfGOP);
                }
				sws_scale
				(
					sws_ctx,
					(uint8_t const * const *)pFrame->data,
					pFrame->linesize,
					0,
					pCodecCtx->height,
					pFrameYUV420->data,
					pFrameYUV420->linesize
				);

				// Save the frame to disk
				SaveFrameToYUV(pFrameYUV420, pCodecCtx->width, pCodecCtx->height, fp_out);
                LOGI("save frame %d", i);
				if(++i>=pCodecCtx->gop_size)
                {
                    av_free_packet(&packet);
                    break;
                }

			}
			else
			    skipped_frame++;
		}
        //把帧数复制给pNUM参数返回给JAVA端
		// Free the packet that was allocated by av_read_frame
		av_free_packet(&packet);
	}
    jclass intclz = (*pEnv)->FindClass(pEnv, "java/lang/Integer");
    jfieldID id = (*pEnv)->GetFieldID(pEnv, intclz, "value", "I");
    (*pEnv)->SetIntField(pEnv, gopsize, id, pCodecCtx->gop_size);

    //close output file
    fclose(fp_out);
    LOGE("file close");
	// Free the NV12 image
	av_free(pFrameYUV420);

    //Free buffer
    free(buffer);

	// Free the YUV frame
	av_free(pFrame);

	// Close the codec
	avcodec_close(pCodecCtx);

	// Close the video file
	avformat_close_input(&pFormatCtx);

	return 0;
}

int main(int argc, char *argv[])
{
 //   addPNGToMp4("/sdcard/android-ffmpeg-tutorial01/.test/hand.mp4", "/sdcard/android-ffmpeg-tutorial01/.test/testtest.mp4", "/sdcard/android-ffmpeg-tutorial01/.test/test.png");
  //  decodeVideo("/sdcard/android-ffmpeg-tutorial01/.test/hand.mp4", "/sdcard/android-ffmpeg-tutorial01/.test/test.yuv");
 //   decodeVideo("/sdcard/android-ffmpeg-tutorial01/.test/quickhand.mp4");
 //   selectFrames(0,pframeContainer->framenum[0],0);
 //   selectFrames(0,pframeContainer->framenum[1],1);
 //   startEncode();

 //   playFramesContain(pframeContainer2); //播放container*/
    return 0;
}