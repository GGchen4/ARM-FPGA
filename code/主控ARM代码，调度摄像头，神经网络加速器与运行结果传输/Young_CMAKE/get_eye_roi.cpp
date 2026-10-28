#include "get_eye_roi.h"
//**********相机参数设置*********
//image_width: 640
//image_height: 480
//相机内参矩阵（3x3）
//    rows: 3
//    cols: 3
Mat camera_matrix = (cv::Mat_<double>(3,3) <<
                      640, 0,   320 ,    // fx=640, cx=320（焦距和光心x坐标）
                      0,   640, 240,     // fy=640, cy=240（焦距和光心y坐标）
                      0,   0,   1  );    // 齐次坐标
// 畸变系数（k1,k2,p1,p2,k3）, 无畸变
Mat dist_coeffs = (cv::Mat_<double>(5,1) << 0, 0, 0, 0, 0);
// 归一化相机矩阵（用于输出图像变换）
Mat normalized_cam = (cv::Mat_<double>(3,3) <<
                      960, 0, 30,        // 缩放后的新焦距和偏移
                      0, 960, 18,
                      0, 0, 1);
// 3D人脸模型关键点坐标（单位：米）
vector<Point3f> model_points =  {
    cv::Point3f(-0.03210, -0.03172, 0.03668),   // left eye（x,y,z）
    cv::Point3f(0.03210, -0.03172, 0.03668),   // right eye
    cv::Point3f(0.00000, 0.01382, 0.01206),  // noise
    cv::Point3f(-0.02847, 0.03332, 0.03668), // left mouth
    cv::Point3f(0.02847, 0.03332, 0.03668)  // right mouth
};
// 预定义特征点索引（根据实际模型调整）
const vector<int> REYE_INDICES = {1}; // 右眼特征点索引
const vector<int> LEYE_INDICES = {0}; // 左眼特征点索引
//const vector<int> MOUTH_INDICES = {48, 54};// 嘴部特征点索引 1-->2
// 计算3D点集的几何中心
Point3f compute3DCenter(const vector<Point3f>& points, const vector<int>& indices) {
    Point3f center(0, 0, 0);
    for (int idx : indices) {
        // 累加指定索引点的坐标
        center += points[idx];
    }
    // 返回平均值
    return center * (1.0f / indices.size());
}
// 生成Z轴缩放矩阵（基于距离调整深度）
Mat getScaleMatrix(double distance) {
        // 单位矩阵
        cv::Mat scale = cv::Mat::eye(3, 3, CV_64F);
         // 缩放Z轴分量
        scale.at<double>(2, 2) = 0.9 / distance;
        return scale;
    }
// 主函数：提取眼睛ROI
Mat extractEyeROI(
    float& eye_center_x,
    float& eye_center_y,
    float& eye_center_z,
    // 输入图
    const Mat& image,
    // 2D特征点
    const vector<Point2f>& image_points,
    // 3D模型点
    const vector<Point3f>& model_points,
    // 相机内参
    const Mat& camera_matrix,
    // 畸变系数
    const Mat& dist_coeffs,
    // 归一化相机参数
    const Mat& normalized_cam_matrix,
    // 输出尺寸（60x36）
    Size output_size,
    Mat& out_normalizing_rot
) {
    // 0. 初始化姿态估计参数 (必须使用双精度)
    Mat rvec = Mat::zeros(3, 1, CV_64FC1); // 旋转向量
    Mat tvec = (Mat_<double>(3, 1) << 0, 0, 1); // 平移向量，Z=1
    // Mat tvec = Mat::zeros(3, 1, CV_64FC1); // 平移向量
    // 1. 使用solvePnP求解3D-2D对应点姿态估计
    solvePnP(model_points, image_points, camera_matrix, dist_coeffs,
             rvec, tvec, true, SOLVEPNP_ITERATIVE);
    // 2. 将旋转向量转换为旋转矩阵
    Mat rot_mat;
    Rodrigues(rvec, rot_mat);
    //cout<<"rvec:  "<<rvec<<endl;

    // 5. 转换3D模型点到相机坐标系
    vector<Point3f> model3d;
    for (const Point3f& pt : model_points) {
        Mat pt_mat = (Mat_<double>(3,1) << pt.x, pt.y, pt.z);
        // Mat rotated = rot_mat.t() * pt_mat; // 应用旋转转置
        Mat rotated = rot_mat * pt_mat; // 应用旋转---------change
        Mat translated = rotated + tvec; // 应用平移
        // 存储转换后的3D点
        model3d.emplace_back(translated.at<double>(0),
                            translated.at<double>(1),
                            translated.at<double>(2));
    }
    // 6. 计算左右眼中心3D坐标（相机坐标系下）
    Point3f reye_center = compute3DCenter(model3d, REYE_INDICES);
    Point3f leye_center = compute3DCenter(model3d, LEYE_INDICES);
    //cout<<"right eye "<<reye_center<<"leye_center: "<<leye_center<<endl;

    // if (sender != nullptr) {
    //         //Point3f reye_center = compute3DCenter(model3d, REYE_INDICES);
    //         //Point3f leye_center = compute3DCenter(model3d, LEYE_INDICES);
    //         sender->sendEyeCoords(reye_center, leye_center);
    //     }

    // 7. 构建注视方向向量（以左眼为中心）
    // 计算注视方向向量
    eye_center_x = leye_center.x;
    eye_center_y = leye_center.y;
    eye_center_z = leye_center.z;
    Mat gaze_vec = (Mat_<double>(3,1) << leye_center.x,
                                        leye_center.y,
                                        leye_center.z);
//    cout<<"gaze_vec "<<gaze_vec<<endl;
    // 8. 计算缩放因子（基于到眼睛的距离）
    double distance = norm(gaze_vec);// 计算模长
    Mat scale_mat = getScaleMatrix(distance); // 生成缩放矩阵
//    cout<<"distance: "<<distance<<" scale_mat: "<<scale_mat<<endl;

    // 9. 计算相机内参的逆矩阵
    Mat cam_inv;
    invert(camera_matrix, cam_inv, DECOMP_LU);
      // 10. 计算归一化旋转矩阵
    Mat normalizing_rot = computeNormalizingRotation(gaze_vec, rot_mat);

        // 3. 计算欧拉角（头部姿态）
    // cv::Vec3d head_pose = rotationMatrixToEulerAngles(rot_mat);
    Mat normalized_head_rot = rot_mat * normalizing_rot;
    cv::Vec3d head_pose = rotationMatrixToEulerAngles(normalized_head_rot);
    //cout <<float_to_int16(head_pose[0], 15)<< " :[0]head pose[1]: "<<float_to_int16(head_pose[1], 15)<<endl;
    //Send head pose to DDR at 0x11000000+0x01000000
    // 4. 将姿态数据写入硬件寄存器
    write_register(0x12000000, float_to_int16(head_pose[0], 15));
    write_register(0x12000004, float_to_int16(head_pose[1], 15));


        //output  normalizing_ro
    out_normalizing_rot = normalizing_rot.clone();
    // 11. 构建完整的投影矩阵
    Mat conversion_matrix = scale_mat * normalizing_rot;
    Mat proj_mat = normalized_cam_matrix * conversion_matrix * cam_inv;
     // 12. 执行透视变换获取ROI
    Mat normalized_img;
//    imwrite("debug_input.jpg", image);
    warpPerspective(image, normalized_img, proj_mat, output_size);
//    imwrite("eye_roi.jpg", normalized_img); // 保存原始图像用于验证
    return normalized_img;// 返回归一化的眼睛图像
}

// 向量归一化（单位化）
Mat normalizeVector(const Mat& vector) {
//    if (vector.empty() || vector.total() != 3) {
//        throw invalid_argument("Input vector must be 3-element vector");
//    }
    double norm = cv::norm(vector);
//    if (norm < std::numeric_limits<double>::epsilon()) {
//        throw runtime_error("Cannot normalize zero vector");
//    }
    return vector / norm;// 各分量除以模长
}

// 计算归一化旋转矩阵
Mat computeNormalizingRotation(const Mat& center, const Mat& headRotation) {
    // 输入验证
    if (center.size() != Size(1,3) && center.size() != Size(3,1)) {
        throw invalid_argument("Center must be 3-element vector");
    }

    if (headRotation.size() != Size(3,3) || headRotation.type() != CV_64F) {
        throw invalid_argument("Head rotation must be 3x3 double matrix");
    }

    try {
        // 1. 计算Z轴（注视方向）
        Mat zAxis = normalizeVector(center.reshape(1, 3)); // 确保3x1形状
        // 2. 提取头部原始X轴（旋转矩阵第一列）
        Mat headXAxis;
        headRotation.col(0).copyTo(headXAxis);
         // 3. 计算新Y轴（Z轴与头部X轴的叉积）
        Mat yAxis = normalizeVector(zAxis.cross(headXAxis));
         // 4. 计算新X轴（Y轴与Z轴的叉积）
        Mat xAxis = normalizeVector(yAxis.cross(zAxis));
         // 5. 构建旋转矩阵
        Mat rotationMatrix(3, 3, CV_64F);
        xAxis.reshape(1, 1).copyTo(rotationMatrix.row(0)); // 新X轴
        yAxis.reshape(1, 1).copyTo(rotationMatrix.row(1)); // 新Y轴
        zAxis.reshape(1, 1).copyTo(rotationMatrix.row(2)); // 新Z轴
         // 6. 正交性验证（确保是合法旋转矩阵）
        Mat shouldBeIdentity = rotationMatrix * rotationMatrix.t();
        if (cv::norm(shouldBeIdentity - Mat::eye(3,3,CV_64F)) > 1e-6) {
            throw runtime_error("Result is not orthogonal rotation matrix");
        }
        return rotationMatrix;
    } catch (const cv::Exception& e) {
        throw runtime_error(string("OpenCV exception: ") + e.what());
    }
}
// 旋转矩阵转欧拉角（pitch,yaw,roll）
cv::Vec3d rotationMatrixToEulerAngles(const cv::Mat& R) {
    CV_Assert(R.rows == 3 && R.cols == 3);

    double sy = std::sqrt(R.at<double>(0, 0) * R.at<double>(0, 0) + R.at<double>(1, 0) * R.at<double>(1, 0));// 计算绕Y轴的sin值

    bool singular = sy < 1e-6;// 判断是否处于万向锁状态

    double x, y, z;
    if (!singular) {
        x = std::atan2(R.at<double>(2, 1), R.at<double>(2, 2)); // pitch
        y = std::atan2(-R.at<double>(2, 0), sy);                // yaw
        z = std::atan2(R.at<double>(1, 0), R.at<double>(0, 0)); // roll
    } else {// 万向锁时的处理
        x = std::atan2(-R.at<double>(1, 2), R.at<double>(1, 1));
        y = std::atan2(-R.at<double>(2, 0), sy);
        z = 0;
    }

    return cv::Vec3d(x, y, z);// 返回弧度值
}
