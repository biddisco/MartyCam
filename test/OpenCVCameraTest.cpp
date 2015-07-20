#include <stdio.h>
#include <opencv2/highgui.hpp>

int main(int argc, char **argv){

//    CvCapture *camera=cvCaptureFromFile("http://username:pass@cam_address/axis-cgi/mjpg/video.cgi?resolution=640x480&req_fps=30&.mjpg");
//    CvCapture *camera=cvCaptureFromFile("http://192.168.1.21/videostream.asf?user=admin&pwd=1234&req_fps=30&.mjpg");
//    CvCapture *camera=cvCaptureFromFile("http://admin:1234@192.168.1.21/videostream.cgi?req_fps=30&.mjpg");

    printf("Reading from any camera\n");

    CvCapture* camera = cvCaptureFromCAM(CV_CAP_ANY); //Capture using any camera connected to your system
    if (camera==NULL)
        printf("camera is null\n");
    else
        printf("camera is not null");

    cvNamedWindow("img");
    while (cvWaitKey(10)!=atoi("q")){
        double t1=(double)cvGetTickCount();
        IplImage *img=cvQueryFrame(camera);
        double t2=(double)cvGetTickCount();
        printf("time: %gms  fps: %.2g\n",(t2-t1)/(cvGetTickFrequency()*1000.), 1000./((t2-t1)/(cvGetTickFrequency()*1000.)));
        cvShowImage("img",img);
    }
    cvReleaseCapture(&camera);
}

