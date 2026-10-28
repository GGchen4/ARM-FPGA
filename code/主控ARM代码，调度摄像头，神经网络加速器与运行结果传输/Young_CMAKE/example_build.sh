SDK_PATH=/opt/SDK/install_path/SDK/2017.4
if [ ! -d $SDK_PATH ]; then
  echo "no dir ${SDK_PATH}"
  exit 1
fi

source ${SDK_PATH}/settings64.sh 
export PKG_CONFIG_LIBDIR=${SDK_PATH}/gnu/aarch32/lin/gcc-arm-linux-gnueabi/arm-linux-gnueabihf/lib
export CROSS_COMPILE=arm-linux-gnueabihf
mkdir build
cd build
echo "set( CMAKE_SYSTEM_NAME Linux )" > toolchain.make
echo "set( CMAKE_SYSTEM_PROCESSOR arm )" >> toolchain.make
echo "set( CMAKE_C_COMPILER ${CROSS_COMPILE}-gcc )" >> toolchain.make
echo "set( CMAKE_CXX_COMPILER ${CROSS_COMPILE}-g++ )" >> toolchain.make
echo "set( CMAKE_INSTALL_PREFIX $ZYNQ_CV_BUILD )" >> toolchain.make
echo "set( CMAKE_FIND_ROOT_PATH $ZYNQ_CV_BUILD )" >> toolchain.make
echo "set( CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER )" >> toolchain.make
echo "set( CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY )" >> toolchain.make
echo "set( CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY )" >> toolchain.make

cmake -D CMAKE_TOOLCHAIN_FILE=toolchain.make -DUSE_OPENMP=ON -DENABLE_NEON=ON -DENABLE_AVX2=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS:STRING="-march=armv7-a -mfloat-abi=hard -mfpu=neon -mtune=cortex-a9 -Wno-psabi" -DCMAKE_C_FLAGS:STRING="-march=armv7-a -mfloat-abi=hard -mfpu=neon -mtune=cortex-a9 -Wno-psabi" -DCMAKE_ASM_FLAGS:STRING="-march=armv7-a -mfloat-abi=hard -mfpu=neon -mtune=cortex-a9 -g" ..
make
