
#include <iostream>

#include "cv.h"
#include "highgui.h"

#include "ssim.h"


static bool comparable(CvCapture* video1, CvCapture* video2)
{

    int h1 = cvGetCaptureProperty(video1, CV_CAP_PROP_FRAME_HEIGHT);
    int w1 = cvGetCaptureProperty(video1, CV_CAP_PROP_FRAME_WIDTH);

    int h2 = cvGetCaptureProperty(video2, CV_CAP_PROP_FRAME_HEIGHT);
    int w2 = cvGetCaptureProperty(video2, CV_CAP_PROP_FRAME_WIDTH);

    if(h1 != h2 || w1 != w2)
    {
        return false;
    }

    return true;
}


static int framesCounter(CvCapture* video)
{

    int frames = 0;

    while(cvGrabFrame(video))
    {
        frames++;
    }

    return frames;
}


int main(int argc, char* argv[])
{

    if(argc < 3)
    {
        std::cout << "parametri: <video1> <video2> [<maskingVideo>]" << std::endl;
        return -1;
    }


    char* videoFile1 = argv[1];
    char* videoFile2 = argv[2];
    char* maskingVideoFile = argv[3]; // opzionale


    // primo video
    CvCapture* video1 = cvCreateFileCapture(videoFile1);

    if(!video1)
    {
        std::cout << "ERRORE: primo video non trovato!" << std::endl;
        return -1;
    }

    cv::Mat frame1;


    // secondo video
    CvCapture* video2 = cvCreateFileCapture(videoFile2);

    if(!video2)
    {
        std::cout << "ERRORE: secondo video non trovato!" << std::endl;
        return -1;
    }

    cv::Mat frame2;


    if(!comparable(video1, video2))
    {
        std::cout << "ERRORE: i video hanno dimensioni dei frames diverse!" << std::endl;
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

    if(maskingVideo != NULL)
    {
        if(!comparable(video1, maskingVideo))
        {
            std::cout << "ERRORE: il masking video ha dimensioni dei frames diverse da quelle dei due video!" << std::endl;
            return -1;
        }
    }

    IplImage* mask = NULL;

    /*
    // soluzione veloce ma forse inaffidabile
    int frames1 = cvGetCaptureProperty(video1, CV_CAP_PROP_FRAME_COUNT);
    int frames2 = cvGetCaptureProperty(video2, CV_CAP_PROP_FRAME_COUNT);
    */

    // soluzione lenta ma sicura
    std::cout << "Conteggio frames primo video..." << std::endl;
    int frames1 = framesCounter(video1);
    std::cout << "Conteggio frames secondo video..." << std::endl;
    int frames2 = framesCounter(video2);
    int minFrames = frames1;

    if(frames2 < minFrames)
    {
        minFrames = frames2;
    }

    // necessario re-inizializzare
    cvReleaseCapture(&video1);
    cvReleaseCapture(&video2);
    video1 = cvCreateFileCapture(videoFile1);
    video2 = cvCreateFileCapture(videoFile2);


    CvSize size;

    if(maskingVideo != NULL)
    {
        int w = cvGetCaptureProperty(maskingVideo, CV_CAP_PROP_FRAME_WIDTH);
        int h = cvGetCaptureProperty(maskingVideo, CV_CAP_PROP_FRAME_HEIGHT);
        size = cvSize(w, h);
    }


    double avgSSIM = 0;
    float tempSSIM = 0;
    int frames = 0;


    for(int i = 0; i < minFrames; i++)
    {

        frame1 = cvQueryFrame(video1);
        frame2 = cvQueryFrame(video2);

        if(maskingVideo != NULL)
        {
            mask = cvQueryFrame(maskingVideo);
        }


        if(mask != NULL) // il masking video potrebbe avere #frames < minFrames
        {

            IplImage* mask1ch = cvCreateImage(size, IPL_DEPTH_8U, 1);   // la maschera DEVE essere single-channel
            cvCvtColor(mask, mask1ch, CV_RGB2GRAY);
            cvNot(mask1ch, mask1ch); // perché voglio valutare la SSIM sui pixel NON smoothati, mentre la maschera selezionava i pixel da smoothare

            tempSSIM = calcSSIM(frame1, frame2, 0, CV_BGR2YCrCb, mask1ch);
            std::cout << "Frame " << (i + 1) << " di " << minFrames << ": SSIM = " << tempSSIM << std::endl;

            if(tempSSIM != 0)
            {
                avgSSIM += tempSSIM;
                frames++;
            }

            cvReleaseImage(&mask1ch);
        }
        else
        {
            tempSSIM = calcSSIM(frame1, frame2);
            std::cout << "Frame " << (i + 1) << " di " << minFrames << ": SSIM = " << tempSSIM << std::endl;

            if(tempSSIM != 0)
            {
                avgSSIM += tempSSIM;
                frames++;
            }
        }
    }


    avgSSIM = avgSSIM / frames;
    std::cout << "SSIM media calcolata su tutti i " << frames << " frames per i quali la SSIM era non nulla = " << avgSSIM << std::endl;

    cvReleaseCapture(&video1);
    cvReleaseCapture(&video2);
    cvReleaseCapture(&maskingVideo);
}
