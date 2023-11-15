#include "cv.h"
#include "highgui.h"

#define ALLCHANNEL -1

double calcSSIM(cv::Mat& src1, cv::Mat& src2, int channel = 0, int method = CV_BGR2YCrCb, cv::Mat mask = cv::Mat(), const double K1 = 0.01, const double K2 = 0.03, const int L = 255, const int downsamplewidth = 256, const int gaussian_window = 11, const double gaussian_sigma = 1.5, cv::Mat ssim_map = cv::Mat());
double calcSSIMBB(cv::Mat& src1, cv::Mat& src2, int channel = 0, int method = CV_BGR2YCrCb, int boundx = 0, int boundy = 0, const double K1 = 0.01, const double K2 = 0.03, const int L = 255, const int downsamplewidth = 256, const int gaussian_window = 11, const double gaussian_sigma = 1.5, cv::Mat ssim_map = cv::Mat());

double calcDSSIM(cv::Mat& src1, cv::Mat& src2, int channel = 0, int method = CV_BGR2YCrCb, cv::Mat mask = cv::Mat(), const double K1 = 0.01, const double K2 = 0.03, const int L = 255, const int downsamplewidth = 256, const int gaussian_window = 11, const double gaussian_sigma = 1.5, cv::Mat ssim_map = cv::Mat());
double calcDSSIMBB(cv::Mat& src1, cv::Mat& src2, int channel = 0, int method = CV_BGR2YCrCb, int boundx = 0, int boundy = 0, const double K1 = 0.01, const double K2 = 0.03,    const int L = 255, const int downsamplewidth = 256, const int gaussian_window = 11, const double gaussian_sigma = 1.5, cv::Mat ssim_map = cv::Mat());
