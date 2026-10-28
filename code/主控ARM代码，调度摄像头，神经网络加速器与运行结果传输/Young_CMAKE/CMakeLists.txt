cmake_minimum_required( VERSION 2.8 )
project(USB-GAZE-CMAKE)
#
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast")
add_compile_options(-std=c++11 -Ofast)
FIND_PACKAGE(Threads)
if(THREADS_FOUND)
   message("THREADS FOUND")
endif()
add_definitions(-D_ENABLE_NEON)
FIND_PACKAGE(OpenMP)
if(OPENMP_FOUND)
	message("OPENMP FOUND")
	message(STATUS "OPENMP:=${OpenMP_CXX_FLAGS}")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()
# 设置源文件列表
set(SOURCE_FILES
    main.cpp
    GazeWorker.cpp
    get_eye_roi.cpp
)

# 设置头文件列表
set(HEADERS
    facedetectcnn.h
    facedetection_export.h
    GazeWorker.h
    get_eye_roi.h
)

set(OpenCV_DIR /home/wjj/project/sdk_work/base_petalinux1/opencv_path/alinx_opencv_3.1.0/zynq_src_tmp/opencv-3.4.1/build)
set(Libface_DIR /home/wjj/project/sdk_work/base_petalinux1/libface_test/libfacedetection-master)
find_package( OpenCV REQUIRED )



# ==============================
# JPEG 库配置（新增部分）
# ==============================
# 包含路径 - 对应 INCLUDEPATH
include_directories(/home/wjj/桌面/turbo_jpeg/include)


# 库路径 - 对应 QMAKE_LIBDIR_FLAGS
link_directories(/home/wjj/桌面/turbo_jpeg/lib)


message(STATUS "OpenCV_LIBS = ${OpenCV_LIBS}")
include_directories( ${OpenCV_INCLUDE_DIRS} )
#include_directories( ${OpenCV_Lib_DIR/../include/opencv2} )
include_directories( ${PROJECT_SOURCE_DIR} )

add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${HEADERS})
# an5640 的链接库
target_link_libraries(${PROJECT_NAME} 
    ${OpenCV_LIBS}
    ${Libface_DIR}/build/install/lib/libfacedetection.so
    ${OpenCV_DIR}/lib/libopencv_highgui.so
    ${OpenCV_DIR}/lib/libopencv_imgproc.so
    ${OpenCV_DIR}/lib/libopencv_core.so
    ${OpenCV_DIR}/lib/libopencv_imgcodecs.so
    ${OpenCV_DIR}/lib/libopencv_video.so
    Threads::Threads
    # JPEG 库（新增）
    jpeg
    turbojpeg
)
