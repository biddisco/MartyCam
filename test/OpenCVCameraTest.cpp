#include <stdio.h>
#include <opencv2/highgui.hpp>
#include <opencv2/videoio/videoio_c.h>
#include <opencv2/highgui/highgui_c.h>

cv::VideoCapture capture;

int main(int argc, char **argv){

    cv::Size size(1024, 768);

    int index = 1;

    capture.set(CV_CAP_PROP_FRAME_WIDTH,  size.width);
    capture.set(CV_CAP_PROP_FRAME_HEIGHT, size.height);
    if (capture.isOpened()) {
      capture.release();
    }
    capture.open(index);
    capture.set(CV_CAP_PROP_FPS, 10);
    std::ostringstream output;
    output << "CV_CAP_PROP_FRAME_WIDTH\t"   << capture.get(CV_CAP_PROP_FRAME_WIDTH) << std::endl;
    output << "CV_CAP_PROP_FRAME_HEIGHT\t"  << capture.get(CV_CAP_PROP_FRAME_HEIGHT) << std::endl;
    output << "CV_CAP_PROP_FPS\t"           << capture.get(CV_CAP_PROP_FPS) << std::endl;
    output << "CV_CAP_PROP_FOURCC\t"        << capture.get(CV_CAP_PROP_FOURCC) << std::endl;
    output << "CV_CAP_PROP_BRIGHTNESS\t"    << capture.get(CV_CAP_PROP_BRIGHTNESS) << std::endl;
    output << "CV_CAP_PROP_CONTRAST\t"      << capture.get(CV_CAP_PROP_CONTRAST) << std::endl;
    output << "CV_CAP_PROP_SATURATION\t"    << capture.get(CV_CAP_PROP_SATURATION) << std::endl;
    output << "CV_CAP_PROP_HUE\t"           << capture.get(CV_CAP_PROP_HUE) << std::endl;

//    CvCapture *camera=cvCaptureFromFile("http://username:pass@cam_address/axis-cgi/mjpg/video.cgi?resolution=640x480&req_fps=30&.mjpg");
//    CvCapture *camera=cvCaptureFromFile("http://192.168.1.21/videostream.asf?user=admin&pwd=1234&req_fps=30&.mjpg");
//    CvCapture *camera=cvCaptureFromFile("http://admin:1234@192.168.1.21/videostream.cgi?req_fps=30&.mjpg");

    printf("Reading from any camera\n");

//    CvCapture* camera = cvCaptureFromCAM(CV_CAP_ANY); //Capture using any camera connected to your system
//    CvCapture* camera = cvCaptureFromCAM(1); //Capture using any camera connected to your system
    // get latest frame from webcam


//    if (camera==NULL)
//        printf("camera is null\n");
//    else
//        printf("camera is not null");

    cv::namedWindow("img", 1);
    while (cvWaitKey(10)!=atoi("q")){
        double t1=(double)cvGetTickCount();

        cv::Mat frame;
        capture >> frame;

//        IplImage *img=cvQueryFrame(camera);
        double t2=(double)cvGetTickCount();
        printf("time: %gms  fps: %.2g\n",(t2-t1)/(cvGetTickFrequency()*1000.), 1000./((t2-t1)/(cvGetTickFrequency()*1000.)));
//        cvShowImage("img",img);
//        cvShowImage("img",frame.);
        cv::imshow("img", frame);
    }
//    cvReleaseCapture(&camera);
}

