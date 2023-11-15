
#define __STDC_CONSTANT_MACROS
#include <iostream>

#include "cv.h"
#include "highgui.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/mathematics.h>
#include <libswscale/swscale.h>
}


// variabili globali per FFMpeg
AVFrame* picture, * tmp_picture;
uint8_t* video_outbuf;
int video_outbuf_size;


static bool checkMaskingVideo(CvCapture* inputVideo, CvCapture* maskingVideo)
{
    // se i frames totali o gli fps sono diversi non c'è problema, contano solo width e height
    // ATTENZIONE: non c'è alcun controllo che il maskingVideo sia in bianco e nero!

    if(maskingVideo == NULL)
    {
        return true;
    }

    int h1 = cvGetCaptureProperty(inputVideo, CV_CAP_PROP_FRAME_HEIGHT);
    int w1 = cvGetCaptureProperty(inputVideo, CV_CAP_PROP_FRAME_WIDTH);

    int h2 = cvGetCaptureProperty(maskingVideo, CV_CAP_PROP_FRAME_HEIGHT);
    int w2 = cvGetCaptureProperty(maskingVideo, CV_CAP_PROP_FRAME_WIDTH);

    if(h1 != h2 || w1 != w2)
    {
        return false;
    }

    return true;
}


static void maskedSmoothing(IplImage* img, IplImage* mask)
{
    // effettua smoothing di img in tutti e soli i pixel omologhi a quelli BIANCHI di mask

    int w = img->width;
    int h = img->height;
    CvSize size = cvSize(w, h);
    int depth = img->depth;
    int nChannels = img->nChannels;

    // smothing di TUTTA l'immagine
    IplImage* img_smoothed =  cvCreateImage(size, depth, nChannels);
    cvSmooth(img, img_smoothed, CV_BLUR, 5, 5);

    // le due img_temp sommate (cvAdd) insieme daranno il risultato (sovrascritto su img)
    IplImage* img_temp1 =  cvCreateImage(size, depth, nChannels);
    IplImage* img_temp2 =  cvCreateImage(size, depth, nChannels);

    // creazione maschera complementare
    IplImage* notmask = cvCreateImage(size, IPL_DEPTH_8U, 3);
    cvNot(mask, notmask);

    cvAnd(img_smoothed, mask, img_temp1); //AND
    cvAnd(img, notmask, img_temp2); //AND

    cvAdd(img_temp1, img_temp2, img); // add (somma)

    cvReleaseImage(&img_smoothed);
    cvReleaseImage(&img_temp1);
    cvReleaseImage(&img_temp2);
    cvReleaseImage(&notmask);
}


// http://code.google.com/p/vmon/wiki/IplImageToAVFrame
static void IplImage_to_AVFrame(IplImage* iplImage, AVFrame* avFrame, enum PixelFormat pix_fmt)
{
    struct SwsContext* img_convert_ctx = 0;
    int linesize[4] = {0, 0, 0, 0};

    uint8_t** imageData;

    img_convert_ctx = sws_getContext(iplImage->width, iplImage->height,
                                     PIX_FMT_BGR24,
                                     iplImage->width,
                                     iplImage->height,
                                     pix_fmt, SWS_BICUBIC, 0, 0, 0);

    if(img_convert_ctx != 0)
    {
        linesize[0] = 3 * iplImage->width;
        imageData = (uint8_t**)(&iplImage->imageData);
        sws_scale(img_convert_ctx, imageData, linesize, 0, iplImage->height, avFrame->data, avFrame->linesize);
        sws_freeContext(img_convert_ctx);
    }
}


/*
 * Il seguente codice, fino alla main (esclusa), è un adattamento del seguente esempio:
 * http://cekirdek.uludag.org.tr/~ismail/ffmpeg-docs/output-example_8c-source.html
 */

static AVStream* add_video_stream(AVFormatContext* oc, CodecID codec_id, CvCapture* capture, int fps, float crf)
{
    AVCodecContext* c;
    AVStream* st;

    st = av_new_stream(oc, 0);

    if(!st)
    {
        printf("Could not alloc stream\n");
        exit(1);
    }

    c = st->codec;
    c->codec_id = codec_id;
    c->codec_type = CODEC_TYPE_VIDEO;

    c->time_base.num = 1;
    c->time_base.den = fps;
    c->width = cvGetCaptureProperty(capture, CV_CAP_PROP_FRAME_WIDTH);
    c->height = cvGetCaptureProperty(capture, CV_CAP_PROP_FRAME_HEIGHT);
    c->pix_fmt = PIX_FMT_YUV420P;


    // i seguenti 5 parametri sono necessari, altrimenti "broken ffmpeg default settings detected" !!!
    // i valori sono presi da "libx264-default.ffpreset"
    c->qcompress = 0.6;
    c->qmin = 10;
    c->qmax = 51;
    c->max_qdiff = 4;
    c->i_quant_factor = 0.71;

    // Constant Rate Factor
    c->crf = crf; // 25=qualità bassa, 15=qualità alta (ad esempio!)


    // some formats want stream headers to be separate
    if(oc->oformat->flags & AVFMT_GLOBALHEADER)
    {
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }

    return st;
}


static AVFrame* alloc_picture(int pix_fmt, int width, int height)
{
    AVFrame* picture;
    uint8_t* picture_buf;
    int size;

    picture = avcodec_alloc_frame();

    if(!picture)
    {
        return NULL;
    }

    size = avpicture_get_size(pix_fmt, width, height);
    picture_buf = (uint8_t*) av_malloc(size);

    if(!picture_buf)
    {
        av_free(picture);
        return NULL;
    }

    avpicture_fill((AVPicture*)picture, picture_buf,
                   pix_fmt, width, height);
    return picture;
}


static void open_video(AVFormatContext* oc, AVStream* st)
{
    AVCodec* codec;
    AVCodecContext* c;

    c = st->codec;

    /* find the video encoder */
    codec = avcodec_find_encoder(c->codec_id);

    if(!codec)
    {
        printf("codec not found\n");
        exit(1);
    }

    /* open the codec */
    if(avcodec_open(c, codec) < 0)
    {
        printf("could not open codec\n");
        exit(1);
    }

    video_outbuf = NULL;

    if(!(oc->oformat->flags & AVFMT_RAWPICTURE))
    {
        /* allocate output buffer */
        video_outbuf_size = 200000;
        video_outbuf = (uint8_t*) av_malloc(video_outbuf_size);
    }

    /* allocate the encoded raw picture */
    picture = alloc_picture(c->pix_fmt, c->width, c->height);

    if(!picture)
    {
        printf("Could not allocate picture\n");
        exit(1);
    }

}



static int write_video_frame(AVFormatContext* oc, AVStream* st, AVFrame* pic, IplImage* openCV_frame, IplImage* mask)
{
    // il fatto che sia "int" serve solo per svuotare il buffer al termine del video

    int out_size;
    int ret;
    AVCodecContext* c;

    c = st->codec;

    if(openCV_frame != NULL && pic != NULL)
    {

        // masked-smoothing (qualunque altra operazione con OpenCV va fatta qui!)
        if(mask != NULL)
        {
            maskedSmoothing(openCV_frame, mask);
        }

        // conversione da IplImage ad AVFrame
        IplImage_to_AVFrame(openCV_frame, pic, PIX_FMT_YUV420P);

    }

    /* encode the image */
    out_size = avcodec_encode_video(c, video_outbuf, video_outbuf_size, pic);

    /* if zero size, it means the image was buffered */
    if(out_size > 0)
    {
        AVPacket pkt;
        av_init_packet(&pkt);

        if(c->coded_frame->pts != AV_NOPTS_VALUE)
        {
            pkt.pts = av_rescale_q(c->coded_frame->pts, c->time_base, st->time_base);
        }

        if(c->coded_frame->key_frame)
        {
            pkt.flags |= PKT_FLAG_KEY;
        }

        pkt.stream_index = st->index;
        pkt.data = video_outbuf;
        pkt.size = out_size;

        /* write the compressed frame in the media file */
        ret = av_interleaved_write_frame(oc, &pkt);
    }
    else
    {
        ret = 0;
    }

    if(ret != 0)
    {
        printf("Error while writing video frame\n");
        exit(1);
    }

    return out_size; // utilizzato solo per svuotare il buffer
}


static void close_video(AVFormatContext* oc, AVStream* st)
{
    avcodec_close(st->codec);
    av_free(picture->data[0]);
    av_free(picture);

    if(tmp_picture)
    {
        av_free(tmp_picture->data[0]);
        av_free(tmp_picture);
    }

    av_free(video_outbuf);
}


static void encodeH264video(CvCapture* inputVideo, char* filename, int fps, float crf, CvCapture* maskingVideo)
{
    AVOutputFormat* fmt;
    AVFormatContext* oc;
    AVStream* video_st;
    double video_pts;
    int i;

    /* initialize libavcodec, and register all codecs and formats */
    av_register_all();

    // inizializzazioni per OpenCV
    IplImage* openCV_frame;
    IplImage* mask = NULL;

    /* auto detect the output format from the name. default is
       mpeg. */
    fmt = guess_format(NULL, filename, NULL);

    if(!fmt)
    {
        printf("Could not deduce output format from file extension: using MPEG.\n");
        fmt = guess_format("mpeg", NULL, NULL);
    }

    if(!fmt)
    {
        printf("Could not find suitable output format\n");
        exit(1);
    }

    /* allocate the output media context */
    oc = avformat_alloc_context();

    if(!oc)
    {
        printf("Memory error\n");
        exit(1);
    }

    oc->oformat = fmt;
    snprintf(oc->filename, sizeof(oc->filename), "%s", filename);

    video_st = NULL;

    if(fmt->video_codec != CODEC_ID_NONE)
    {
        video_st = add_video_stream(oc, CODEC_ID_H264, inputVideo, fps, crf);
    }

    /* set the output parameters (must be done even if no parameters). */
    if(av_set_parameters(oc, NULL) < 0)
    {
        printf("Invalid output format parameters\n");
        exit(1);
    }

    dump_format(oc, 0, filename, 1);

    /* now that all the parameters are set, we can open the
       video codec and allocate the necessary encode buffers */
    if(video_st)
    {
        open_video(oc, video_st);
    }

    /* open the output file, if needed */
    if(!(fmt->flags & AVFMT_NOFILE))
    {
        if(url_fopen(&oc->pb, filename, URL_WRONLY) < 0)
        {
            printf("Could not open '%s'\n", filename);
            exit(1);
        }
    }

    /* write the stream header, if any */
    av_write_header(oc);


    // main loop --------------------------------------------------------------------------------------
    while((openCV_frame = cvQueryFrame(inputVideo)) != NULL)
    {

        /* compute current video time */
        if(video_st)
        {
            video_pts = (double)video_st->pts.val * video_st->time_base.num / video_st->time_base.den;
        }
        else
        {
            video_pts = 0.0;
        }

        if(!video_st)
        {
            break;
        }

        if(maskingVideo != NULL)
        {
            mask = cvQueryFrame(maskingVideo);
        }

        write_video_frame(oc, video_st, picture, openCV_frame, mask);
        // NULL check su mask a suo carico e comportamento coerente a seconda del caso

    }

    // end of main loop -------------------------------------------------------------------------------

    // il video è finito, ma devo recuperare i frames rimasti nel buffer!
    while(write_video_frame(oc, video_st, NULL, NULL, NULL) > 0)
    {
        ; // il corpo del while è svolto da write_video_frame stessa
    }

    /* write the trailer, if any.  the trailer must be written
     * before you close the CodecContexts open when you wrote the
     * header; otherwise write_trailer may try to use memory that
     * was freed on av_codec_close() */
    av_write_trailer(oc);

    /* close video codec */
    if(video_st)
    {
        close_video(oc, video_st);
    }

    /* free the streams */
    for(i = 0; i < oc->nb_streams; i++)
    {
        av_freep(&oc->streams[i]->codec);
        av_freep(&oc->streams[i]);
    }

    if(!(fmt->flags & AVFMT_NOFILE))
    {
        /* close the output file */
        url_fclose(oc->pb);
    }

    /* free the stream */
    av_free(oc);


}


int main(int argc, char** argv)
{

    if(argc < 5)
    {
        std::cout << "parametri: <inputVideo> <outVideo> <outVideoFPS> <CRF> [<maskingVideo>]" << std::endl;
        std::cout << "nota: se outVideoFPS<=0, verrà usata l'inaffidabile cvGetCaptureProperty(inputVideo, CV_CAP_PROP_FPS)" << std::endl;
        return -1;
    }


    char* inputVideoFile = argv[1];
    char* outVideoFile = argv[2];
    int fps = atoi(argv[3]); // perché cvGetCaptureProperty(inputVideo, CV_CAP_PROP_FPS) non è affidabile!
    float crf = atof(argv[4]);
    char* maskingVideoFile = argv[5]; // opzionale


    CvCapture* inputVideo = cvCreateFileCapture(inputVideoFile);

    if(!inputVideo)
    {
        std::cout << "ERRORE: input video non trovato!" << std::endl;
        return -1;
    }


    if(fps <= 0)
    {
        fps = cvGetCaptureProperty(inputVideo, CV_CAP_PROP_FPS);  // non è affidabile! :(
    }


    if(crf < 1)
    {
        std::cout << "ERRORE: inserire crf >= 1" << std::endl;
        return -1;
    }


    CvCapture* maskingVideo = NULL;

    if(maskingVideoFile != NULL) // è stato passato da linea di comando
    {
        maskingVideo = cvCreateFileCapture(maskingVideoFile);

        if(!maskingVideo)
        {
            std::cout << "ERRORE: masking video non trovato!" << std::endl;
            return -1;
        }
    }

    if(!checkMaskingVideo(inputVideo, maskingVideo))
    {
        std::cout << "ERRORE: i frames del masking video hanno dimensioni diverse da quelle dei frames del video di input!" << std::endl;
        return -1;
    }


    encodeH264video(inputVideo, outVideoFile, fps, crf, maskingVideo);
    // NULL-check su maskingVideo a suo carico e comportamento coerente a seconda del caso

    cvReleaseCapture(&inputVideo);
    cvReleaseCapture(&maskingVideo);

    return 0;
}
