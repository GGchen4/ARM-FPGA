#ifndef GET_EYE_ROI_H
#define GET_EYE_ROI_H

#include <opencv2/opencv.hpp>
#include <vector>
#include "GazeWorker.h"
using namespace cv;
using namespace std;

Point3f compute3DCenter(const vector<Point3f>& points, const vector<int>& indices);
Mat getScaleMatrix(double distance);
Mat extractEyeROI(
    float& eye_center_x,
    float& eye_center_y,
    float& eye_center_z,
    const Mat& image,
    const vector<Point2f>& image_points,
    const vector<Point3f>& model_points,
    const Mat& camera_matrix,
    const Mat& dist_coeffs,
    const Mat& normalized_cam_matrix,
    Size output_size,
    Mat& out_normalizing_rot
);
cv::Vec3d rotationMatrixToEulerAngles(const cv::Mat& R);
//Mat extractEyeROI(Mat& image,
//                 vector<Point2f>& image_points,
//                 vector<Point3f>& model_points,
//                 Mat& camera_matrix,
//                 Mat& dist_coeffs,
//                 Mat& normalized_cam,
//                 Size output_size);
//**********set camera matrix*********
//image_width: 640
//image_height: 480
//camera_matrix:
//    rows: 3
//    cols: 3
extern Mat camera_matrix;
extern Mat dist_coeffs;
extern Mat normalized_cam;
//set 3d face model point
extern vector<Point3f> model_points;

Mat normalizeVector(const Mat& vector);
Mat computeNormalizingRotation(const Mat& center, const Mat& headRotation);


#endif // GET_EYE_ROI_H
