#include "main_window.h"
#include "CaptureThread.h"
#include "GazeWorker.h"

CaptureThread *captureThread  = nullptr;
GazeWorker *gazeWorker        = nullptr;
MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
{
    QGridLayout *pLayout = new QGridLayout(this); //创建一个网格布局(QGridLayout)
    QFrame *pHeaderMenu = new QFrame;	//create一个矩形框
    QHBoxLayout *pHeaderMenuLayout = new QHBoxLayout(pHeaderMenu);	//创建一个水平布局，并把它应用到pHeaderMenu对象上
    QLabel *pLogoName = new QLabel;	//create 一个标签对象，用于显示文本
    QLabel *pLogoImg = new QLabel;	//create 一个标签对象，用于显示图像
    pVideo0 = new QLabel;	//create 一个标签对象，用于显示视频流动态内容
    //pVideo1 = new QLabel;


    QWidget *pVideoBox0 = new QWidget;	//创建另一个QWidget对象
    //QWidget *pVideoBox1 = new QWidget;
    QHBoxLayout *pVideoBox0Layout0 = new QHBoxLayout(pVideoBox0);	//创建一个水平布局，并把它应用到pVideoBox0对象上。
    //QHBoxLayout *pVideoBox0Layout1 = new QHBoxLayout(pVideoBox1);

    //将控件按需要排版
    pLayout->addWidget(pHeaderMenu, 0, 0, 1, 1, Qt::AlignTop);  //change the widget numbers
    //    pLayout->addWidget(pVideo0, 1, 0);
    //    pLayout->addWidget(pVideo1, 1, 1);
    pLayout->addWidget(pVideoBox0, 1, 0);
    //pLayout->addWidget(pVideoBox1, 1, 1);
    pLayout->setRowStretch(1, 1);
    pLayout->setMargin(0);
    pLayout->setSpacing(0);

    pVideoBox0Layout0->addWidget(pVideo0);
    //pVideoBox0Layout1->addWidget(pVideo1);

    pHeaderMenuLayout->addWidget(pLogoImg);
    pHeaderMenuLayout->addWidget(pLogoName);
    pHeaderMenuLayout->addStretch(1);
    pHeaderMenuLayout->setMargin(0);
    pHeaderMenuLayout->setSpacing(0);

    pLogoName->setText(tr("withoutPL Test"));


    //设置样式
    pHeaderMenu->setObjectName("HeaderMenu");
    pVideoBox0->setObjectName("videobox");
    //pVideoBox1->setObjectName("videobox");
    pVideo0->setObjectName("videolabel");
    //pVideo1->setObjectName("videolabel");
    pLogoName->setObjectName("logoname");
    pLogoImg->setObjectName("cpuinfo");
	//设置窗口CSS的样式
    setStyleSheet("#videobox{background:black;}"
                  "#HeaderMenu{background:qlineargradient(spread:reflect,"
                  "x1:0.5, y1:0, x2:0.5, y2:1,"
                  "stop:0.0 rgba(80,80,80,255),"
                  "stop:0.6 rgba(34,34,34,255),"
                  "stop:1.0 rgba(25,25,25,255));"
                  "min-height:60px;"
                  "max-height:60px;"
                  "border:1px solid #000;}"
                  "#videolabel{border:1px solid #555;background:#2b2b2b;max-width:800px;max-height:600px;}"
                  "#logoname{"
                  "color:#aff;font-size:22px;background:#666;max-height:26px;"
                  "border:2px solid #222;"
                  "border-bottom:1px solid #888;"
                  "border-right:1px solid #888;"
                  "padding-left:3px;padding-right:3px;"
                  "}"
                  "#cpuinfo{border-image:url(../img/SysInfoBg.png);"
                  "min-width:50px;min-height:50px;"
                  "max-width:50px;max-height:50px;}");
    pLogoImg->setAlignment(Qt::AlignCenter);	//居中对齐

    //界面最大化
    showMaximized();

	// //这两行代码创建了两个新的catchImage对象，用于捕获和处理视频流。
    // pCatchVideo0 = new catchImage(0);
    // //run_elf_p = new run_elf();
    // //pCatchVideo1 = new catchImage(1);

	// //将catchiamge对象的senimage信号链接到updatepic槽函数上
    // QObject::connect(pCatchVideo0, SIGNAL(sendImg(QImage*)), this, SLOT(updatePic0(QImage*)));
    // //QObject::connect(pCatchVideo1, SIGNAL(sendImg(QImage*)), this, SLOT(updatePic1(QImage*)));
	// //启动`catchImage`对象的视频捕获和处理

    // pCatchVideo0->start();
    // //run_elf_p->start();// 启动线程，执行catchImage::run(),可能导致elf重复执行，请甄别其与actionELF的使用
    // //pCatchVideo1->start();

    dev_0.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    dev_0.mem_type = V4L2_MEMORY_MMAP;
    dev_0.format.pixelformat = V4L2_PIX_FMT_MJPEG;  // ← 改成 MJPEG !!!
    dev_0.format.width = IMAGEWIDTH;
    dev_0.format.height = IMAGEHEIGHT;
    dev_0.format.bytesperline = 0;  // MJPEG 不需要此字段，必须设为 0
    dev_0.format.colorspace = V4L2_COLORSPACE_SRGB;
    dev_0.format.field = V4L2_FIELD_NONE;

    strcpy(dev_0.devname, "/dev/video2");

    // if (v4l2_parse_node("uvcvideo", dev_0.devname)) {
    //     qDebug() << "no dev found";
    //     return;
    // }

    v4l2_init(&dev_0, BUFFER_CNT);
    // 1. 创建采集线程（只负责从 v4l2_dev 取 BGR 帧）
    captureThread = new CaptureThread(&dev_0, IMAGEWIDTH, IMAGEHEIGHT);
    // 2. 创建 worker 线程和 GazeWorker 对象
    workerThread = new QThread(this);
    gazeWorker   = new GazeWorker();   // 不指定 parent，交给 QThread 管
    gazeWorker->moveToThread(workerThread);
    // 3. 连接：采集线程 → GazeWorker（做检测 + gaze）
    connect(captureThread, &CaptureThread::frameReady,
            gazeWorker, &GazeWorker::processFrame,
            Qt::QueuedConnection);

    // 4. 连接：GazeWorker → MainWindow（显示）
    connect(gazeWorker, &GazeWorker::imageReady,
            this, &MainWindow::onImageReady,
            Qt::QueuedConnection);

    // 5. 线程结束时清理 worker（这里可选，你也可以在析构里 delete）
    connect(workerThread, &QThread::finished,
            gazeWorker, &QObject::deleteLater);
    // 6. 启动线程
    workerThread->start();
    captureThread->start();
    // connect(captureThread, &CaptureThread::frameReady, this,
    //         [&](const cv::Mat &frame){
    //             processFrame(frame);  // <-- 使用新逻辑
    //         });

    // captureThread->start();


}

MainWindow::~MainWindow()
{
    if (captureThread) {
        captureThread->stop();
        captureThread->wait();   // 等待线程安全退出
        delete captureThread;
    }
    delete ui;
    delete timer;

}

void MainWindow::updatePic0(QImage *pImg)
{
    if(pImg && !pImg->isNull()){
        QPixmap pix = QPixmap::fromImage(*pImg);
        if(!pix.isNull()){
            pVideo0->setPixmap(QPixmap::fromImage(*pImg).scaled(pVideo0->width()-2, pVideo0->height()-2));
        }else{
            qWarning() << "Received null QPixmap";
        }
    }else{
        qWarning() << "Received invalid QImage pointer";
    }
    delete pImg; // 确保释放内存
    //pVideo0->setPixmap(QPixmap::fromImage(*pImg).scaled(pVideo0->width()-2, pVideo0->height()-2));
    //delete pImg;
}

void MainWindow::updatePic1(QImage *pImg)
{
    pVideo1->setPixmap(QPixmap::fromImage(*pImg).scaled(pVideo1->width()-2, pVideo1->height()-2));
    delete pImg;
}

void MainWindow::play_change(bool)
{
    QPushButton *pBut = qobject_cast<QPushButton *>(sender());

    if(pBut->isFlat())
    {
        pBut->setFlat(false);
    }
    else
    {
        pBut->setFlat(true);
    }
    pBut->setStyleSheet(pBut->styleSheet());
}
