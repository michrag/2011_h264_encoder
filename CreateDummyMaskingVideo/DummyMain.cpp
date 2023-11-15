
#include <iostream>

#include "cv.h"
#include "highgui.h"


static void createDummyMaskingVideo(CvCapture* capture, char* maskingVideoFile)
{

    int h = cvGetCaptureProperty(capture, CV_CAP_PROP_FRAME_HEIGHT);
    int w = cvGetCaptureProperty(capture, CV_CAP_PROP_FRAME_WIDTH);
    double fps = 15;

    CvSize size = cvSize(w, h);

    IplImage* mask = cvCreateImage(size, IPL_DEPTH_8U, 3);
    cvSet(mask, cvScalar(0, 0, 0, 0));

    for(int row = 0; row < h / 2; row++)
    {
        for(int col = 0; col < w / 2; col++)
        {
            cvSet2D(mask, row, col, cvScalar(255, 255, 255, 0));
        }
    }

    for(int row = h / 2; row < h; row++)
    {
        for(int col = w / 2; col < w; col++)
        {
            cvSet2D(mask, row, col, cvScalar(255, 255, 255, 0));
        }
    }


    CvVideoWriter* writer = cvCreateVideoWriter(
                                maskingVideoFile,
                                CV_FOURCC('D', 'I', 'V', 'X'),
                                fps,
                                size
                            );


    while(cvGrabFrame(capture))
    {
        cvWriteFrame(writer, mask);
    }

    cvReleaseImage(&mask);
    cvReleaseVideoWriter(&writer);
}


int main(int argc, char** argv)
{

    if(argc < 3)
    {
        std::cout << "parametri: <inputVideo> <outMaskingVideo>" << std::endl;
        std::cout << "nota: outMaskingVideo DEVE essere .avi" << std::endl;
        return -1;
    }

    char* inputVideoFile = argv[1];
    char* maskingVideoFile = argv[2];


    CvCapture* inputVideo = cvCreateFileCapture(inputVideoFile);

    if(!inputVideo)
    {
        std::cout << "ERRORE: input video non trovato!" << std::endl;
        return -1;
    }


    createDummyMaskingVideo(inputVideo, maskingVideoFile);

    cvReleaseCapture(&inputVideo);

    return 0;

}
