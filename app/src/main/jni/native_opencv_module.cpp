#include <jni.h>

#include <android/log.h>
#include <android/bitmap.h>

#include "opencv2/imgproc.hpp"
#include "opencv2/video.hpp"

#define  LOG_TAG    "Optical Flow"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)



#ifdef __cplusplus
extern "C" {
#endif


/* Global Parameters */

// Previous frame
cv::Mat previous_gray_frame;

// Keypoints
std::vector<cv::Point2f> previous_features, current_features;

const int MAX_CORNERS = 500;
// maxCorners The maximum number of corners to return. If there are more corners than that will
// be found, the strongest of them will be returned

const int win_size = 5;

// cv::cornerSubPix
// win_size - Half of the side length of the search window. For example, if winSize=Size(5,5),
//        then a 5  2 + 1  5  2 + 1 = 11  11 search window would be used

JNIEXPORT void JNICALL
Java_ph_edu_dlsu_opticalflow_CameraActivity_process(JNIEnv *env, jobject instance,
                                                            jobject pTarget, jbyteArray pSource, jint levels) {
    //long t;
    cv::Mat srcBGR;
    std::vector<std::vector<cv::Point> > contours;
    std::vector<cv::Vec4i> hierarchy;

    cv::RNG randnum(12345); // for random color
    cv::Scalar color;

    AndroidBitmapInfo bitmapInfo;
    uint32_t* bitmapContent;

    if(AndroidBitmap_getInfo(env, pTarget, &bitmapInfo) < 0) abort();
    if(bitmapInfo.format != ANDROID_BITMAP_FORMAT_RGBA_8888) abort();
    if(AndroidBitmap_lockPixels(env, pTarget, (void**)&bitmapContent) < 0) abort();

    /// Access source array data... OK
    jbyte* source = (jbyte*)env->GetPrimitiveArrayCritical(pSource, 0);
    if (source == NULL) abort();

    /// cv::Mat for YUV420sp source and output BGRA
    cv::Mat srcGray(bitmapInfo.height, bitmapInfo.width, CV_8UC1, (unsigned char *)source);
    cv::Mat src(bitmapInfo.height + bitmapInfo.height/2, bitmapInfo.width, CV_8UC1, (unsigned char *)source);
    cv::Mat mbgra(bitmapInfo.height, bitmapInfo.width, CV_8UC4, (unsigned char *)bitmapContent);


/***********************************************************************************************/
    /// Native Image Processing HERE...

    //t = cv::getTickCount();

    if(srcBGR.empty())
        srcBGR = cv::Mat(bitmapInfo.height, bitmapInfo.width, CV_8UC3);

    // RGB frame input
    cv::cvtColor(src, srcBGR, CV_YUV420sp2RGB);

    /// If previous frame doesn't exist yet, initialize to srcGray
    if(previous_gray_frame.empty()){
        srcGray.copyTo(previous_gray_frame);
        LOGI("Initializing previous frame...");
    }

    // Detect the strong corners on an image.
    cv::goodFeaturesToTrack (previous_gray_frame, previous_features,
                             MAX_CORNERS, 0.05, 5, cv::noArray(), 3, false, 0.04);

   // Refines the corner locations
    cornerSubPix (previous_gray_frame, previous_features,
                  cv::Size(win_size, win_size), cv::Size(-1,-1),
                  cv::TermCriteria (CV_TERMCRIT_ITER |CV_TERMCRIT_EPS ,20,0.03));

    std::vector<uchar> features_found;
    // The output status vector. Each element of the vector is set to 1 if the flow for
    // the corresponding features has been found, 0 otherwise

    // Calculates the optical flow for a sparse feature set using the iterative Lucas-Kanade method with
    // pyramids
    cv::calcOpticalFlowPyrLK (previous_gray_frame, srcGray,
                          previous_features, current_features, features_found,
                          cv::noArray(),cv::Size(win_size*4+1,win_size*4+1), 0,
            cv::TermCriteria (CV_TERMCRIT_ITER |CV_TERMCRIT_EPS ,20,0.3));

    for( int i = 0; i < (int)previous_features.size(); i++ )
    {
        if(features_found[i]) {
            // Randomize color and display the velocity vectors
            color = cv::Scalar(randnum.uniform(0, 255), randnum.uniform(0,255), randnum.uniform(0,255) );
            line(srcBGR, previous_features[i], current_features[i], color);
        }
    }


    //LOGI("Processing took %0.2f ms.", 1000*(cv::getTickCount() - t)/(float)cv::getTickFrequency());

    cvtColor(srcBGR, mbgra, CV_BGR2BGRA);

    // Copy the current gray frame into previous_gray_frame
    srcGray.copyTo(previous_gray_frame);

/************************************************************************************************/

    /// Release Java byte buffer and unlock backing bitmap
    //env-> ReleasePrimitiveArrayCritical(pSource,source,0);
    /*
     * If 0, then JNI should copy the modified array back into the initial Java
     * array and tell JNI to release its temporary memory buffer.
     *
     * */

    env-> ReleasePrimitiveArrayCritical(pSource, source, JNI_COMMIT);
    /*
 * If JNI_COMMIT, then JNI should copy the modified array back into the
 * initial array but without releasing the memory. That way, the client code
 * can transmit the result back to Java while still pursuing its work on the
 * memory buffer
 *
 * */

    /*
     * Get<Primitive>ArrayCritical() and Release<Primitive>ArrayCritical()
     * are similar to Get<Primitive>ArrayElements() and Release<Primitive>ArrayElements()
     * but are only available to provide a direct access to the target array
     * (instead of a copy). In exchange, the caller must not perform blocking
     * or JNI calls and should not hold the array for a long time
     *
     */


    if (AndroidBitmap_unlockPixels(env, pTarget) < 0) abort();
}


#ifdef __cplusplus
}
#endif
