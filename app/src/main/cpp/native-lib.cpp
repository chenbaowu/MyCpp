#include <jni.h>
#include <string>
#include <stdio.h>
#include <wchar.h>
#include <Android/bitmap.h>
#include <android/log.h>

extern "C"
JNIEXPORT jstring JNICALL
Java_cn_poco_mycpp_VideoNativeUtils_stringFromJNI(JNIEnv *env, jobject /* this */) {
    std::string hello = " cbw to write cpp ";
    return env->NewStringUTF(hello.c_str());
}

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfiltergraph.h>
#include <libavfilter/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
};

#define LOG_TAG "android-ffmpeg-video"
#define LOGI(...) __android_log_print(4, LOG_TAG, __VA_ARGS__);
#define LOGE(...) __android_log_print(6, LOG_TAG, __VA_ARGS__);


// 视屏解前iFrame帧生成bitmap保存文件到指定文件夹

void SaveFrame(JNIEnv *pEnv, jobject pObj, jobject pBitmap, int width, int height, int iFrame) {
    char szFilename[200];
    jmethodID sSaveFrameMID;
    jclass mainActCls;
    sprintf(szFilename, "/sdcard/cbw/frame%d.jpg", iFrame);
    mainActCls = (pEnv)->GetObjectClass(pObj);
    sSaveFrameMID = (pEnv)->GetMethodID(mainActCls, "saveFrameToPath",
                                        "(Landroid/graphics/Bitmap;Ljava/lang/String;)V");
    LOGI("call java method to save frame %d", iFrame);
    jstring filePath = (pEnv)->NewStringUTF(szFilename);
    (pEnv)->CallVoidMethod(pObj, sSaveFrameMID, pBitmap, filePath);
    LOGI("call java method to save frame %d done", iFrame);
}

jobject createBitmap(JNIEnv *pEnv, int pWidth, int pHeight) {
    int i;
    //get Bitmap class and createBitmap method ID
    jclass javaBitmapClass = (jclass) (pEnv)->FindClass("android/graphics/Bitmap");
    jmethodID mid = (pEnv)->GetStaticMethodID(javaBitmapClass, "createBitmap",
                                              "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
    //create Bitmap.Config
    //reference: https://forums.oracle.com/thread/1548728
    const wchar_t *configName = L"ARGB_8888";
    int len = wcslen(configName);
    jstring jConfigName;
    if (sizeof(wchar_t) != sizeof(jchar)) {
        //wchar_t is defined as different length than jchar(2 bytes)
        jchar *str = (jchar *) malloc((len + 1) * sizeof(jchar));
        for (i = 0; i < len; ++i) {
            str[i] = (jchar) configName[i];
        }
        str[len] = 0;
        jConfigName = (pEnv)->NewString((const jchar *) str, len);
    } else {
        //wchar_t is defined same length as jchar(2 bytes)
        jConfigName = (pEnv)->NewString((const jchar *) configName, len);
    }
    jclass bitmapConfigClass = (pEnv)->FindClass("android/graphics/Bitmap$Config");
    jobject javaBitmapConfig = (pEnv)->CallStaticObjectMethod(bitmapConfigClass,
                                                              (pEnv)->GetStaticMethodID(
                                                                      bitmapConfigClass, "valueOf",
                                                                      "(Ljava/lang/String;)Landroid/graphics/Bitmap$Config;"),
                                                              jConfigName);
    //create the bitmap
    return (pEnv)->CallStaticObjectMethod(javaBitmapClass, mid, pWidth, pHeight, javaBitmapConfig);
}


extern "C"
JNIEXPORT jint JNICALL
Java_cn_poco_mycpp_VideoNativeUtils_SaveFrameToBitmap(JNIEnv *pEnv, jobject pObj, jobject pMainAct,
                                                      jstring pFileName, jint pNumOfFrames) {
    AVFormatContext *pFormatCtx = NULL;
    int i, videoStream;
    AVCodecContext *pCodecCtx = NULL;
    AVCodec *pCodec = NULL;
    AVFrame *pFrame = NULL;
    AVFrame *pFrameRGBA = NULL;
    AVPacket packet;
    int frameFinished;
    jobject bitmap;
    void *buffer;

    AVDictionary *optionsDict = NULL;
    struct SwsContext *sws_ctx = NULL;
    char *videoFileName;

    // Register all formats and codecs
    av_register_all();

    //get C string from JNI jstring
    videoFileName = (char *) (pEnv)->GetStringUTFChars(pFileName, NULL);

    char *buf = new char(1024);
    int ret = 0;
    // Open video file
    if ((ret = avformat_open_input(&pFormatCtx, videoFileName, NULL, NULL)) != 0) {
        LOGE("Couldn't open file %s  error code:%d", videoFileName, ret);
        av_strerror(ret, buf, 1024);
        LOGE("%s", buf);
        return -1; // Couldn't open file
    }
    // Retrieve stream information
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        LOGE("Couldn't find stream information");
        return -1; // Couldn't find stream information
    }
    // Dump information about file onto standard error
    av_dump_format(pFormatCtx, 0, videoFileName, 0);

    // Find the first video stream
    videoStream = -1;
    for (i = 0; i < pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStream = i;
            break;
        }
    }
    if (videoStream == -1)
        return -1; // Didn't find a video stream

    // Get a pointer to the codec context for the video stream
    pCodecCtx = pFormatCtx->streams[videoStream]->codec;

    // Find the decoder for the video stream
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if (pCodec == NULL) {
        LOGE("Unsupported codec!\n");
        return -1; // Codec not found
    }
    // Open codec
    if (avcodec_open2(pCodecCtx, pCodec, &optionsDict) < 0) {
        LOGE("Could not open codec");
        return -1; // Could not open codec
    }
    // Allocate video frame
    pFrame = avcodec_alloc_frame();

    // Allocate an AVFrame structure
    pFrameRGBA = avcodec_alloc_frame();
    if (pFrameRGBA == NULL)
        return -1;

    //create a bitmap as the buffer for pFrameRGBA
    bitmap = createBitmap(pEnv, pCodecCtx->width, pCodecCtx->height);
    if (AndroidBitmap_lockPixels(pEnv, bitmap, &buffer) < 0) {
        LOGE("AndroidBitmap_lockPixels failed");
        return -1;
    }
    //get the scaling context
    sws_ctx = sws_getContext
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

    // Assign appropriate parts of bitmap to image planes in pFrameRGBA
    // Note that pFrameRGBA is an AVFrame, but AVFrame is a superset
    // of AVPicture
    avpicture_fill((AVPicture *) pFrameRGBA, (uint8_t *) buffer, AV_PIX_FMT_RGBA,
                   pCodecCtx->width, pCodecCtx->height);

    // Read frames and save first five frames to disk
    i = 0;
    while (av_read_frame(pFormatCtx, &packet) >= 0) {
        // Is this a packet from the video stream?
        if (packet.stream_index == videoStream) {
            // Decode video frame
            avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished,
                                  &packet);
            // Did we get a video frame?
            if (frameFinished) {
                // Convert the image from its native format to RGBA
                sws_scale
                        (
                                sws_ctx,
                                (uint8_t const *const *) pFrame->data,
                                pFrame->linesize,
                                0,
                                pCodecCtx->height,
                                pFrameRGBA->data,
                                pFrameRGBA->linesize
                        );

                // Save the frame to disk
                if (++i <= pNumOfFrames) {
                    SaveFrame(pEnv, pMainAct, bitmap, pCodecCtx->width, pCodecCtx->height, i);
                    LOGI("save frame %d", i);
                }
            }
        }
        // Free the packet that was allocated by av_read_frame
        av_free_packet(&packet);
    }

    //unlock the bitmap
    AndroidBitmap_unlockPixels(pEnv, bitmap);

    // Free the RGB image
    av_free(pFrameRGBA);

    // Free the YUV frame
    av_free(pFrame);

    // Close the codec
    avcodec_close(pCodecCtx);

    // Close the video file
    avformat_close_input(&pFormatCtx);

    return 0;
}



// 视频水印

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


static AVFormatContext *pFormatCtx = NULL;
static AVCodecContext *pCodecCtx = NULL;  //视频解码上下文
static AVCodecContext *pCodecCtx2 = NULL;  //音频解码上下文
AVFilterContext *buffersink_ctx = NULL;
AVFilterContext *buffersrc_ctx = NULL;
AVFilterGraph *filter_graph = NULL;
static int video_stream_index = -1;
static int audio_stream_index = -1;
const char *filter_descr = "movie=/sdcard/android-ffmpeg-tutorial01/test.png[wm];[in][wm]overlay=5:5[out]";
//const char *filter_descr = "movie=my_logo.png[wm];[in][wm]overlay=5:5[out]";


/////////////////////////////////////////////////////////////////////h264encode.c///////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct {
    AVFormatContext *pFormatCtx;
    AVOutputFormat *fmt;
    AVStream *video_st;
    AVCodecContext *pCodecCtx;
    AVCodec *pCodec;
    AVPacket pkt;
    uint8_t *picture_buf;
    AVFrame *pFrame;
    int picture_size;
    int y_size;
    int framecnt;
} encodeh264bean, *Encodeh264BeanPtr;

int initEncodeh264Bean(Encodeh264BeanPtr pbean, const char *out_file) {
    pbean->framecnt = 0;

    //Method1.
    pbean->pFormatCtx = avformat_alloc_context();
    //Guess Format
    pbean->fmt = av_guess_format(NULL, out_file, NULL);
    pbean->pFormatCtx->oformat = pbean->fmt;

    //Method 2.
    //avformat_alloc_output_context2(&pFormatCtx, NULL, NULL, out_file);
    //fmt = pFormatCtx->oformat;


    //Open output URL
    if (avio_open(&pbean->pFormatCtx->pb, out_file, AVIO_FLAG_READ_WRITE) < 0) {
        LOGE("Failed to open output file! \n");
        return -1;
    }

    pbean->video_st = avformat_new_stream(pbean->pFormatCtx, 0);
//	pbean->video_st->time_base.num = pCodecCtx->time_base.num;
//	pbean->video_st->time_base.den = pCodecCtx->time_base.den;

    if (pbean->video_st == NULL) {
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
    pbean->pCodecCtx->time_base.den = pFormatCtx->streams[video_stream_index]->avg_frame_rate.num /
                                      pFormatCtx->streams[video_stream_index]->avg_frame_rate.den;
    pbean->pCodecCtx->bit_rate = pCodecCtx->bit_rate;
    pbean->pCodecCtx->gop_size = 3;
    //H264
    pbean->pCodecCtx->me_range = 16;
    pCodecCtx->max_qdiff = 4;
    //pCodecCtx->qcompress = 0.6;
    pbean->pCodecCtx->qmin = 10;
    pbean->pCodecCtx->qmax = 51;

    //Optional Param
    pbean->pCodecCtx->max_b_frames = 0;

    // Set Option
    AVDictionary *param = 0;
    //H.264
    if (pbean->pCodecCtx->codec_id == AV_CODEC_ID_H264) {
        av_dict_set(&param, "preset", "ultrafast", 0);
        av_dict_set(&param, "tune", "zerolatency", 0);
        //av_dict_set(&param, "profile", "main", 0);
    }
    //H.265
    if (pbean->pCodecCtx->codec_id == AV_CODEC_ID_H265) {
        av_dict_set(&param, "preset", "ultrafast", 0);
        av_dict_set(&param, "tune", "zero-latency", 0);
    }

    //Show some Information
    av_dump_format(pbean->pFormatCtx, 0, out_file, 1);

    pbean->pCodec = avcodec_find_encoder(pbean->pCodecCtx->codec_id);
    if (!pbean->pCodec) {
        LOGE("Can not find encoder! \n");
        return -1;
    }
    if (avcodec_open2(pbean->pCodecCtx, pbean->pCodec, &param) < 0) {
        LOGE("Failed to open encoder! \n");
        return -1;
    }

    pbean->pFrame = av_frame_alloc();
    pbean->picture_size = avpicture_get_size(pbean->pCodecCtx->pix_fmt, pbean->pCodecCtx->width,
                                             pbean->pCodecCtx->height);
    pbean->picture_buf = (uint8_t *) av_malloc(pbean->picture_size);
    avpicture_fill((AVPicture *) (pbean->pFrame), pbean->picture_buf, pbean->pCodecCtx->pix_fmt,
                   pbean->pCodecCtx->width, pbean->pCodecCtx->height);

    //Write File Header
    avformat_write_header(pbean->pFormatCtx, NULL);

    //av_new_packet(&pbean->pkt,pbean->picture_size);

    pbean->y_size = pbean->pCodecCtx->width * pbean->pCodecCtx->height;
    return 0;
}

int flush_encoder2(AVFormatContext *fmt_ctx, unsigned int stream_index) {
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
        ret = avcodec_encode_video2(fmt_ctx->streams[stream_index]->codec, &enc_pkt,
                                    NULL, &got_frame);
        av_frame_free(NULL);
        if (ret < 0)
            break;
        if (!got_frame) {
            ret = 0;
            break;
        }
        LOGE("Flush Encoder: Succeed to encode 1 frame!\tsize:%5d\n", enc_pkt.size);
        /* mux encoded frame */
        ret = av_write_frame(fmt_ctx, &enc_pkt);
        if (ret < 0)
            break;
    }
    return ret;
}

int encodeH264(Encodeh264BeanPtr pbean, AVFrame *picref) {
    pbean->pFrame->data[0] = picref->data[0];              // Y
    pbean->pFrame->data[1] = picref->data[1];
    pbean->pFrame->data[2] = picref->data[2];

    int got_picture = 0;
    //Encode
    av_new_packet(&pbean->pkt, pbean->picture_size);
    int ret = avcodec_encode_video2(pbean->pCodecCtx, &pbean->pkt, pbean->pFrame, &got_picture);
    if (ret < 0) {
        LOGE("Failed to encode! \n");
        return -1;
    }
    if (got_picture == 1) {
//		LOGE("Succeed to encode frame: %5d\tsize:%5d\n",pbean->framecnt,pbean->pkt.size);
        pbean->framecnt++;
        pbean->pkt.stream_index = pbean->video_st->index;
        ret = av_write_frame(pbean->pFormatCtx, &pbean->pkt);
        av_free_packet(&pbean->pkt);
    }

    pbean->pFrame->pts++;
    return 0;
}
/////////////////////////////////////////////////###########h264encode.c############////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////AACencode.c////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int getAACFromVideo(const char *in_filename, const char *out_filename_a) {
    AVOutputFormat *ofmt_a = NULL;
    //（Input AVFormatContext and Output AVFormatContext）
    AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx_a = NULL;
    AVPacket pkt;
    int ret, i;
    int audioindex = -1;
    int frame_index = 0;

//	const char *in_filename  = "cuc_ieschool.flv";//Input file URL
    //char *in_filename  = "cuc_ieschool.mkv";
    //char *out_filename_a = "cuc_ieschool.mp3";
//	const char *out_filename_a = "cuc_ieschool.aac";

    av_register_all();
    //Input
    if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
        LOGE("Could not open input file.");
        goto end;
    }
    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        LOGE("Failed to retrieve input stream information");
        goto end;
    }


    avformat_alloc_output_context2(&ofmt_ctx_a, NULL, NULL, out_filename_a);
    if (!ofmt_ctx_a) {
        LOGE("Could not create output context\n");
        ret = AVERROR_UNKNOWN;
        goto end;
    }
    ofmt_a = ofmt_ctx_a->oformat;

    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        //Create output AVStream according to input AVStream
        AVFormatContext *ofmt_ctx;
        AVStream *in_stream = ifmt_ctx->streams[i];
        AVStream *out_stream = NULL;


        if (ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioindex = i;
            out_stream = avformat_new_stream(ofmt_ctx_a, in_stream->codec->codec);
            ofmt_ctx = ofmt_ctx_a;
        } else
            continue;

        if (!out_stream) {
            LOGE("Failed allocating output stream\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }
        //Copy the settings of AVCodecContext
        if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
            //          LOGE( "Failed to copy context from input to output stream codec context\n");
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
            LOGE("Could not open output file '%s'", out_filename_a);
            goto end;
        }
    }

    //Write file header
    if (avformat_write_header(ofmt_ctx_a, NULL) < 0) {
        LOGE("Error occurred when opening audio output file\n");
        goto end;
    }


    while (1) {
        AVFormatContext *ofmt_ctx;
        AVStream *in_stream, *out_stream;
        //Get an AVPacket
        if (av_read_frame(ifmt_ctx, &pkt) < 0)
            break;
        in_stream = ifmt_ctx->streams[pkt.stream_index];


        if (pkt.stream_index == audioindex) {
            out_stream = ofmt_ctx_a->streams[0];
            ofmt_ctx = ofmt_ctx_a;
            LOGE("Write Audio Packet. size:%d\tpts:%lld\n", pkt.size, pkt.pts);
        } else {
            continue;
        }


        //Convert PTS/DTS
        pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base,
                                   (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base,
                                   (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;
        pkt.stream_index = 0;
        //Write
        if (av_interleaved_write_frame(ofmt_ctx, &pkt) < 0) {
            LOGE("Error muxing packet\n");
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
        LOGE("Error occurred.\n");
        return -1;
    }
    return 0;
}
//////////////////////////////////////////#######AACencode.c#########///////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////MuxerMp4.c//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int muxerMp4(const char *in_filename_v, const char *in_filename_a, const char *out_filename) {
    AVOutputFormat *ofmt = NULL;
#if USE_AACBSF
    AVBitStreamFilterContext *aacbsfc;
#endif
    int haveaacdata = 1;
    //Input AVFormatContext and Output AVFormatContext
    AVFormatContext *ifmt_ctx_v = NULL, *ifmt_ctx_a = NULL, *ofmt_ctx = NULL;
    AVPacket pkt;
    int ret, i;
    int videoindex_v = -1, videoindex_out = -1;
    int audioindex_a = -1, audioindex_out = -1;
    int frame_index = 0;
    int64_t cur_pts_v = 0, cur_pts_a = 0;

    av_register_all();
    //Input
    if ((ret = avformat_open_input(&ifmt_ctx_v, in_filename_v, 0, 0)) < 0) {
        printf("Could not open input video file.");
        goto end;
    }
    if ((ret = avformat_find_stream_info(ifmt_ctx_v, 0)) < 0) {
        printf("Failed to retrieve input stream information");
        goto end;
    }
    if ((ret = avformat_open_input(&ifmt_ctx_a, in_filename_a, 0, 0)) < 0) {
        LOGE("Could not open input audio file.");
        haveaacdata = 0;
        ret = 0;
//        goto end;
    }
    if (haveaacdata == 1)
        if ((ret = avformat_find_stream_info(ifmt_ctx_a, 0)) < 0) {//如果找不到音频流，则不写入音频
            LOGE("Failed to retrieve input stream information");
            haveaacdata = 0;
            ret = 0;
//		    goto end;
        }
    LOGE("===========Input Information==========\n");
//    av_dump_format(ifmt_ctx_v, 0, in_filename_v, 0);
//    av_dump_format(ifmt_ctx_a, 0, in_filename_a, 0);
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
        if (ifmt_ctx_v->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            AVStream *in_stream = ifmt_ctx_v->streams[i];
            AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
            videoindex_v = i;
            if (!out_stream) {
                LOGE("Failed allocating output stream\n");
                ret = AVERROR_UNKNOWN;
                goto end;
            }
            videoindex_out = out_stream->index;
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
    if (haveaacdata == 1)
        for (i = 0; i < ifmt_ctx_a->nb_streams; i++) {
            //Create output AVStream according to input AVStream
            if (ifmt_ctx_a->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
                AVStream *in_stream = ifmt_ctx_a->streams[i];
                AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
                audioindex_a = i;
                if (!out_stream) {
                    LOGE("Failed allocating output stream\n");
                    ret = AVERROR_UNKNOWN;
                    goto end;
                }
                audioindex_out = out_stream->index;
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
    aacbsfc = av_bitstream_filter_init("aac_adtstoasc");
#endif
    while (1) {
        AVFormatContext *ifmt_ctx;
        int stream_index = 0;
        AVStream *in_stream, *out_stream;

        if (audioindex_a == -1 || haveaacdata == 0) //如果没有音频直接写入视频
        {
            ifmt_ctx = ifmt_ctx_v;
            stream_index = videoindex_out;

            if (av_read_frame(ifmt_ctx, &pkt) >= 0) {
                do {
                    in_stream = ifmt_ctx->streams[pkt.stream_index];
                    out_stream = ofmt_ctx->streams[stream_index];

                    if (pkt.stream_index == videoindex_v) {
                        //FIX：No PTS (Example: Raw H.264)
                        //Simple Write PTS
                        if (pkt.pts == AV_NOPTS_VALUE) {
                            //Write PTS
                            AVRational time_base1 = in_stream->time_base;
                            //Duration between 2 frames (us)
                            int64_t calc_duration =
                                    (double) AV_TIME_BASE / av_q2d(in_stream->r_frame_rate);
                            //Parameters
                            pkt.pts = (double) (frame_index * calc_duration) /
                                      (double) (av_q2d(time_base1) * AV_TIME_BASE);
                            pkt.dts = pkt.pts;
                            pkt.duration = (double) calc_duration /
                                           (double) (av_q2d(time_base1) * AV_TIME_BASE);
                            frame_index++;
                        }

                        cur_pts_v = pkt.pts;
                        break;
                    }
                } while (av_read_frame(ifmt_ctx, &pkt) >= 0);
            } else {
                break;
            }
        }
            //Get an AVPacket
        else if (av_compare_ts(cur_pts_v, ifmt_ctx_v->streams[videoindex_v]->time_base, cur_pts_a,
                               ifmt_ctx_a->streams[audioindex_a]->time_base) <= 0) {
            ifmt_ctx = ifmt_ctx_v;
            stream_index = videoindex_out;
            if (av_read_frame(ifmt_ctx, &pkt) >= 0) {
                do {
                    in_stream = ifmt_ctx->streams[pkt.stream_index];
                    out_stream = ofmt_ctx->streams[stream_index];

                    if (pkt.stream_index == videoindex_v) {
                        //FIX：No PTS (Example: Raw H.264)
                        //Simple Write PTS
                        if (pkt.pts == AV_NOPTS_VALUE) {
                            //Write PTS
                            AVRational time_base1 = in_stream->time_base;
                            //Duration between 2 frames (us)
                            int64_t calc_duration =
                                    (double) AV_TIME_BASE / av_q2d(in_stream->r_frame_rate);
                            //Parameters
                            pkt.pts = (double) (frame_index * calc_duration) /
                                      (double) (av_q2d(time_base1) * AV_TIME_BASE);
                            pkt.dts = pkt.pts;
                            pkt.duration = (double) calc_duration /
                                           (double) (av_q2d(time_base1) * AV_TIME_BASE);
                            frame_index++;
                        }

                        cur_pts_v = pkt.pts;
                        break;
                    }
                } while (av_read_frame(ifmt_ctx, &pkt) >= 0);
            } else {
                break;
            }
        } else {
            ifmt_ctx = ifmt_ctx_a;
            stream_index = audioindex_out;
            if (av_read_frame(ifmt_ctx, &pkt) >= 0) {
                do {
                    in_stream = ifmt_ctx->streams[pkt.stream_index];
                    out_stream = ofmt_ctx->streams[stream_index];

                    if (pkt.stream_index == audioindex_a) {

                        //FIX：No PTS
                        //Simple Write PTS
                        if (pkt.pts == AV_NOPTS_VALUE) {
                            //Write PTS
                            AVRational time_base1 = in_stream->time_base;
                            //Duration between 2 frames (us)
                            int64_t calc_duration =
                                    (double) AV_TIME_BASE / av_q2d(in_stream->r_frame_rate);
                            //Parameters
                            pkt.pts = (double) (frame_index * calc_duration) /
                                      (double) (av_q2d(time_base1) * AV_TIME_BASE);
                            pkt.dts = pkt.pts;
                            pkt.duration = (double) calc_duration /
                                           (double) (av_q2d(time_base1) * AV_TIME_BASE);
                            frame_index++;
                        }
                        cur_pts_a = pkt.pts;

                        break;
                    }
                } while (av_read_frame(ifmt_ctx, &pkt) >= 0);
            } else {
                break;
            }

        }
        //FIX:Bitstream Filter
#if USE_H264BSF
        av_bitstream_filter_filter(h264bsfc, in_stream->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);
#endif
#if USE_AACBSF
        av_bitstream_filter_filter(aacbsfc, out_stream->codec, NULL, &pkt.data, &pkt.size, pkt.data,
                                   pkt.size, 0);
#endif

        //Convert PTS/DTS
        pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base,
                                   (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base,
                                   (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;
        pkt.stream_index = stream_index;

        //LOGE("Write 1 Packet. size:%5d\tpts:%lld\n",pkt.size,pkt.pts);
        //Write
        if (av_interleaved_write_frame(ofmt_ctx, &pkt) < 0) {
            LOGE("Error muxing packet\n");
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
////////////////////////////////////////////##########mexuermp4###########///////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////开始编写添加水印代码////////////////////////////////////////////////////////////////////////////////////
static int open_input_file(const char *filename) {
    int ret;
    AVCodec *dec, *dec2;

    if ((ret = avformat_open_input(&pFormatCtx, filename, NULL, NULL)) < 0) {
        LOGE("Cannot open input file\n");
        return ret;
    }

    if ((ret = avformat_find_stream_info(pFormatCtx, NULL)) < 0) {
        LOGE("Cannot find stream information\n");
        return ret;
    }

    /* select the video stream */
    ret = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0);
    if (ret < 0) {
        LOGE("Cannot find a video stream in the input file, may be input file is not invild, now set video stream to 0");
        ret = 0;
        //       return ret;
    }
    video_stream_index = ret;
    pCodecCtx = pFormatCtx->streams[video_stream_index]->codec;

//select the audio stream
    ret = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, &dec2, 0);
    if (ret < 0) {
        LOGE("Cannot find a audio stream in the input file , may be input file is not invild, now set audio stream to 0\n");
//        return ret;
        ret = 0;
    }
    audio_stream_index = ret;
    pCodecCtx2 = pFormatCtx->streams[audio_stream_index]->codec;
    /* init the video decoder */
    if ((ret = avcodec_open2(pCodecCtx, dec, NULL)) < 0) {
        LOGE("Cannot open video decoder\n");
        return ret;
    }
    /* init the audio decoder */
    if ((ret = avcodec_open2(pCodecCtx2, dec2, NULL)) < 0) {
        LOGE("Cannot open audio decoder\n");
        return ret;
    }
    return 0;
}

static int init_filters(const char *filters_descr) {
    char args[512];
    int ret;
    AVFilter *buffersrc = avfilter_get_by_name("buffer");
    AVFilter *buffersink = avfilter_get_by_name("ffbuffersink");
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs = avfilter_inout_alloc();
    enum PixelFormat pix_fmts[] = {PIX_FMT_YUV420P, PIX_FMT_NONE};
    AVBufferSinkParams *buffersink_params;

    filter_graph = avfilter_graph_alloc();

    /* buffer video source: the decoded frames from the decoder will be inserted here. */
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
             pCodecCtx->time_base.num, pCodecCtx->time_base.den,
             pCodecCtx->sample_aspect_ratio.num, pCodecCtx->sample_aspect_ratio.den);

    ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                                       args, NULL, filter_graph);
    if (ret < 0) {
        LOGE("Cannot create buffer source\n");
        return ret;
    }

    /* buffer video sink: to terminate the filter chain. */
    buffersink_params = av_buffersink_params_alloc();
    buffersink_params->pixel_fmts = pix_fmts;
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                       NULL, buffersink_params, filter_graph);
    av_free(buffersink_params);
    if (ret < 0) {
        LOGE("Cannot create buffer sink\n");
        return ret;
    }

    /* Endpoints for the filter graph. */
    outputs->name = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = NULL;

    inputs->name = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = NULL;

    if ((ret = avfilter_graph_parse_ptr(filter_graph, filters_descr,
                                        &inputs, &outputs, NULL)) < 0)
        return ret;

    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
        return ret;
    return 0;
}

int addPNGToMp4(const char *in_vFilename, const char *temp_path, const char *out_vFilename,
                const char *png_Filename) {
    int ret;
    AVPacket packet;
    AVFrame frame;
    int got_frame;
    int skipped_frame;
    uint8_t *out_buffer;
    Encodeh264BeanPtr pbean;
    int i = 0;
    char *temph264 = "/temp.h264";
    char *h264path = (char *) malloc(strlen(temp_path) + strlen(temph264) + 1);
    strcpy(h264path, temp_path);
    strcat(h264path, "/temp.h264");

    char *tempaac = "/temp.aac";
    char *aacpath = (char *) malloc(strlen(temp_path) + strlen(tempaac) + 1);
    strcpy(aacpath, temp_path);
    strcat(aacpath, "/temp.aac");

    av_register_all();
    avfilter_register_all();

//	LOGE("h264path=%s", h264path);
//	LOGE("aacpath=%s", aacpath);
//	LOGE("outfilepath=%s", out_vFilename);
    char args[512];
    snprintf(args, sizeof(args),
             "movie=%s[wm];[in][wm]overlay=5:5[out]", //"video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             png_Filename);

//    char *out_videofilename = (*pEnv)->GetStringUTFChars(pEnv, pFileName, 0);
    if ((ret = open_input_file(in_vFilename)) < 0)
        goto end;
    if ((ret = init_filters(args)) < 0)
        goto end;

    ///初始化H264编码器
    pbean = (Encodeh264BeanPtr) malloc(sizeof(encodeh264bean));
    initEncodeh264Bean(pbean, h264path);

    AVFrame *pFrameYUV;
    pFrameYUV = avcodec_alloc_frame();
    struct SwsContext *img_convert_ctx;
    img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
                                     pCodecCtx->width, pCodecCtx->height, PIX_FMT_YUV420P,
                                     SWS_BICUBIC, NULL, NULL, NULL);
    out_buffer = (uint8_t *) av_malloc(
            avpicture_get_size(PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height));
    /* read all packets */

    skipped_frame = 0;
    while (1) {
        AVFilterBufferRef *picref = NULL;

        ret = av_read_frame(pFormatCtx, &packet);
        if (ret < 0)  //PACKET读取完毕后   还要读取残留在PACKET的帧
        {
            break;
        }

        if (packet.stream_index == video_stream_index) {
            avcodec_get_frame_defaults(&frame);
            got_frame = 0;
            ret = avcodec_decode_video2(pCodecCtx, &frame, &got_frame, &packet);
            if (ret < 0) {
                LOGE("Error decoding video\n");
                break;
            }

            if (got_frame) {
                frame.pts = av_frame_get_best_effort_timestamp(&frame);

                /* push the decoded frame into the filtergraph */
                if (av_buffersrc_add_frame(buffersrc_ctx, &frame) < 0) {
                    LOGE("Error while feeding the filtergraph\n");
                    break;
                }

                /* pull filtered pictures from the filtergraph */
                while (1) {
                    ret = av_buffersink_get_buffer_ref(buffersink_ctx, &picref, 0);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                        break;
                    if (ret < 0)
                        goto end;

                    if (picref) {
                        // 这里写H264编码代码
                        avpicture_fill((AVPicture *) pFrameYUV, out_buffer, PIX_FMT_YUV420P,
                                       pCodecCtx->width, pCodecCtx->height);
                        sws_scale(img_convert_ctx, (const uint8_t *const *) picref->data,
                                  picref->linesize, 0, pCodecCtx->height, pFrameYUV->data,
                                  pFrameYUV->linesize);
                        encodeH264(pbean, pFrameYUV);
                        avfilter_unref_bufferp(&picref);
                    }
                }
            } else {
                skipped_frame++;
            }
        }
        av_free_packet(&packet);
    }

    i = 0;
    for (i = skipped_frame; i > 0; i--) {
        ret = avcodec_decode_video2(pCodecCtx, &frame, &got_frame, &packet);
        if (got_frame) {
            // packet.pts = av_rescale_q(packet.pts,video_dec_st->time_base,video_enc_st->time_base);
            frame.pts = av_frame_get_best_effort_timestamp(&frame);
            if (av_buffersrc_add_frame(buffersrc_ctx, &frame) < 0) {
                LOGE("Error while feeding the filtergraph\n");
                break;
            }
            /* pull filtered pictures from the filtergraph */
            while (1) {
                AVFilterBufferRef *picref = NULL;
                ret = av_buffersink_get_buffer_ref(buffersink_ctx, &picref, 0);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                    break;
                if (ret < 0)
                    goto end;

                if (picref) {
                    // 这里写H264编码代码
                    avpicture_fill((AVPicture *) pFrameYUV, out_buffer, PIX_FMT_YUV420P,
                                   pCodecCtx->width, pCodecCtx->height);
                    sws_scale(img_convert_ctx, (const uint8_t *const *) picref->data,
                              picref->linesize, 0, pCodecCtx->height, pFrameYUV->data,
                              pFrameYUV->linesize);
                    encodeH264(pbean, pFrameYUV);
                    avfilter_unref_bufferp(&picref);
                }
            }
        }
    }
    av_free(out_buffer);
    av_free(pFrameYUV);
    sws_freeContext(img_convert_ctx);
    //刷新编码器以及回收编码器所用到的容器
    ret = flush_encoder2(pbean->pFormatCtx, 0);
    if (ret < 0) {
        LOGE("Flushing video encoder failed\n");
        return -1;
    }
    //Write file trailer
    av_write_trailer(pbean->pFormatCtx);
    //Clean video encoder
    if (pbean->video_st) {
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

    avformat_close_input(&pFormatCtx);

    if (ret < 0 && ret != AVERROR_EOF) {
        char buf[1024];
        av_strerror(ret, buf, sizeof(buf));
        LOGE("Error occurred: %s\n", buf);
        return -1;
    }
    getAACFromVideo(in_vFilename, aacpath);
    muxerMp4(h264path, aacpath, out_vFilename);
    return 0;
}

extern "C"
JNIEXPORT jint JNICALL
Java_cn_poco_mycpp_VideoNativeUtils_AddPNGToMp4(JNIEnv *env, jclass thiz, jstring jinputpath,
                                                jstring jtemppath, jstring joutputpath,
                                                jstring jbmppath) {
    char *inputpath = (char *) (env)->GetStringUTFChars(jinputpath, NULL);
    char *temppath = (char *) (env)->GetStringUTFChars(jtemppath, NULL);
    char *outputpath = (char *) (env)->GetStringUTFChars(joutputpath, NULL);
    char *bmppath = (char *) (env)->GetStringUTFChars(jbmppath, NULL);

    return addPNGToMp4(inputpath, temppath, outputpath, bmppath);
}


// 修改视频速率

int VFRtoCFR(const char *MP4PATH, const char *AACPATH, const char *OUTPUTMP4) {
#if USE_AACBSF
    AVBitStreamFilterContext *aacbsfc;
#endif

    const char *in_filename_v = MP4PATH;
    const char *in_filename_a = AACPATH;
    const char *out_filename = OUTPUTMP4;
    AVOutputFormat *ofmt = NULL;
    int haveaacdata = 1;
    //Input AVFormatContext and Output AVFormatContext
    AVFormatContext *ifmt_ctx_v = NULL, *ifmt_ctx_a = NULL, *ofmt_ctx = NULL;
    AVPacket pkt;
    int ret, i;
    int videoindex_v = -1, videoindex_out = -1;
    int audioindex_a = -1, audioindex_out = -1;
    int frame_index = 0;
    int64_t cur_pts_v = 0, cur_pts_a = 0;

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
    if (haveaacdata == 1)
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
        if (ifmt_ctx_v->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            AVStream *in_stream = ifmt_ctx_v->streams[i];
            AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
            videoindex_v = i;
            if (!out_stream) {
                LOGE("Failed allocating output stream\n");
                ret = AVERROR_UNKNOWN;
                goto end;
            }
            videoindex_out = out_stream->index;
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
    if (haveaacdata == 1)
        for (i = 0; i < ifmt_ctx_a->nb_streams; i++) {
            //Create output AVStream according to input AVStream
            if (ifmt_ctx_a->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
                AVStream *in_stream = ifmt_ctx_a->streams[i];
                AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
                audioindex_a = i;
                if (!out_stream) {
                    LOGE("Failed allocating output stream\n");
                    ret = AVERROR_UNKNOWN;
                    goto end;
                }
                audioindex_out = out_stream->index;
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
    aacbsfc = av_bitstream_filter_init("aac_adtstoasc");
#endif
    while (1) {
        AVFormatContext *ifmt_ctx;
        int stream_index = 0;
        AVStream *in_stream, *out_stream;

        if (audioindex_a == -1 || haveaacdata == 0) //如果没有音频直接写入视频
        {
            ifmt_ctx = ifmt_ctx_v;
            stream_index = videoindex_out;

            if (av_read_frame(ifmt_ctx, &pkt) >= 0) {
                do {
                    in_stream = ifmt_ctx->streams[pkt.stream_index];
                    out_stream = ofmt_ctx->streams[stream_index];

                    if (pkt.stream_index == videoindex_v) {
                        //FIX：No PTS (Example: Raw H.264)
                        //Simple Write PTS
                        if (pkt.pts == AV_NOPTS_VALUE) {
                            //Write PTS
                            AVRational time_base1 = in_stream->time_base;
                            //Duration between 2 frames (us)
                            int64_t calc_duration =
                                    (double) AV_TIME_BASE / av_q2d(in_stream->r_frame_rate);
                            //Parameters
                            pkt.pts = (double) (frame_index * calc_duration) /
                                      (double) (av_q2d(time_base1) * AV_TIME_BASE);
                            pkt.dts = pkt.pts;
                            pkt.duration = (double) calc_duration /
                                           (double) (av_q2d(time_base1) * AV_TIME_BASE);
                            frame_index++;
                        }

                        cur_pts_v = pkt.pts;
                        break;
                    }
                } while (av_read_frame(ifmt_ctx, &pkt) >= 0);
            } else {
                break;
            }
        }
            //Get an AVPacket
        else if (av_compare_ts(cur_pts_v, ifmt_ctx_v->streams[videoindex_v]->time_base, cur_pts_a,
                               ifmt_ctx_a->streams[audioindex_a]->time_base) <= 0) {
            ifmt_ctx = ifmt_ctx_v;
            stream_index = videoindex_out;
            if (av_read_frame(ifmt_ctx, &pkt) >= 0) {
                do {
                    in_stream = ifmt_ctx->streams[pkt.stream_index];
                    out_stream = ofmt_ctx->streams[stream_index];

                    if (pkt.stream_index == videoindex_v) {
                        //FIX：No PTS (Example: Raw H.264)
                        //Simple Write PTS
                        if (pkt.pts == AV_NOPTS_VALUE) {
                            //Write PTS
                            AVRational time_base1 = in_stream->time_base;
                            //Duration between 2 frames (us)
                            int64_t calc_duration =
                                    (double) AV_TIME_BASE / av_q2d(in_stream->r_frame_rate);
                            //Parameters
                            pkt.pts = (double) (frame_index * calc_duration) /
                                      (double) (av_q2d(time_base1) * AV_TIME_BASE);
                            pkt.dts = pkt.pts;
                            pkt.duration = (double) calc_duration /
                                           (double) (av_q2d(time_base1) * AV_TIME_BASE);
                            frame_index++;
                        }

                        cur_pts_v = pkt.pts;
                        break;
                    }
                } while (av_read_frame(ifmt_ctx, &pkt) >= 0);
            } else {
                break;
            }
        } else {
            ifmt_ctx = ifmt_ctx_a;
            stream_index = audioindex_out;
            if (av_read_frame(ifmt_ctx, &pkt) >= 0) {
                do {
                    in_stream = ifmt_ctx->streams[pkt.stream_index];
                    out_stream = ofmt_ctx->streams[stream_index];

                    if (pkt.stream_index == audioindex_a) {

                        //FIX：No PTS
                        //Simple Write PTS
                        if (pkt.pts == AV_NOPTS_VALUE) {
                            //Write PTS
                            AVRational time_base1 = in_stream->time_base;
                            //Duration between 2 frames (us)
                            int64_t calc_duration =
                                    (double) AV_TIME_BASE / av_q2d(in_stream->r_frame_rate);
                            //Parameters
                            pkt.pts = (double) (frame_index * calc_duration) /
                                      (double) (av_q2d(time_base1) * AV_TIME_BASE);
                            pkt.dts = pkt.pts;
                            pkt.duration = (double) calc_duration /
                                           (double) (av_q2d(time_base1) * AV_TIME_BASE);
                            frame_index++;
                        }
                        cur_pts_a = pkt.pts;

                        break;
                    }
                } while (av_read_frame(ifmt_ctx, &pkt) >= 0);
            } else {
                break;
            }

        }
        //FIX:Bitstream Filter
#if USE_H264BSF
        av_bitstream_filter_filter(h264bsfc, in_stream->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);
#endif
#if USE_AACBSF
        av_bitstream_filter_filter(aacbsfc, out_stream->codec, NULL, &pkt.data, &pkt.size, pkt.data,
                                   pkt.size, 0);
#endif

        //Convert PTS/DTS
        pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base,
                                   (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base,
                                   (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;
        pkt.stream_index = stream_index;

        //LOGE("Write 1 Packet. size:%5d\tpts:%lld\n",pkt.size,pkt.pts);
        //Write
        if (av_interleaved_write_frame(ofmt_ctx, &pkt) < 0) {
            LOGE("Error muxing packet\n");
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

int changeVideoSpeed(const char *video_in, const char *video_out1, const char *video_out2,
                     float radio) {
    AVOutputFormat *ofmt = NULL;
    //Input AVFormatContext and Output AVFormatContext
    AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
    AVPacket pkt;
    const char *in_filename, *out_filename;
    int ret, i;
    int frame_index = 0;
    float speedratio = 1 / radio;
    in_filename = video_in;
    out_filename = video_out1;//Output file URL

    av_register_all();
    //Input
    if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
        LOGE("Could not open input file.");
        goto end;
    }
    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        LOGE("Failed to retrieve input stream information");
        goto end;
    }
    av_dump_format(ifmt_ctx, 0, in_filename, 0);
    //Output
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);
    if (!ofmt_ctx) {
        LOGE("Could not create output context\n");
        ret = AVERROR_UNKNOWN;
        goto end;
    }
    ofmt = ofmt_ctx->oformat;
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        //Create output AVStream according to input AVStream
        AVStream *in_stream = ifmt_ctx->streams[i];
        AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
        if (!out_stream) {
            LOGE("Failed allocating output stream\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }
        //Copy the settings of AVCodecContext
        if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
            LOGE("Failed to copy context from input to output stream codec context\n");
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
            LOGE("Could not open output file '%s'", out_filename);
            goto end;
        }
    }
    //Write file header
    if (avformat_write_header(ofmt_ctx, NULL) < 0) {
        LOGE("Error occurred when opening output file\n");
        goto end;
    }

    AVRational newtimebase;
    while (1) {
        AVStream *in_stream, *out_stream;
        //Get an AVPacket
        ret = av_read_frame(ifmt_ctx, &pkt);
        if (ret < 0)
            break;
        in_stream = ifmt_ctx->streams[pkt.stream_index];
        out_stream = ofmt_ctx->streams[pkt.stream_index];

        newtimebase.den = out_stream->time_base.den;
        newtimebase.num = out_stream->time_base.num;

        newtimebase.den *= speedratio;
        //Convert PTS/DTS
        pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, newtimebase,
                                   (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, newtimebase,
                                   (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, newtimebase);
        pkt.pos = -1;

        //Write
        if (av_interleaved_write_frame(ofmt_ctx, &pkt) < 0) {
            LOGE("Error muxing packet\n");
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

    VFRtoCFR(video_out1, "", video_out2);
    return 1;
}

extern "C"
JNIEXPORT jint JNICALL
Java_cn_poco_mycpp_VideoNativeUtils_ChangeVideoSpeed(JNIEnv *env, jclass thiz, jstring jinputpath,
                                                     jstring jtemppath, jstring joutputpath,
                                                     jfloat radio) {

    char *inputpath = (char *) (env)->GetStringUTFChars(jinputpath, NULL);
    char *temppath = (char *) (env)->GetStringUTFChars(jtemppath, NULL);
    char *outputpath = (char *) (env)->GetStringUTFChars(joutputpath, NULL);

    return changeVideoSpeed(inputpath, temppath, outputpath, radio);
}



// 2个视频融合

int video_stream_index1 = -1;
int video_stream_index2 = -1;

static int
open_input_file2(const char *filename, AVFormatContext **pFormatCtx, AVCodecContext **pCodecCtx,
                 int index) {
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
    if (index == 1) {
        video_stream_index1 = ret;
        *pCodecCtx = (*pFormatCtx)->streams[video_stream_index1]->codec;
    } else if (index == 2) {
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

int
getNextFrame(AVFormatContext *pFormatCtx, AVCodecContext *pCodecCtx, AVFrame *pFrame, int index) {
    int video_stream_index;
    if (index == 1)
        video_stream_index = video_stream_index1;
    else if (index == 2)
        video_stream_index = video_stream_index2;
    AVPacket packet;
    while (1) {
        //read one packet.

        int ret = av_read_frame(pFormatCtx, &packet);
        if (ret < 0)
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

int initEncodeh264Bean(Encodeh264BeanPtr pbean, const char *out_file, AVFormatContext *pFormatCtx,
                       AVCodecContext *pCodecCtx, int index) {

    int video_stream_index;
    if (index == 1)
        video_stream_index = video_stream_index1;
    else if (index == 2)
        video_stream_index = video_stream_index2;

    pbean->framecnt = 0;

    //Method1.
    pbean->pFormatCtx = avformat_alloc_context();
    //Guess Format
    pbean->fmt = av_guess_format(NULL, out_file, NULL);
    pbean->pFormatCtx->oformat = pbean->fmt;

    //Method 2.
    //avformat_alloc_output_context2(&pFormatCtx, NULL, NULL, out_file);
    //fmt = pFormatCtx->oformat;


    //Open output URL
    if (avio_open(&pbean->pFormatCtx->pb, out_file, AVIO_FLAG_READ_WRITE) < 0) {
        printf("Failed to open output file! \n");
        return -1;
    }

    pbean->video_st = avformat_new_stream(pbean->pFormatCtx, 0);
//	pbean->video_st->time_base.num = pFormatCtx->streams[video_stream_index]->time_base.num;
//	pbean->video_st->time_base.den = pFormatCtx->streams[video_stream_index]->time_base.den;

    if (pbean->video_st == NULL) {
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
    pbean->pCodecCtx->time_base.den = pFormatCtx->streams[video_stream_index]->avg_frame_rate.num /
                                      pFormatCtx->streams[video_stream_index]->avg_frame_rate.den;
    pbean->pCodecCtx->bit_rate = 1024 * 1024;
    pbean->pCodecCtx->gop_size = 3;
    //H264
    pbean->pCodecCtx->me_range = 16;
    pbean->pCodecCtx->max_qdiff = 4;
    //pbean->pCodecCtx->qcompress = 0.6;
    pbean->pCodecCtx->qmin = 10;
    pbean->pCodecCtx->qmax = 51;

    //Optional Param
    pbean->pCodecCtx->max_b_frames = 0;

    // Set Option
    AVDictionary *param = 0;
    //H.264
    if (pbean->pCodecCtx->codec_id == AV_CODEC_ID_H264) {
        av_dict_set(&param, "preset", "ultrafast", 0);
        av_dict_set(&param, "tune", "zerolatency", 0);
        //av_dict_set(&param, "profile", "baseline", 0);
        //av_dict_set(&param, "profile", "main", 0);
    }
    //H.265
    if (pbean->pCodecCtx->codec_id == AV_CODEC_ID_H265) {
        av_dict_set(&param, "preset", "ultrafast", 0);
        av_dict_set(&param, "tune", "zero-latency", 0);
    }

    //Show some Information
    av_dump_format(pbean->pFormatCtx, 0, out_file, 1);

    pbean->pCodec = avcodec_find_encoder(pbean->pCodecCtx->codec_id);
    if (!pbean->pCodec) {
        LOGE("Can not find encoder! \n");
        return -1;
    }
    if (avcodec_open2(pbean->pCodecCtx, pbean->pCodec, &param) < 0) {
        LOGE("Failed to open encoder! \n");
        return -1;
    }


    pbean->pFrame = av_frame_alloc();
    pbean->pFrame->pts = 0;
    pbean->picture_size = avpicture_get_size(pbean->pCodecCtx->pix_fmt, pbean->pCodecCtx->width,
                                             pbean->pCodecCtx->height);
    pbean->picture_buf = (uint8_t *) av_malloc(pbean->picture_size);
    avpicture_fill((AVPicture *) (pbean->pFrame), pbean->picture_buf, pbean->pCodecCtx->pix_fmt,
                   pbean->pCodecCtx->width, pbean->pCodecCtx->height);

    //Write File Header
    avformat_write_header(pbean->pFormatCtx, NULL);

    //av_new_packet(&pbean->pkt,pbean->picture_size);

    pbean->y_size = pbean->pCodecCtx->width * pbean->pCodecCtx->height;

    return 1;
}

int flush_encoder(AVFormatContext *fmt_ctx, unsigned int stream_index) {
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
        ret = avcodec_encode_video2(fmt_ctx->streams[stream_index]->codec, &enc_pkt,
                                    NULL, &got_frame);
        av_frame_free(NULL);
        if (ret < 0)
            break;
        if (!got_frame) {
            ret = 0;
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

int encodeH264(Encodeh264BeanPtr pbean) {
    /*pbean->pFrame->data[0] = picref->data[0];              // Y
    pbean->pFrame->data[1] = picref->data[1];
    pbean->pFrame->data[2] = picref->data[2];*/

    int got_picture = 0;
    //Encode
    av_new_packet(&pbean->pkt, pbean->picture_size);
    //pbean->pFrame->pts = picref->pts;
    LOGE("start avcodec_encode_video2");
    int ret = avcodec_encode_video2(pbean->pCodecCtx, &pbean->pkt, pbean->pFrame, &got_picture);
    LOGE("end avcodec_encode_video2");
    if (ret < 0) {
        LOGE("Failed to encode! \n");
        return -1;
    }
    if (got_picture == 1) {
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

static int
init_filter_graph(AVFilterGraph **pGraph, AVFilterContext **pInput1, AVFilterContext **pInput2,
                  AVFilterContext **pOutput, AVFormatContext *pFormatCtx, AVCodecContext *pCodecCtx,
                  int time) {
    AVFilterGraph *tFilterGraph;
    AVFilterContext *tBufferContext1;
    AVFilter *tBuffer1;
    AVFilterContext *tBufferContext2;
    AVFilter *tBuffer2;
    AVFilterContext *tOverlayContext;
    AVFilter *tOverlay;
    AVFilterContext *tBufferSinkContext;
    AVFilter *tBufferSink;
    AVFilter *tFormat;
    AVFilterContext *tFormatContext;

    int tError;

    /* Create a new filtergraph, which will contain all the filters. */
    tFilterGraph = avfilter_graph_alloc();
    char args[512];
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=0:time_base=%d/%d:pixel_aspect=1/1", pCodecCtx->width,
             pCodecCtx->height, 1,
             pFormatCtx->streams[video_stream_index1]->avg_frame_rate.num /
             pFormatCtx->streams[video_stream_index1]->avg_frame_rate.den);

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
                 "A*(if(gte(T,%d),1,T/%d))+B*(1-(if(gte(T,%d),1,T/%d)))", time * 10000,
                 time * 10000, time * 10000, time * 10000);
        AVDictionary *tOptionsDict = NULL;
        av_dict_set(&tOptionsDict, "all_mode", "normal", 0);
        av_dict_set(&tOptionsDict, "all_opacity", "1", 0);
        //	av_dict_set(&tOptionsDict, "all_expr", "if(eq(mod(X,2),mod(Y,2)),A,B)", 0);
        av_dict_set(&tOptionsDict, "all_expr", blendargs, 0);
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
    if (tError >= 0) {
        tError = avfilter_link(tBufferContext2, 0, tOverlayContext, 1);
    }
    if (tError >= 0) {
        tError = avfilter_link(tOverlayContext, 0, tFormatContext, 0);
    }
    if (tError >= 0) {
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

extern "C"
JNIEXPORT jint JNICALL
Java_cn_poco_mycpp_VideoNativeUtils_Blend2Video(JNIEnv *pEnv, jclass object, jstring MP4PATH1,
                                                jstring MP4PATH2, jstring OUTPUTH264,
                                                jstring OUTMP4PUTH, jint type, jint time) {
    LOGE("开始融合视频...");
    char *in_vFilename1 = (char *) (pEnv)->GetStringUTFChars(MP4PATH1, NULL);
    char *in_vFilename2 = (char *) (pEnv)->GetStringUTFChars(MP4PATH2, NULL);
    char *out_vh264name = (char *) (pEnv)->GetStringUTFChars(OUTPUTH264, NULL);
    char *out_vFilename = (char *) (pEnv)->GetStringUTFChars(OUTMP4PUTH, NULL);

    int ret;
    AVPacket packet;
    int got_frame;

    AVFilterContext *inputContexts1, *inputContexts2;
    AVFilterContext *outputContext;
    AVFilterGraph *filter_graph;

    AVFormatContext *pFormatCtx = NULL, *pFormatCtx2 = NULL;
    AVCodecContext *pCodecCtx = NULL, *pCodecCtx2 = NULL;

    Encodeh264BeanPtr pbean;
    AVFrame *Frame;
    int in_width;
    int in_height;

    avcodec_register_all();
    av_register_all();
    avfilter_register_all();

    //    char *out_videofilename = (*pEnv)->GetStringUTFChars(pEnv, pFileName, 0);
    if ((ret = open_input_file2(in_vFilename1, &pFormatCtx, &pCodecCtx, 1)) < 0)
        goto end;
    if ((ret = open_input_file2(in_vFilename2, &pFormatCtx2, &pCodecCtx2, 2)) < 0)
        goto end;

    if ((ret = init_filter_graph(&filter_graph, &inputContexts1, &inputContexts2, &outputContext,
                                 pFormatCtx2, pCodecCtx2, time)) < 0)
        goto end;


    ///初始化H264编码器
    pbean = (Encodeh264BeanPtr) malloc(sizeof(encodeh264bean));
    initEncodeh264Bean(pbean, out_vh264name, pFormatCtx2, pCodecCtx2, 2);

    // 开始解码两个视频并推送到Avfilter处理
    in_width = pCodecCtx->width;
    in_height = pCodecCtx->height;
    AVFrame *frame_in;
    frame_in = av_frame_alloc();

    struct SwsContext *img_convert_ctx;
    img_convert_ctx = sws_getContext(in_width, in_height, pCodecCtx->pix_fmt, in_width, in_height,
                                     PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);

    frame_in->width = in_width;
    frame_in->height = in_height;
    frame_in->format = AV_PIX_FMT_YUV420P;

    Frame = av_frame_alloc();
    AVFilterBufferRef *picref;

    while (getNextFrame(pFormatCtx, pCodecCtx, Frame, 1) > 0) {

        LOGE("正在塞bottom层数据. pts = %lld             ", Frame->pts);
        if (av_buffersrc_add_frame(inputContexts2, Frame) < 0) {
            printf("Error while feeding the filtergraph\n");
            break;
        }
        if (getNextFrame(pFormatCtx2, pCodecCtx2, frame_in, 2) > 0) {

            frame_in->pts = Frame->pts;//av_rescale_q_rnd(frame_in->pts, pCodecCtx2->time_base, pCodecCtx->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
            LOGE("正在塞top层数剧. pts = %lld             ", frame_in->pts);
            if (av_buffersrc_add_frame(inputContexts1, frame_in) < 0) {
                printf("Error while add frame.\n");
                break;
            }
        }

        // pull filtered pictures from the filtergraph
        while (1) {
            ret = av_buffersink_get_buffer_ref(outputContext, &picref, 0);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            if (ret < 0)
                break;

            if (picref) {
                //avfilter_copy_buf_props(pFrame_out,picref);
                //avpicture_fill((AVPicture *)pFrame_out, out_buffer, PIX_FMT_YUV420P, in_width, in_height);
                sws_scale(img_convert_ctx, (const uint8_t *const *) picref->data, picref->linesize,
                          0, in_height, pbean->pFrame->data, pbean->pFrame->linesize);

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
    if (pbean->video_st) {
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
    muxerMp4(out_vh264name, "", out_vFilename);

    return 1;
}