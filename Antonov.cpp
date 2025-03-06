#include "Antonov.h"
#include "ui_Antonov.h"
#include <QDockWidget>
#include <QVBoxLayout>
#include <QDateTime>
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QGraphicsLineItem>
#include <QGraphicsEllipseItem>
#include <cmath>
#include <random>
#include <QInputDialog>
#include <QLocale>
#include <QInputMethod>
#include <stack>
#include <QApplication>
#include <QPen>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsPolygonItem>
#include <QGraphicsPathItem>
#include <QPainterPath>

Antonov::Antonov(QWidget* parent) :
    QMainWindow(parent),
    ui(new Ui::AntonovClass),
    programProgress(0),
    programRunning(false),
    inInches(false),
    currentProgramRow(0),
    spindleSpeed(1000),
    simulationTimer(new QTimer(this)),
    currentStep(0),
    feedRate(100),
    feedRateMultiplier(1.0),
    spindleSpeedMultiplier(1.0),
    xValueCurrent(0),
    yValueCurrent(0),
    zValueCurrent(0),
    xValueFinal(10),
    yValueFinal(10),
    zValueFinal(10),
    coordinateSystem(QStringLiteral("СКС")),
    interfaceLanguage(QStringLiteral("Русский")),
    startFromRowEnabled(false),
    scene(new QGraphicsScene(this))
{
    ui->setupUi(this);
    ui->graphView_simul->setScene(scene);

    // Настройка док-виджетов
    QDockWidget* mainDockWidget = new QDockWidget(tr(""), this);
    mainDockWidget->setObjectName("MainDockWidget");
    QWidget* mainWidget = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);
    QTimer::singleShot(500, this, SLOT(executeNextStep()));
    toolTimer = new QTimer(this);
    stepTimer = new QTimer(this);
    QList<QDockWidget*> dockWidgets = {
        ui->dockWidget, ui->dockWidget_2, ui->dockWidget_3,
        ui->dockWidget_4, ui->dockWidget_5, ui->dockWidget_6,
        ui->dockWidget_7, ui->dockWidget_8
    };

    for (QDockWidget* dock : dockWidgets) {
        connect(dock, &QDockWidget::visibilityChanged, this, [this, dock](bool visible) {
            if (!visible) {
                dock->setFloating(false);
                addDockWidget(Qt::LeftDockWidgetArea, dock);
                dock->show();
            }
            });
    }

    mainLayout->addWidget(ui->stackedWidget);
    mainWidget->setLayout(mainLayout);
    mainDockWidget->setWidget(mainWidget);
    addDockWidget(Qt::TopDockWidgetArea, mainDockWidget);
    setDockNestingEnabled(true);
    pathItem = new QGraphicsPathItem();
    pathItem->setPath(toolPath);
    pathItem->setPen(QPen(Qt::red, 2));
    scene->addItem(pathItem);
    scene = new QGraphicsScene(this);
    moveTimer = new QTimer(this);
    ui->graphView_simul->setScene(scene);

    // Подключение сигналов и слотов для элементов интерфейса
    connect(ui->slider_spindle_speed, &QSlider::valueChanged, this, &Antonov::handleSpindleSpeedChange);
    connect(ui->slider_feed_rate, &QSlider::valueChanged, this, &Antonov::handleFeedRateChange);

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(updateTime()));
    connect(toolTimer, SIGNAL(timeout()), this, SLOT(updateToolStep()));
    connect(stepTimer, &QTimer::timeout, this, &Antonov::executeNextStep);
    timer->start(1000);

    connect(ui->button_numeration, &QPushButton::clicked, this, &Antonov::handleNumeration);
    connect(ui->button_smenaekrana, &QPushButton::clicked, this, &Antonov::handleSmenaEkrana);
    connect(ui->button_mmdyum, &QPushButton::clicked, this, &Antonov::handleMmDyum);
    connect(ui->button_changesk, &QPushButton::clicked, this, &Antonov::handleChangeSK);
    connect(ui->button_priv, &QPushButton::clicked, this, &Antonov::handlePriv);
    connect(ui->button_startkadr, &QPushButton::clicked, this, &Antonov::handleStartKadr);
    connect(ui->button_korrekt, &QPushButton::clicked, this, &Antonov::handleKorrekt);
    connect(ui->button_smesh, &QPushButton::clicked, this, &Antonov::handleSmesh);
    connect(ui->button_selectprog, &QPushButton::clicked, this, &Antonov::loadProgram);
    connect(ui->button_back, &QPushButton::clicked, this, &Antonov::handleBack);
    connect(ui->commandLinkButton_stop, &QPushButton::clicked, this, &Antonov::handleStop);
    connect(ui->commandLinkButton_start, &QPushButton::clicked, this, &Antonov::handleStart);
    connect(ui->commandLinkButton_reset, &QPushButton::clicked, this, &Antonov::handleReset);
    connect(ui->commandLinkButton_resetalarms, &QPushButton::clicked, this, &Antonov::handleResetAlarm);
    connect(ui->button_korrekt, &QPushButton::clicked, this, &Antonov::switchToCorrectionScreen);
    connect(ui->button_backk2, &QPushButton::clicked, this, &Antonov::handleBackK2);
    connect(ui->button_upwiget2, &QPushButton::clicked, this, &Antonov::handleCorrectionUp);
    connect(ui->button_downwiget2, &QPushButton::clicked, this, &Antonov::handleCorrectionDown);
    connect(ui->button_leftwiget2, &QPushButton::clicked, this, &Antonov::handleMoveLeft);
    connect(ui->button_rightwiget2, &QPushButton::clicked, this, &Antonov::handleMoveRight);
    connect(ui->button_wiborwiget2, &QPushButton::clicked, this, &Antonov::handleCorrectionSelect);
    connect(ui->button_podtverkorrert, &QPushButton::clicked, this, &Antonov::handleCorrectionConfirm);
    connect(ui->button_otmenakorrert, &QPushButton::clicked, this, &Antonov::handleCorrectionCancel);

    updateCoordinatesDisplay();
    updateUnits();
    updateProgramStatus();
}


Antonov::~Antonov() {
    if (pathItem) {
        delete pathItem;
        pathItem = nullptr;
    }
    if (tool) {
        delete tool;
        tool = nullptr;
    }
    if (scene) {
        delete scene;
        scene = nullptr;
    }
    delete ui;
}

// Логика отдельных компонентов (дата и время, ползунки, проверка ошибок и тд...)

void Antonov::updateProgramStatus() {
    if (loadedProgram.isEmpty()) {
        ui->label_sostoyanine->setText("WAIT FOR LOADING");
        ui->label_sostoyanine_4->setText("WAIT FOR LOADING");
    }
    else {
        bool errors = analyzeProgramForErrors();
        if (errors) {
            ui->label_sostoyanine->setText("UNABLE TO START");
            ui->label_sostoyanine_4->setText("UNABLE TO START");
        }
        else {
            ui->label_sostoyanine->setText("READY TO START");
            ui->label_sostoyanine_4->setText("READY TO START");
        }
    }
}

void Antonov::updateProgressBar() {
    if (programRunning && currentProgramRow < loadedProgram.size()) {
        int progressPercentage = static_cast<int>((static_cast<double>(currentProgramRow) / loadedProgram.size()) * 100);
        updateStatusBar(progressPercentage);
        highlightCurrentProgramRow(currentProgramRow);
        currentProgramRow++;
        QTimer::singleShot(500, this, &Antonov::updateProgressBar);
    }
    else {
        programRunning = false;
    }
}

void Antonov::updateLanguageLabel() {
    QInputMethod* inputMethod = QGuiApplication::inputMethod();
    QLocale currentLocale = inputMethod->locale();

    if (currentLocale.language() == QLocale::Russian) {
        ui->label_language->setText("RU");
    }
    else if (currentLocale.language() == QLocale::English) {
        ui->label_language->setText("EN");
    }
    else {
        ui->label_language->setText(currentLocale.nativeLanguageName());
    }

    qDebug() << "Current input language:" << currentLocale.nativeLanguageName();
}

bool Antonov::analyzeProgramForErrors() {
    for (const QString& line : loadedProgram) {
        if (line.contains("G") || line.contains("M") || line.contains(QRegularExpression("N\\d+"))) {
            ui->label_error->setText(tr("Ошибок нет"));
            ui->label_error_4->setText(tr("Ошибок нет"));
            return false;
        }
    }
    ui->label_error->setText(tr("Найдены ошибки, проверьте исполняющую программу"));
    ui->label_error_4->setText(tr("Найдены ошибки, проверьте исполняющую программу"));
    return true;
}

void Antonov::updateTime() {
    QDateTime currentTime = QDateTime::currentDateTime();
    ui->label_date->setText(currentTime.toString("dd.MM.yyyy"));
    ui->label_time->setText(currentTime.toString("HH:mm:ss"));
    ui->label_date_2->setText(currentTime.toString("dd.MM.yyyy"));
    ui->label_time_2->setText(currentTime.toString("HH:mm:ss"));
}

void Antonov::updateStatusBar(int value) {
    ui->progressBar_runtime->setValue(value);
    ui->progressBar_runtime_2->setValue(value);
}

// Активные функции, анализ УП после загрузки

void Antonov::displayActiveFunctions() {
    QSet<QString> gCodes, mCodes, tCodes, dCodes;
    QRegularExpression regexG(R"(G\d{1,3})");
    QRegularExpression regexM(R"(M\d{1,3})");
    QRegularExpression regexT(R"(T\d{1,2})");
    QRegularExpression regexD(R"(D\d{1,2})");

    for (const QString& line : loadedProgram) {
        QRegularExpressionMatch matchG = regexG.match(line);
        if (matchG.hasMatch())
            gCodes.insert(matchG.captured(0));

        QRegularExpressionMatch matchM = regexM.match(line);
        if (matchM.hasMatch())
            mCodes.insert(matchM.captured(0));

        QRegularExpressionMatch matchT = regexT.match(line);
        if (matchT.hasMatch())
            tCodes.insert(matchT.captured(0));

        QRegularExpressionMatch matchD = regexD.match(line);
        if (matchD.hasMatch())
            dCodes.insert(matchD.captured(0));
    }

    QList<QLabel*> gLabels = {
        ui->label_g_code, ui->label_g_code_1, ui->label_g_code_2, ui->label_g_code_3, ui->label_g_code_4,
        ui->label_g_code_5, ui->label_g_code_6, ui->label_g_code_7, ui->label_g_code_8, ui->label_g_code_9,
        ui->label_g_code_10, ui->label_g_code_11, ui->label_g_code_12, ui->label_g_code_13, ui->label_g_code_14,
        ui->label_g_code_15, ui->label_g_code_16, ui->label_g_code_17, ui->label_g_code_18, ui->label_g_code_19,
        ui->label_g_code_20, ui->label_g_code_21, ui->label_g_code_22, ui->label_g_code_23, ui->label_g_code_24,
        ui->label_g_code_25, ui->label_g_code_26, ui->label_g_code_27, ui->label_g_code_28, ui->label_g_code_29
    };
    QList<QLabel*> mLabels = {
        ui->label_m_code, ui->label_m_code_1, ui->label_m_code_2, ui->label_m_code_3,
        ui->label_m_code_4, ui->label_m_code_5, ui->label_m_code_6
    };
    QList<QLabel*> tLabels = {
        ui->label_t_code, ui->label_t_code_1, ui->label_t_code_2, ui->label_t_code_3,
        ui->label_t_code_4, ui->label_t_code_5, ui->label_t_code_6
    };
    QList<QLabel*> dLabels = {
        ui->label_d_code, ui->label_d_code_1, ui->label_d_code_2, ui->label_d_code_3, ui->label_d_code_4
    };

    auto assignRandomLabels = [](const QSet<QString>& codes, QList<QLabel*>& labels) {
        QList<int> availableIndexes;
        for (int i = 0; i < labels.size(); ++i)
            availableIndexes.append(i);
        std::random_device rd;
        std::mt19937 rng(rd());
        std::shuffle(availableIndexes.begin(), availableIndexes.end(), rng);
        int index = 0;
        for (const QString& code : codes) {
            if (index < availableIndexes.size())
                labels[availableIndexes[index++]]->setText(code);
        }
        for (int i = index; i < availableIndexes.size(); ++i)
            labels[availableIndexes[i]]->setText("");
        };

    assignRandomLabels(gCodes, gLabels);
    assignRandomLabels(mCodes, mLabels);
    assignRandomLabels(tCodes, tLabels);
    assignRandomLabels(dCodes, dLabels);
}


void Antonov::changeCoordinateSystem()
{
}

void __cdecl Antonov::drawLineTo(double x, double y)
{
}

void Antonov::drawArcTo(double xEnd, double yEnd, double radius, bool clockwise) {
    double xStart = currentX, yStart = currentY;
    double dx = xEnd - xStart;
    double dy = yEnd - yStart;
    double dist = sqrt(dx * dx + dy * dy);

    if (radius < dist / 2.0) {
        qDebug() << "Ошибка: Радиус слишком мал!";
        return;
    }

    double midX = (xStart + xEnd) / 2.0;
    double midY = (yStart + yEnd) / 2.0;
    double h = sqrt(radius * radius - (dist / 2.0) * (dist / 2.0));
    double centerX = midX + (clockwise ? h * dy / dist : -h * dy / dist);
    double centerY = midY - (clockwise ? h * dx / dist : -h * dx / dist);

    double startAngle = atan2(yStart - centerY, xStart - centerX) * 180 / M_PI;
    double endAngle = atan2(yEnd - centerY, xEnd - centerX) * 180 / M_PI;
    double spanAngle = clockwise ? (startAngle - endAngle) : (endAngle - startAngle);

    QPainterPath arcPath;
    arcPath.arcTo(centerX - radius, -centerY - radius, radius * 2, radius * 2, startAngle, spanAngle);
    toolPath.addPath(arcPath);

    if (pathItem) {
        scene->removeItem(pathItem);
        delete pathItem;
    }
    pathItem = new QGraphicsPathItem(toolPath);
    pathItem->setPen(QPen(Qt::red, 3));
    scene->addItem(pathItem);

    updateToolPosition(xEnd, yEnd);
    currentX = xEnd;
    currentY = yEnd;

    ui->graphView_simul->scene()->update();
}

void Antonov::changeFeedUnit()
{
}

void Antonov::switchToSecondScreen()
{
}

void Antonov::syncLanguage()
{
}

void Antonov::updateInfoLine(const QString& message) {
    ui->info_line->setText(message);
}

void Antonov::startExecution() {
    if (commandQueue.isEmpty()) {
        qDebug() << "Нет команд для выполнения!";
        return;
    }

    qDebug() << "Запуск программы!";
    executeNextStep();
}

void Antonov::keyPressEvent(QKeyEvent* event) {
    updateLanguageLabel();

    QMainWindow::keyPressEvent(event);
}

// Управление кнопками управления

void Antonov::handleResetAlarm() {
    QMessageBox::information(this, tr("Reset Alarm"), tr("Все ошибки сброшены."));
    qDebug() << "Сигналы тревоги сброшены";
}

void Antonov::handleSpindleSpeedChange() {
    spindleSpeedMultiplier = 1.0 + (ui->slider_spindle_speed->value() / 100.0);
    ui->label_spindle_value->setText(QString::number(spindleSpeed * spindleSpeedMultiplier));
}

void Antonov::handleFeedRateChange() {
    feedRateMultiplier = 1.0 + (ui->slider_feed_rate->value() / 100.0);
    ui->label_feed_value->setText(QString::number(feedRate * feedRateMultiplier));
}

void Antonov::handleStart() {
    if (!programRunning && !loadedProgram.isEmpty()) {
        programRunning = true;
        programProgress = 0;
        updateProgramStatus();
        updateProgressBar();
        ui->label_rejim_isp->setText("AUTO");
        ui->label_rejim_isp_4->setText("AUTO");
    }
    if (commandQueue.isEmpty()) {
        qDebug() << "Нет команд для выполнения!";
        return;
    }

    if (!stepTimer) {
        stepTimer = new QTimer(this);
        connect(stepTimer, &QTimer::timeout, this, &Antonov::handleStart);
    }

    scene->clear(); // ✅ Очищаем сцену перед стартом, чтобы убрать старые линии

    executeNextStep();

    stepTimer->start(500); // Интервал 500 мс (можно подстроить)
    qDebug() << "Запуск обработки программы!";
}

void Antonov::handleStop() {
    if (programRunning) {
        programRunning = false;
        ui->label_rejim_isp->setText("PROG");
        ui->label_rejim_isp_4->setText("PROG");
        qDebug() << "Программа остановлена";
    }
}

void Antonov::handleReset() {
    programProgress = 0;
    currentProgramRow = 0;
    updateStatusBar(0);
    programRunning = false;

    xValueCurrent = 0;
    yValueCurrent = 0;
    zValueCurrent = 0;
    xValueFinal = 10;
    yValueFinal = 10;
    zValueFinal = 10;

    updateCoordinatesDisplay();

    if (ui->listWidget_program->count() > 0) {
        QListWidgetItem* item = ui->listWidget_program->item(0);
        ui->listWidget_program->setCurrentItem(item);
    }
}

// Работа с загрузкой программы и логикой

void Antonov::loadProgram() {
    loadedProgram.clear();
    commandQueue.clear();
    ui->listWidget_program->clear();

    bool useMockData = true; // Переключатель между моками и реальными данными

    if (useMockData) {
        this->loadMockProgram(MOCK_BASIC); // Загружаем сложную мок-программу
        logEvent("Загружена мок-программа.");
    }
    else {
        loadRealProgram();
        logEvent("Загружена программа из файла.");
    }

    updateProgramStatus();
}

void Antonov::logEvent(const QString& message) {
    QFile logFile("program_log.txt");
    if (logFile.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&logFile);
        out << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << " - " << message << "\n";
        logFile.close();
    }
}


void Antonov::loadRealProgram() {
    QString fileName = QFileDialog::getOpenFileName(
        this, tr("Выбрать программу"), QString(),
        tr("Файлы программы (*.txt *.nc);;Все файлы (*.*)")
    );

    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            while (!in.atEnd()) {
                QString line = in.readLine();
                loadedProgram.append(line);
                ui->listWidget_program->addItem(line);
            }
            updateInfoLine("Программа загружена успешно");
            analyzeProgram();
        }
    }
}

enum MockType {
    MOCK_BASIC,
    MOCK_ARC,
    MOCK_COMPLEX
};

void Antonov::loadMockProgram(MockType type) {
    loadedProgram.clear();
    ui->listWidget_program->clear();

    QStringList mockProgram;

    switch (type) {
    case MOCK_BASIC:
        mockProgram = {
            "O0007 (G-Code Program)",
            "G17 G90 G15 G191 G71 G72 G172 G272 G94 G97 G49 G40 G0 G80 G98 G53 G153 G193 G64 BRISK CUT2DF",
            "G17 M06 T1",
            "M03 S3000",
            "G0 Z1",
            "G0 X40.9375 Y-40.5",
            "G1 Z-0.25 F1500",
            "G1 X33.25 Y-19.75",
            "G1 X25.9375 Y-40.5",
            "G1 X40.9375 Y-40.5",

            "G0 Z1",
            "G0 X56.75 Y-61.25",
            "G1 Z-0.25 F1500",
            "G1 X48.5 Y-61.25",
            "G1 X43 Y-46.75",
            "G1 X24.0625 Y-46.75",
            "G1 X18.8125 Y-61.25",
            "G1 X11 Y-61.25",
            "G1 X29.4375 Y-12.4375",
            "G1 X37.125 Y-12.4375",
            "G1 X56.75 Y-61.25",

            "G0 Z1",
            "G0 X62.5625 Y-61.25",
            "G1 Z-0.25 F1500",
            "G1 X62.5625 Y-25.5625",
            "G1 X70 Y-25.5625",
            "G1 X70 Y-39.8125",
            "G1 X84.3125 Y-39.8125",
            "G1 X84.3125 Y-25.5625",
            "G1 X91.75 Y-25.5625",
            "G1 X91.75 Y-61.25",
            "G1 X84.3125 Y-61.25",
            "G1 X84.3125 Y-45.875",
            "G1 X70 Y-45.875",
            "G1 X70 Y-61.25",
            "G1 X62.5625 Y-61.25",

            "G0 Z1",
            "G0 X98.625 Y-31.625",
            "G1 Z-0.25 F1500",
            "G1 X98.625 Y-25.5625",
            "G1 X127.9375 Y-25.5625",
            "G1 X127.9375 Y-31.625",
            "G1 X117 Y-31.625",
            "G1 X117 Y-61.25",
            "G1 X109.5625 Y-61.25",
            "G1 X109.5625 Y-31.625",
            "G1 X98.625 Y-31.625",

            "G0 Z1",
            "G0 X149.1875 Y-24.75",
            "G1 Z-0.25 F1500",
            "G1 X152.8125 Y-25.0625",
            "G1 X156.0625 Y-26",
            "G1 X158.9375 Y-27.5",
            "G1 X161.4375 Y-29.6875",
            "G1 X163.4375 Y-32.375",
            "G1 X164.875 Y-35.5625",
            "G1 X165.6875 Y-39.1875",
            "G1 X166 Y-43.375",
            "G1 X165.6875 Y-47.5625",
            "G1 X164.875 Y-51.1875",
            "G1 X163.4375 Y-54.375",
            "G1 X161.4375 Y-57.125",
            "G1 X158.9375 Y-59.25",
            "G1 X156.0625 Y-60.8125",
            "G1 X152.8125 Y-61.6875",
            "G1 X149.1875 Y-62",
            "G1 X145.5625 Y-61.6875",
            "G1 X142.375 Y-60.8125",
            "G1 X139.5 Y-59.25",
            "G1 X137 Y-57.125",
            "G1 X135 Y-54.4375",
            "G1 X133.5625 Y-51.25",
            "G1 X132.75 Y-47.5625",
            "G1 X132.4375 Y-43.375",
            "G1 X132.75 Y-39.1875",
            "G1 X133.5625 Y-35.5625",
            "G1 X135 Y-32.375",
            "G1 X137 Y-29.625",
            "G1 X139.5 Y-27.5",
            "G1 X142.375 Y-26",
            "G1 X145.5625 Y-25.0625",
            "G1 X149.1875 Y-24.75",

            "G0 Z1",
            "G0 X149.1875 Y-55.9375",
            "G1 Z-0.25 F1500",
            "G1 X151.125 Y-55.75",
            "G1 X152.875 Y-55.125",
            "G1 X154.4375 Y-54.1875",
            "G1 X155.8125 Y-52.8125",
            "G1 X156.9375 Y-51.0625",
            "G1 X157.75 Y-48.9375",
            "G1 X158.1875 Y-46.375",
            "G1 X158.375 Y-43.375",
            "G1 X158.1875 Y-40.4375",
            "G1 X157.75 Y-37.875",
            "G1 X156.9375 Y-35.6875",
            "G1 X155.8125 Y-33.9375",
            "G1 X154.4375 Y-32.5625",
            "G1 X152.875 Y-31.625",
            "G1 X151.125 Y-31",
            "G1 X149.1875 Y-30.8125",
            "G1 X147.25 Y-31",
            "G1 X145.5 Y-31.625",
            "G1 X143.9375 Y-32.5625",
            "G1 X142.5625 Y-33.9375",
            "G1 X141.4375 Y-35.6875",
            "G1 X140.6875 Y-37.8125",
            "G1 X140.1875 Y-40.375",
            "G1 X140.0625 Y-43.375",
            "G1 X140.1875 Y-46.375",
            "G1 X140.6875 Y-48.9375",
            "G1 X141.4375 Y-51.0625",
            "G1 X142.5625 Y-52.8125",
            "G1 X143.9375 Y-54.1875",
            "G1 X145.5 Y-55.125",
            "G1 X147.25 Y-55.75",
            "G1 X149.1875 Y-55.9375",

            "G0 Z1",
            "G0 X174 Y-61.25",
            "G1 Z-0.25 F1500",
            "G1 X174 Y-25.5625",
            "G1 X181.4375 Y-25.5625",
            "G1 X181.4375 Y-39.8125",
            "G1 X195.75 Y-39.8125",
            "G1 X195.75 Y-25.5625",
            "G1 X203.1875 Y-25.5625",
            "G1 X203.1875 Y-61.25",
            "G1 X195.75 Y-61.25",
            "G1 X195.75 Y-45.875",
            "G1 X181.4375 Y-45.875",
            "G1 X181.4375 Y-61.25",
            "G1 X174 Y-61.25",

            "G0 Z1",
            "G0 X227.9375 Y-24.75",
            "G1 Z-0.25 F1500",
            "G1 X231.5625 Y-25.0625",
            "G1 X234.8125 Y-26",
            "G1 X237.6875 Y-27.5",
            "G1 X240.1875 Y-29.6875",
            "G1 X242.1875 Y-32.375",
            "G1 X243.625 Y-35.5625",
            "G1 X244.4375 Y-39.1875",
            "G1 X244.75 Y-43.375",
            "G1 X244.4375 Y-47.5625",
            "G1 X243.625 Y-51.1875",
            "G1 X242.1875 Y-54.375",
            "G1 X240.1875 Y-57.125",
            "G1 X237.6875 Y-59.25",
            "G1 X234.8125 Y-60.8125",
            "G1 X231.5625 Y-61.6875",
            "G1 X227.9375 Y-62",
            "G1 X220.1875 Y-35.6875",
            "G1 X219.4375 Y-37.8125",
            "G1 X218.9375 Y-40.375",
            "G1 X218.8125 Y-43.375",
            "G1 X218.9375 Y-46.375",
            "G1 X219.4375 Y-48.9375",
            "G1 X220.1875 Y-51.0625",
            "G1 X221.3125 Y-52.8125",
            "G1 X222.6875 Y-54.1875",
            "G1 X224.25 Y-55.125",
            "G1 X226 Y-55.75",
            "G1 X227.9375 Y-55.9375",
            "G1 X227.9375 Y-55.9375",
            "G0 Z1",
            "G0 X269.4375 Y-61.25",
            "G1 Z-0.25 F1500",
            "G1 X252.75 Y-61.25",
            "G1 X252.75 Y-25.5625",
            "G1 X267.3125 Y-25.5625",
            "G1 X270.4375 Y-25.6875",
            "G1 X273.1875 Y-26.1875",
            "G1 X275.5 Y-26.9375",
            "G1 X277.375 Y-28",
            "G1 X278.8125 Y-29.375",
            "G1 X279.875 Y-31.0625",
            "G1 X280.5 Y-33",
            "G1 X280.6875 Y-35.3125",
            "G1 X280.4375 Y-37.9375",
            "G1 X279.5625 Y-40.0625",
            "G1 X278.1875 Y-41.625",
            "G1 X276.25 Y-42.625",
            "G1 X278.875 Y-43.6875",
            "G1 X280.75 Y-45.4375",
            "G1 X281.875 Y-47.875",
            "G1 X282.25 Y-50.9375",
            "G1 X282 Y-53.1875",
            "G1 X281.375 Y-55.1875",
            "G1 X280.25 Y-56.9375",
            "G1 X278.75 Y-58.4375",
            "G1 X274.6875 Y-60.5625",
            "G1 X269.4375 Y-61.25",
            "G1 X269.4375 Y-61.25",
            "G0 Z1",
            "G0 X260.1875 Y-31.625",
            "G1 Z-0.25 F1500",
            "G1 X260.1875 Y-39.9375",
            "G1 X266.75 Y-39.9375",
            "G1 X269.8125 Y-39.6875",
            "G1 X271.8125 Y-39",
            "G1 X272.875 Y-37.75",
            "G1 X273.25 Y-35.8125",
            "G1 X272.875 Y-33.9375",
            "G1 X271.75 Y-32.625",
            "G1 X269.5625 Y-31.875",
            "G1 X266.25 Y-31.625",
            "G1 X260.1875 Y-31.625",
            "G0 Z1",
            "G0 X260.1875 Y-46",
            "G1 Z-0.25 F1500",
            "G1 X260.1875 Y-55.1875",
            "G1 X267.5 Y-55.1875",
            "G1 X270.5625 Y-54.875",
            "G1 X272.8125 Y-54.0625",
            "G1 X274.125 Y-52.625",
            "G1 X274.5625 Y-50.625",
            "G1 X274.0625 Y-48.625",
            "G1 X272.6875 Y-47.1875",
            "G1 X270.3125 Y-46.3125",
            "G1 X267 Y-46",
            "G1 X260.1875 Y-46",
            "G0 Z1",
            "G0 X56.1875 Y-125.3125",
            "G1 Z-0.25 F1500",
            "G1 X62.4375 Y-125.3125",
            "G1 X62.4375 Y-143.3125",
            "G1 X54.8125 Y-143.3125",
            "G1 X54.8125 Y-132.25",
            "G1 X19.8125 Y-132.25",
            "G1 X19.8125 Y-143.3125",
            "G1 X12.1875 Y-143.3125",
            "G1 X12.1875 Y-125.3125",
            "G1 X16.8125 Y-125.3125",
            "G1 X17.875 Y-123.1875",
            "G1 X18.8125 Y-120.6875",
            "G1 X19.75 Y-117.6875",
            "G1 X20.625 Y-114.3125",
            "G1 X21.3125 Y-110.25",
            "G1 X21.8125 Y-105.1875",
            "G1 X22.125 Y-99.25",
            "G1 X22.25 Y-92.3125",
            "G1 X22.25 Y-83.4375",
            "G1 X56.1875 Y-83.4375",
            "G1 X56.1875 Y-125.3125",
            "G0 Z1",
            "G0 X48.5 Y-125.3125",
            "G1 Z-0.25 F1500",
            "G1 X48.5 Y-90.25",
            "G1 X29.875 Y-90.25",
            "G1 X29.875 Y-92.875",
            "G1 X29.75 Y-99.3125",
            "G1 X29.5 Y-105",
            "G1 X29 Y-109.875",
            "G1 X28.3125 Y-114",
            "G1 X27.4375 Y-117.5",
            "G1 X26.5 Y-120.5",
            "G1 X25.375 Y-123.125",
            "G1 X24.125 Y-125.3125",
            "G1 X48.5 Y-125.3125",
            "G0 Z1",
            "G0 X69.9375 Y-132.25",
            "G1 Z-0.25 F1500",
            "G1 X69.9375 Y-96.5625",
            "G1 X79.9375 Y-96.5625",
            "G1 X89.75 Y-123.25",
            "G1 X99.625 Y-96.5625",
            "G1 X109.625 Y-96.5625",
            "G1 X109.625 Y-132.25",
            "G1 X102.1875 Y-132.25",
            "G1 X102.1875 Y-105.3125",
            "G1 X92.9375 Y-132.25",
            "G1 X86.625 Y-132.25",
            "G1 X77.375 Y-105.3125",
            "G1 X77.375 Y-132.25",
            "G1 X69.9375 Y-132.25",
            "G0 Z1",
            "G0 X120.125 Y-132.25",
            "G1 Z-0.25 F1500",
            "G1 X120.125 Y-96.5625",
            "G1 X127.5625 Y-96.5625",
            "G1 X127.5625 Y-122.6875",
            "G1 X141.875 Y-96.5625",
            "G1 X149.5625 Y-96.5625",
            "G1 X149.5625 Y-132.25",
            "G1 X142.0625 Y-132.25",
            "G1 X142.0625 Y-106.0625",
            "G1 X127.75 Y-132.25",
            "G1 X120.125 Y-132.25",
            "G0 Z1",
            "G0 X156.375 Y-102.625",
            "G1 Z-0.25 F1500",
            "G1 X156.375 Y-96.5625",
            "G1 X185.75 Y-96.5625",
            "G1 X185.75 Y-102.625",
            "G1 X174.8125 Y-102.625",
            "G1 X174.8125 Y-132.25",
            "G1 X167.375 Y-132.25",
            "G1 X167.375 Y-102.625",
            "G1 X156.375 Y-102.625",
            "G0 Z1",
            "G0 X199.8125 Y-100.625",
            "G1 Z-0.25 F1500",
            "G1 X201.5625 Y-98.5",
            "G1 X203.6875 Y-97",
            "G1 X206.125 Y-96.0625",
            "G1 X209 Y-95.75",
            "G1 X212.3125 Y-96.0625",
            "G1 X215.25 Y-97",
            "G1 X217.75 Y-98.5625",
            "G1 X219.9375 Y-100.75",
            "G1 X221.625 Y-103.5",
            "G1 X222.8125 Y-106.625",
            "G1 X223.5625 Y-110.1875",
            "G1 X223.8125 Y-114.125",
            "G1 X223.5625 Y-118.25",
            "G1 X222.75 Y-121.875",
            "G1 X221.375 Y-125.125",
            "G1 X219.5 Y-127.9375",
            "G1 X217.1875 Y-130.125",
            "G1 X214.625 Y-131.75",
            "G1 X211.6875 Y-132.6875",
            "G1 X208.5625 Y-133",
            "G1 X205.8125 Y-132.8125",
            "G1 X203.4375 Y-132.125",
            "G1 X201.4375 Y-131.0625",
            "G1 X199.8125 Y-129.5625",
            "G1 X199.8125 Y-145.4375",
            "G1 X192.375 Y-145.4375",
            "G1 X192.375 Y-96.5625",
            "G1 X199.8125 Y-96.5625",
            "G1 X199.8125 Y-100.625"
            "G0 Z1",
            "G0 X199.8125 Y-123.1875",
            "G1 Z-0.25 F1500",
            "G1 X201.6875 Y-124.8125",
            "G1 X203.6875 Y-126",
            "G1 X205.8125 Y-126.6875",
            "G1 X208.1875 Y-126.9375",
            "G1 X209.875 Y-126.75",
            "G1 X211.4375 Y-126.125",
            "G1 X212.75 Y-125.125",
            "G1 X213.9375 Y-123.75",
            "G1 X214.9375 Y-122",
            "G1 X215.625 Y-119.8125",
            "G1 X216 Y-117.1875",
            "G1 X216.125 Y-114.125",
            "G1 X216 Y-111.25",
            "G1 X215.625 Y-108.75",
            "G1 X214.9375 Y-106.625",
            "G1 X214.0625 Y-104.875",
            "G1 X212.9375 Y-103.5",
            "G1 X211.625 Y-102.5625",
            "G1 X210.125 Y-102",
            "G1 X208.4375 Y-101.8125",
            "G1 X206.125 Y-102.125",
            "G1 X203.9375 Y-103.1875",
            "G1 X201.8125 Y-104.875",
            "G1 X199.8125 Y-107.25",
            "G1 X199.8125 Y-123.1875",
            "G0 Z1",
            "G0 X231.75 Y-132.25",
            "G1 Z-0.25 F1500",
            "G1 X231.75 Y-96.5625",
            "G1 X239.1875 Y-96.5625",
            "G1 X239.1875 Y-122.6875",
            "G1 X253.5625 Y-96.5625",
            "G1 X261.1875 Y-96.5625",
            "G1 X261.1875 Y-132.25",
            "G1 X253.75 Y-132.25",
            "G1 X253.75 Y-106.0625",
            "G1 X239.375 Y-132.25",
            "G1 X231.75 Y-132.25",
            "G0 Z1",
            "G0 X271.375 Y-132.25",
            "G1 Z-0.25 F1500",
            "G1 X271.375 Y-96.5625",
            "G1 X278.8125 Y-96.5625",
            "G1 X278.8125 Y-122.6875",
            "G1 X293.125 Y-96.5625",
            "G1 X300.8125 Y-96.5625",
            "G1 X300.8125 Y-132.25",
            "G1 X293.3125 Y-132.25",
            "G1 X293.3125 Y-106.0625",
            "G1 X279 Y-132.25",
            "G1 X271.375 Y-132.25",
            "G0 Z1",
            "G0 X296.3125 Y-83",
            "G1 Z-0.25 F1500",
            "G1 X295.3125 Y-87.3125",
            "G1 X293.0625 Y-90.3125",
            "G1 X289.9375 Y-92.125",
            "G1 X286 Y-92.6875",
            "G1 X282.125 Y-92.125",
            "G1 X278.9375 Y-90.3125",
            "G1 X276.75 Y-87.3125",
            "G1 X275.8125 Y-83",
            "G1 X281 Y-83",
            "G1 X281.625 Y-84.9375",
            "G1 X282.6875 Y-86.3125",
            "G1 X284.125 Y-87.125",
            "G1 X286 Y-87.375",
            "G1 X287.875 Y-87.125",
            "G1 X289.375 Y-86.3125",
            "G1 X290.4375 Y-84.9375",
            "G1 X291.125 Y-83",
            "G0 X296.3125 Y-83",
            "G0 Z1",
            "M30"
        };
        this->updateInfoLine("Загружена базовая мок-программа.");
        break;
    case MOCK_ARC:
        mockProgram = {
            "G17 G90 G15 G191 G71 G72 G172 G272 G94 G97 G49 G40 G00 G80 G98 G53 G153 G193 G64 BRISK CUT2DF", // Строка безопасности
            "G17 M06 T2",              // Смена инструмента (R8)
            "M03 S1000 F3000",         // Включение шпинделя (1000 об/мин), подача 3000 мм/мин
            "G0 Z30",                  // Подъем в безопасную зону
            "#workpiece(0, 256, 1, 148, 70, 6, 0, 0, 0)", // Определение заготовки

            // **Перемещение к стартовой позиции**
            "G0 X74 Y0",               // Быстрый переход к стартовой точке
            "G1 Z-1",                     // Опускание инструмента
            "G1 X74 Y32 G42",          // Линейное движение + включение коррекции инструмента
            "G2 X116.222222 Y24.326684 R120", // Дуга по часовой
            "G2 X124 Y13.094016 R12",  // Дуга по часовой
            "G1 X124 Y0",              // Линейное перемещение
            "G1 X148 Y0",              // Линейное перемещение
            "G1 X148 Y58",             // Линейное перемещение

            // **Обработка контура**
            "G3 X136 Y70 R12",         // Дуга против часовой
            "G3 X124.060151 Y59.2 R12",// Дуга против часовой
            "G2 X116.100251 Y52 R8",   // Дуга по часовой
            "G1 X31.899749",           // Линейное движение
            "G2 X23.939849 Y59.2 R8",  // Дуга по часовой
            "G3 X12 Y70 R12",          // Дуга против часовой
            "G3 X0 Y58 R12",           // Дуга против часовой
            "G1 X0 Y0",                // Возврат в начальную точку
            "G1 X24",                  // Линейное движение по X
            "G1 Y13.094016",           // Линейное движение по Y
            "G2 X31.777778 Y24.326684 R12", // Дуга по часовой
            "G2 X74 Y32 R120",         // Дуга по часовой
            "G2 X116.222222 Y24.326684 R120", // Дуга по часовой

            // **Завершение программы**
            "M30"                      // Конец программы

        };
        this->updateInfoLine("Загружена мок-программа с дугами.");
        break;
    case MOCK_COMPLEX:
        mockProgram = {
            "G17 G21 G90 G94",    // Плоскость XY, мм, абсолютные координаты
            "G0 X0 Y0 Z10",       // Быстрое перемещение в исходную точку
            "G1 X50 Y50 Z0 F200", // Линейная интерполяция с подачей
            "G2 X75 Y75 R25",     // Дуга по часовой
            "G3 X100 Y100 R50",   // Дуга против часовой
            "M6 T2",              // Смена инструмента
            "G43 H2",             // Коррекция по высоте
            "M3 S5000",           // Включение шпинделя (5000 об/мин)
            "G0 Z10",             // Подъем в безопасное положение

            // **ЦИКЛ обработки**
            "G0 X20 Y20 Z5",      // Перемещение к началу обработки
            "G1 Z-5 F100",        // Опускание в материал
            "G1 X80 Y20",         // Линейный рез по X
            "G2 X100 Y40 R20",    // Дуга по часовой
            "G1 X100 Y80",        // Вертикальный рез
            "G3 X80 Y100 R20",    // Дуга против часовой
            "G1 X20 Y100",        // Горизонтальный рез
            "G2 X0 Y80 R20",      // Дуга по часовой
            "G1 X0 Y40",          // Вертикальный рез вверх
            "G3 X20 Y20 R20",     // Дуга против часовой
            "G0 Z10",             // Подъем инструмента

            // **Второй проход обработки**
            "G0 X25 Y25 Z5",      // Перемещение к новой точке
            "G1 Z-5 F120",        // Погружение в материал
            "G1 X75 Y25",         // Горизонтальный рез
            "G2 X90 Y40 R15",     // Дуга по часовой
            "G1 X90 Y90",         // Вертикальный рез
            "G3 X75 Y105 R15",    // Дуга против часовой
            "G1 X25 Y105",        // Горизонтальный рез
            "G2 X10 Y90 R15",     // Дуга по часовой
            "G1 X10 Y40",         // Вертикальный рез вверх
            "G3 X25 Y25 R15",     // Дуга против часовой
            "G0 Z15",             // Подъем инструмента

            // **Завершение программы**
            "M5",                 // Выключение шпинделя
            "G0 X0 Y0",           // Возврат в исходное положение
            "M30"
        };
        this->updateInfoLine("Загружена сложная мок-программа.");
        break;
    }

    for (const QString& line : mockProgram) {
        loadedProgram.append(line);
        ui->listWidget_program->addItem(line);
    }

    this->analyzeProgram();
}

// Управление кнопками контролла

void Antonov::handleStartKadr() {
    bool ok;
    int startRow = QInputDialog::getInt(this, tr("Start from Row"), tr("Enter starting row:"), 0, 0, loadedProgram.size() - 1, 1, &ok);
    if (ok) {
        currentProgramRow = startRow;
        startFromRowEnabled = true;
        programRunning = true;
        programProgress = 0;
        updateProgramStatus();
        updateProgressBar();
        ui->label_rejim_isp->setText("AUTO");
        ui->label_rejim_isp_4->setText("AUTO");
    }
}

void Antonov::handleKorrekt() {
    ui->stackedWidget_3->setCurrentWidget(ui->page_7);
    pageHistory.push(2);
}

void Antonov::handleSmesh() {
    ui->stackedWidget->setCurrentWidget(ui->page_4);
    pageHistory = {};
    pageHistory.push(0);
    pageHistory.push(1);
}

void Antonov::handleBackK2() {
    if (pageHistory.size() > 1) {
        pageHistory.pop();
        int previousPage = pageHistory.top();

        if (previousPage == 1) {
            ui->stackedWidget_3->setCurrentWidget(ui->page_8);
        }
        else if (previousPage == 0) {
            ui->stackedWidget->setCurrentWidget(ui->page_3);
        }
    }
    else {
        ui->stackedWidget->setCurrentWidget(ui->page_3);
    }
}

void Antonov::moveSelection(int rowOffset, int colOffset, QTableWidget* table) {
    int rowCount = table->rowCount();
    int colCount = table->columnCount();

    int newRow = currentRow + rowOffset;
    int newCol = currentColumn + colOffset;

    if (newRow >= 0 && newRow < rowCount && newCol >= 0 && newCol < colCount) {
        currentRow = newRow;
        currentColumn = newCol;
        table->setCurrentCell(currentRow, currentColumn);
    }
}

void Antonov::handleMoveUp() {
    int row = ui->tableWidget->currentRow();
    if (row > 0) {
        ui->tableWidget->setCurrentCell(row - 1, ui->tableWidget->currentColumn());
    }
}

void Antonov::handleMoveDown() {
    int row = ui->tableWidget->currentRow();
    if (row < ui->tableWidget->rowCount() - 1) {
        ui->tableWidget->setCurrentCell(row + 1, ui->tableWidget->currentColumn());
    }
}

void Antonov::handleMoveLeft() {
    int col = ui->tableWidget->currentColumn();
    if (col > 0) {
        ui->tableWidget->setCurrentCell(ui->tableWidget->currentRow(), col - 1);
    }
}

void Antonov::handleMoveRight() {
    int col = ui->tableWidget->currentColumn();
    if (col < ui->tableWidget->columnCount() - 1) {
        ui->tableWidget->setCurrentCell(ui->tableWidget->currentRow(), col + 1);
    }
}

void Antonov::handleSelectCell() {
    QTableWidgetItem* currentItem = ui->tableWidget->currentItem();
    if (currentItem) {
        previousValue = currentItem->text();
        bool ok;
        QString newValue = QInputDialog::getText(this, tr("Edit Cell"), tr("Enter new value:"), QLineEdit::Normal, currentItem->text(), &ok);
        if (ok) {
            currentItem->setText(newValue);
        }
    }
}

void Antonov::handleConfirm() {
    QMessageBox::information(this, tr("Confirm Changes"), tr("Changes have been saved."));
}

void Antonov::handleCancel() {
    QTableWidgetItem* currentItem = ui->tableWidget->currentItem();
    if (currentItem) {
        currentItem->setText(previousValue);
    }
    QMessageBox::information(this, tr("Cancel Changes"), tr("Changes have been canceled."));
}

void Antonov::handleBack() {
    qDebug() << "handleBack вызвана";
}

void Antonov::handleNumeration() {
    static bool numbered = false;
    for (int i = 0; i < ui->listWidget_program->count(); ++i) {
        QListWidgetItem* item = ui->listWidget_program->item(i);
        if (!numbered) {
            item->setText(QString::number(i + 1) + ": " + item->text());
        }
        else {
            QStringList parts = item->text().split(": ");
            if (parts.size() > 1) {
                item->setText(parts[1]);
            }
        }
    }
    numbered = !numbered;
}

void Antonov::handleSmenaEkrana() {
    int currentIndex = ui->stackedWidget_winchan->currentIndex();
    ui->stackedWidget_winchan->setCurrentIndex(currentIndex == 0 ? 1 : 0);
}

void Antonov::handleChangeSK() {
    static QStringList feedUnits = { tr("мм") };
    feedUnitIndex = (feedUnitIndex + 1) % feedUnits.size();
    ui->label_changessk->setText(feedUnits[feedUnitIndex]);
}

void Antonov::handleMmDyum() {
    inInches = !inInches;
    updateUnits();
    for (const QString& line : loadedProgram) {
        extractCoordinatesAndSpeed(line);
    }
}

void Antonov::handlePriv() {
    if (coordinateSystem == "СКС") {
        coordinateSystem = "СКД";
    }
    else {
        coordinateSystem = "СКС";
    }
    updateInfoLine(tr("Смена системы координат: %1").arg(coordinateSystem));
    ui->label_sksskd->setText(coordinateSystem);
}

void Antonov::updateUnits() {
    if (inInches) {
        ui->label_x_axis->setText("X (in)");
        ui->label_y_axis->setText("Y (in)");
        ui->label_z_axis->setText("Z (in)");
    }
    else {
        ui->label_x_axis->setText("X (mm)");
        ui->label_y_axis->setText("Y (mm)");
        ui->label_z_axis->setText("Z (mm)");
    }
}

void Antonov::handleCorrectionUp() {
    int currentRow = ui->listWidget_program->currentRow();
    if (currentRow > 0) {
        ui->listWidget_program->setCurrentRow(currentRow - 1);
    }
}

void Antonov::handleCorrectionDown() {
    int currentRow = ui->listWidget_program->currentRow();
    if (currentRow < ui->listWidget_program->count() - 1) {
        ui->listWidget_program->setCurrentRow(currentRow + 1);
    }
}

void Antonov::handleCorrectionSelect() {
    QListWidgetItem* currentItem = ui->listWidget_program->currentItem();
    if (currentItem) {
        bool ok;
        double newValue = QInputDialog::getDouble(
            this, tr("Edit Parameter"),
            tr("Enter new value:"), currentItem->text().toDouble(), -99999, 99999, 2, &ok
        );
        if (ok) {
            currentItem->setText(QString::number(newValue, 'f', 2));
        }
    }
}

void Antonov::handleCorrectionConfirm() {
    QMessageBox::information(this, tr("Confirm Changes"), tr("Changes have been saved."));
    switchToMainScreen();
}

void Antonov::handleCorrectionCancel() {
    QMessageBox::warning(this, tr("Cancel Changes"), tr("Changes have been discarded."));
    switchToMainScreen();
}

void Antonov::switchToCorrectionScreen() {
    ui->stackedWidget->setCurrentIndex(1);
}

void Antonov::switchToMainScreen() {
    ui->stackedWidget->setCurrentIndex(0);
}


// Отрисовка движения инструмента

void Antonov::highlightCurrentProgramRow(int row) {
    if (row < ui->listWidget_program->count()) {
        QListWidgetItem* item = ui->listWidget_program->item(row);
        ui->listWidget_program->setCurrentItem(item);

        double xNext, yNext, zNext;
        int feedRateNext, spindleSpeedNext;

        extractCoordinatesAndSpeed(loadedProgram[row], false);
        extractNextCoordinates(row, xNext, yNext, zNext, feedRateNext, spindleSpeedNext);
        parseAndDrawTrajectory(loadedProgram[row]);

        // Очищаем и обновляем график
        ui->graphView_simul->scene()->update();
    }
}


void Antonov::analyzeProgram() {
    scene->clear();
    xValueCurrent = 0;
    yValueCurrent = 0;
    for (const QString& line : loadedProgram) {
        extractCoordinatesAndSpeed(line);
    }
    displayActiveFunctions();
}

void Antonov::extractCoordinatesAndSpeed(const QString& line, bool isNext) {
    QRegularExpression regexX(R"(X(-?\d+\.?\d*))");
    QRegularExpression regexY(R"(Y(-?\d+\.?\d*))");
    QRegularExpression regexZ(R"(Z(-?\d+\.?\d*))");

    QRegularExpressionMatch matchX = regexX.match(line);
    QRegularExpressionMatch matchY = regexY.match(line);
    QRegularExpressionMatch matchZ = regexZ.match(line);

    if (matchX.hasMatch()) {
        double value = matchX.captured(1).toDouble();
        qDebug() << "Найден X:" << value;
        if (isNext) xValueFinal = value; else xValueCurrent = value;
    }

    if (matchY.hasMatch()) {
        double value = matchY.captured(1).toDouble();
        qDebug() << "Найден Y:" << value;
        if (isNext) yValueFinal = value; else yValueCurrent = value;
    }

    if (matchZ.hasMatch()) {
        double value = matchZ.captured(1).toDouble();
        qDebug() << "Найден Z:" << value;
        if (isNext) zValueFinal = value; else zValueCurrent = value;
    }

    updateCoordinatesDisplay();
}

void Antonov::parseAndDrawTrajectory(const QString& line) {
    QRegularExpression regexG1(R"(G1\s*X(-?\d+\.?\d*)\s*Y(-?\d+\.?\d*))");
    QRegularExpression regexG2(R"(G2\s*X(-?\d+\.?\d*)\s*Y(-?\d+\.?\d*)\s*R(-?\d+\.?\d*))");
    QRegularExpression regexG3(R"(G3\s*X(-?\d+\.?\d*)\s*Y(-?\d+\.?\d*)\s*R(-?\d+\.?\d*))");

    QRegularExpressionMatch matchG1 = regexG1.match(line);
    QRegularExpressionMatch matchG2 = regexG2.match(line);
    QRegularExpressionMatch matchG3 = regexG3.match(line);

    double x = xValueCurrent, y = yValueCurrent, radius = 0;

    if (matchG1.hasMatch()) {
        x = matchG1.captured(1).toDouble();
        y = matchG1.captured(2).toDouble();

        if (inInches) {
            x *= 25.4;
            y *= 25.4;
        }

        drawToolPath(xValueCurrent, yValueCurrent, x, y);
        xValueCurrent = x;
        yValueCurrent = y;
        updateToolPosition(x, y);
    }
    else if (matchG2.hasMatch() || matchG3.hasMatch()) {
        x = matchG2.hasMatch() ? matchG2.captured(1).toDouble() : matchG3.captured(1).toDouble();
        y = matchG2.hasMatch() ? matchG2.captured(2).toDouble() : matchG3.captured(2).toDouble();
        radius = matchG2.hasMatch() ? matchG2.captured(3).toDouble() : matchG3.captured(3).toDouble();

        if (inInches) {
            x *= 25.4;
            y *= 25.4;
            radius *= 25.4;
        }

        drawArc(xValueCurrent, yValueCurrent, x, y, radius, matchG2.hasMatch());
        xValueCurrent = x;
        yValueCurrent = y;
        updateToolPosition(x, y);
    }

    QApplication::processEvents();
    ui->graphView_simul->scene()->update();
}

// Функция отрисовки дуг
void Antonov::drawArc(double xStart, double yStart, double xEnd, double yEnd, double radius, bool clockwise) {
    double dx = xEnd - xStart;
    double dy = yEnd - yStart;
    double dist = sqrt(dx * dx + dy * dy);

    if (radius < dist / 2.0) {
        qDebug() << "Ошибка: Радиус слишком мал!";
        return;
    }

    double midX = (xStart + xEnd) / 2.0;
    double midY = (yStart + yEnd) / 2.0;
    double h = sqrt(radius * radius - (dist / 2.0) * (dist / 2.0));
    double centerX = midX + (clockwise ? h * dy / dist : -h * dy / dist);
    double centerY = midY - (clockwise ? h * dx / dist : -h * dx / dist);

    double startAngle = atan2(yStart - centerY, xStart - centerX) * 180 / M_PI;
    double endAngle = atan2(yEnd - centerY, xEnd - centerX) * 180 / M_PI;
    double spanAngle = (clockwise) ? (startAngle - endAngle) : (endAngle - startAngle);

    QPainterPath arcPath;
    arcPath.arcTo(centerX - radius, -centerY - radius, radius * 2, radius * 2, startAngle, spanAngle);
    toolPath.addPath(arcPath);

    if (pathItem) {
        scene->removeItem(pathItem);
        delete pathItem;
    }
    pathItem = new QGraphicsPathItem(toolPath);
    pathItem->setPen(QPen(Qt::red, 2));
    scene->addItem(pathItem);
    ui->graphView_simul->scene()->update();
}

bool Antonov::extractArcData(const QString& command, double& x, double& y, double& r) {
    QRegularExpression regexX("X(-?\\d*\\.?\\d+)");
    QRegularExpression regexY("Y(-?\\d*\\.?\\d+)");
    QRegularExpression regexR("R(-?\\d*\\.?\\d+)");

    QRegularExpressionMatch matchX = regexX.match(command);
    QRegularExpressionMatch matchY = regexY.match(command);
    QRegularExpressionMatch matchR = regexR.match(command);

    if (matchX.hasMatch()) x = matchX.captured(1).toDouble();
    if (matchY.hasMatch()) y = matchY.captured(1).toDouble();
    if (matchR.hasMatch()) r = matchR.captured(1).toDouble();

    return matchX.hasMatch() && matchY.hasMatch() && matchR.hasMatch();
}

// Функция для отображения движения инструмента
void Antonov::updateToolPosition(double x, double y) {
    if (tool) {
        scene->removeItem(tool);
        delete tool;
    }

    // Создаем треугольник инструмента
    QPolygonF triangle;
    triangle << QPointF(x, -y)
        << QPointF(x - 5, -y - 10)
        << QPointF(x + 5, -y - 10);

    tool = new QGraphicsPolygonItem(triangle);
    tool->setBrush(Qt::yellow);
    scene->addItem(tool);

    // **Обновляем траекторию инструмента**
    if (currentX != x || currentY != y) {
        drawToolPath(currentX, currentY, x, y);
    }

    // Сохраняем новые координаты
    currentX = x;
    currentY = y;

    ui->graphView_simul->scene()->update();
}

void Antonov::updateToolStep() {
    if (currentPointIndex >= trajectoryPoints.size()) {
        moveTimer->stop();
        qDebug() << "Анимация завершена!";
        return;
    }

    QPointF target = trajectoryPoints[currentPointIndex];
    currentPointIndex++;

    updateToolPosition(target.x(), target.y());
    qDebug() << "Двигаем инструмент в: " << target.x() << ", " << target.y();
}

void Antonov::startAnimation() {
    if (trajectoryPoints.isEmpty()) {
        qDebug() << "Ошибка: Траектория пустая!";
        return;
    }

    qDebug() << "Запуск анимации!";
    currentPointIndex = 0;
    moveTimer->start(50);
}

void Antonov::moveToolSmoothly(double xEnd, double yEnd) {
    targetPosition = QPointF(xEnd, -yEnd);
    moveTimer->start(10);
}

void Antonov::updateToolAnimation() {
    if (currentPointIndex >= trajectoryPoints.size()) {
        moveTimer->stop();
        return;
    }

    QPointF target = trajectoryPoints[currentPointIndex];
    currentPointIndex++;

    updateToolPosition(target.x(), target.y());
    drawToolPath(target.x(), target.y());
}

void Antonov::drawToolPath(double x, double y)
{
}

// Функция для отображения траектории движения инструмента
void Antonov::drawToolPath(double xStart, double yStart, double xEnd, double yEnd) {
    QGraphicsLineItem* line = new QGraphicsLineItem(xStart, -yStart, xEnd, -yEnd);
    QPen pen(Qt::red, 2);
    line->setPen(pen);

    // Добавляем линию в сцену
    scene->addItem(line);

    // Обновляем путь инструмента
    toolPath.moveTo(xStart, -yStart);
    toolPath.lineTo(xEnd, -yEnd);

    if (!pathItem) {
        pathItem = new QGraphicsPathItem();
        pathItem->setPen(QPen(Qt::red, 2));
        scene->addItem(pathItem);
    }
    pathItem->setPath(toolPath);

    ui->graphView_simul->scene()->update();
}

void Antonov::clearToolPath() {
    if (pathItem) {
        scene->removeItem(pathItem);
        delete pathItem;
        pathItem = nullptr;
    }
    toolPath = QPainterPath();

    if (scene) {
        QList<QGraphicsItem*> items = scene->items();
        for (QGraphicsItem* item : items) {
            if (dynamic_cast<QGraphicsLineItem*>(item)) {
                scene->removeItem(item);
                delete item;
            }
        }
    }
}


void Antonov::loadGCode(const QStringList& commands) {
    commandQueue.clear();
    for (const auto& cmd : commands) {
        commandQueue.enqueue(cmd);
    }
}

void Antonov::executeNextStep() {
    if (commandQueue.isEmpty()) {
        qDebug() << "Программа завершена!";
        return;
    }

    QString command = commandQueue.dequeue();
    double x, y, z, r;

    if (command.startsWith("G0") || command.startsWith("G1")) {
        extractCoordinates(command, x, y, z);
        drawToolPath(currentX, currentY, x, y);
        updateToolPosition(x, y);
        currentX = x;
        currentY = y;
    }
    else if (command.startsWith("G2") || command.startsWith("G3")) {
        extractArcData(command, x, y, r);
        drawArc(currentX, currentY, x, y, r, command.startsWith("G2"));
        updateToolPosition(x, y);
        currentX = x;
        currentY = y;
    }
    else if (command.startsWith("G04")) {
        int dwellTime = extractDwellTime(command);
        QThread::sleep(dwellTime);
    }
    else if (command.startsWith("G17")) {
        activePlane = XY_PLANE;
    }
    else if (command.startsWith("G18")) {
        activePlane = XZ_PLANE;
    }
    else if (command.startsWith("G19")) {
        activePlane = YZ_PLANE;
    }
    else if (command.startsWith("G20")) {
        inInches = true;
        updateUnits();
    }
    else if (command.startsWith("G21")) {
        inInches = false;
        updateUnits();
    }
    else if (command.startsWith("G28") || command.startsWith("G30")) {
        returnToReferencePoint(command);
    }
    else if (command.startsWith("G40")) {
        cancelToolCompensation();
    }
    else if (command.startsWith("G43") || command.startsWith("G49")) {
        applyToolLengthOffset(command);
    }
    else if (command.startsWith("G50") || command.startsWith("G92")) {
        setCoordinateOffsets(command);
    }
    else if (command.startsWith("G98") || command.startsWith("G99")) {
        setDrillCycleMode(command);
    }

    QTimer::singleShot(500, this, &Antonov::executeNextStep);
}

bool Antonov::extractCoordinates(const QString& command, double& x, double& y, double& z) {
    QRegularExpression regexX(R"(X(-?\d+\.?\d*))");
    QRegularExpression regexY(R"(Y(-?\d+\.?\d*))");
    QRegularExpression regexZ(R"(Z(-?\d+\.?\d*))");

    QRegularExpressionMatch matchX = regexX.match(command);
    QRegularExpressionMatch matchY = regexY.match(command);
    QRegularExpressionMatch matchZ = regexZ.match(command);

    if (matchX.hasMatch()) x = matchX.captured(1).toDouble();
    if (matchY.hasMatch()) y = matchY.captured(1).toDouble();
    if (matchZ.hasMatch()) z = matchZ.captured(1).toDouble();

    return matchX.hasMatch() || matchY.hasMatch() || matchZ.hasMatch();
}

bool Antonov::parseGCode(const QString& command, double& x, double& y) {
    QRegularExpression rx("X(-?\\d*\\.?\\d+)\\s*Y(-?\\d*\\.?\\d+)");
    QRegularExpressionMatch match = rx.match(command);
    if (match.hasMatch()) {
        x = match.captured(1).toDouble();
        y = match.captured(2).toDouble();

        if (std::isnan(x) || std::isnan(y)) {
            qDebug() << "Ошибка парсинга: координаты NaN в команде " << command;
            return false;
        }

        qDebug() << "Команда " << command << " -> X:" << x << " Y:" << y;
        return true;
    }
    qDebug() << "Ошибка парсинга: " << command;
    return false;
}

bool Antonov::parseArcGCode(const QString& command, double& x, double& y, double& r) {
    QRegularExpression rx(R"(X(-?\d*\.?\d+)\s*Y(-?\d*\.?\d+)\s*R(-?\d*\.?\d+))");
    QRegularExpressionMatch match = rx.match(command);

    if (match.hasMatch()) {
        x = match.captured(1).toDouble();
        y = match.captured(2).toDouble();
        r = match.captured(3).toDouble();
        return true;
    }
    return false;
}

void Antonov::moveTriangleTo(double x, double y) {
    qDebug() << "Перемещение в: X=" << x << " Y=" << y;

    // Добавляем линию на сцену
    QGraphicsLineItem* line = new QGraphicsLineItem(currentX, -currentY, x, -y);
    line->setPen(QPen(Qt::red, 2));
    scene->addItem(line);

    // Обновляем координаты текущего положения
    currentX = x;
    currentY = y;

    // Обновляем положение инструмента
    updateToolPosition(x, y);

    // Перерисовываем сцену
    ui->graphView_simul->scene()->update();
}

void Antonov::on_startButton_clicked() {
    handleStart();
}

void Antonov::processCommand(const QString& command) {
    qDebug() << "Выполняем команду:" << command;

    if (command.startsWith("G1")) { // Прямая линия
        // Парсим X, Y, Z координаты
        double x = extractCoordinate(command, 'X');
        double y = extractCoordinate(command, 'Y');
        double z = extractCoordinate(command, 'Z');

        // Добавляем точку на траекторию
        toolPath.lineTo(x, y);

        // Обновляем графику
        ui->graphView_simul->scene()->update();
    }
}

double Antonov::extractCoordinate(const QString& command, QChar axis) {
    QRegularExpression regex(axis + QString("(-?\\d*\\.?\\d+)"));
    QRegularExpressionMatch match = regex.match(command);
    if (match.hasMatch()) {
        return match.captured(1).toDouble();
    }
    return 0.0; // Если координата не найдена, возвращаем 0
}


void Antonov::resetCoordinateOffsets()
{
}

void Antonov::updateCoordinatesDisplay() {
    ui->label_x_value_current->setText(QString::number(xValueCurrent, 'f', 2));
    ui->label_y_value_current->setText(QString::number(yValueCurrent, 'f', 2));
    ui->label_z_value_current->setText(QString::number(zValueCurrent, 'f', 2));

    double xNext = xValueCurrent, yNext = yValueCurrent, zNext = zValueCurrent;
    int feedRateNext = feedRate, spindleSpeedNext = spindleSpeed;

    if (currentStep < loadedProgram.size())
        extractNextCoordinates(currentStep, xNext, yNext, zNext, feedRateNext, spindleSpeedNext);

    xValueFinal = xNext;
    yValueFinal = yNext;
    zValueFinal = zNext;

    ui->label_x_value_final->setText(QString::number(xValueFinal, 'f', 2));
    ui->label_y_value_final->setText(QString::number(yValueFinal, 'f', 2));
    ui->label_z_value_final->setText(QString::number(zValueFinal, 'f', 2));
}

void Antonov::simulateNextStep() {
    if (currentStep >= loadedProgram.size()) {
        simulationTimer->stop();
        updateInfoLine("Симуляция завершена.");
        return;
    }

    double xNext = xValueCurrent, yNext = yValueCurrent, zNext = zValueCurrent;
    int feedRateNext = feedRate, spindleSpeedNext = spindleSpeed;

    extractNextCoordinates(currentStep, xNext, yNext, zNext, feedRateNext, spindleSpeedNext);
    scene->addLine(xValueCurrent, -yValueCurrent, xNext, -yNext, QPen(Qt::red));

    xValueCurrent = xNext;
    yValueCurrent = yNext;
    zValueCurrent = zNext;
    feedRate = feedRateNext;
    spindleSpeed = spindleSpeedNext;

    // Подготовка координат для следующего шага
    extractNextCoordinates(currentStep + 1, xValueFinal, yValueFinal, zValueFinal, feedRateNext, spindleSpeedNext);
    updateCoordinatesDisplay();
    highlightCurrentProgramRow(currentStep);
    updateProgressBar();

    ui->graphView_simul->scene()->update();
    qApp->processEvents();

    currentStep++;
}

void Antonov::applyToolCompensation(const QString& command) {
    if (command.startsWith("G41")) {
        toolCompensation = LEFT;
    }
    else if (command.startsWith("G42")) {
        toolCompensation = RIGHT;
    }
}

void Antonov::animateToolMovement(double xStart, double yStart, double xEnd, double yEnd) {
    int steps = 20;  // Количество промежуточных шагов
    int delayMs = 10;  // Интервал обновления (10 мс)

    QTimer* animationTimer = new QTimer(this);
    int* currentStep = new int(0);  // Создаем переменную для отслеживания текущего шага

    connect(animationTimer, &QTimer::timeout, this, [=]() mutable {
        if (*currentStep > steps) {
            animationTimer->stop();
            animationTimer->deleteLater();
            delete currentStep;
            return;
        }

        double t = static_cast<double>(*currentStep) / steps;
        double x = xStart + t * (xEnd - xStart);
        double y = yStart + t * (yEnd - yStart);

        scene->addLine(xStart, -yStart, x, -y, QPen(Qt::red));
        ui->graphView_simul->scene()->update();  // Обновление интерфейса
        qApp->processEvents();

        (*currentStep)++;
        });

    animationTimer->start(delayMs);  // Запускаем таймер
}

int Antonov::extractDwellTime(const QString& command) {
    QRegularExpression regexP("P(\\d+)");
    QRegularExpressionMatch matchP = regexP.match(command);
    if (matchP.hasMatch()) {
        return matchP.captured(1).toInt();
    }
    return 0; // Возвращаем 0, если значение не найдено
}

void Antonov::cancelToolCompensation() {
    toolCompensation = 40;  // G40 - отключение компенсации инструмента
}

void Antonov::returnToReferencePoint(const QString& command) {
    if (command.startsWith("G28")) {  // G28 - Возврат в нулевую точку
        xValueCurrent = yValueCurrent = zValueCurrent = 0;
    }
    else if (command.startsWith("G30")) {  // G30 - Возврат в предустановленную точку
        xValueCurrent = 50;
        yValueCurrent = 50;
        zValueCurrent = 10;
    }
    updateCoordinatesDisplay();
}


void Antonov::setCoordinateOffsets(const QString& command) {
    QRegularExpression regexX("X(-?\\d*\\.?\\d+)");
    QRegularExpression regexY("Y(-?\\d*\\.?\\d+)");
    QRegularExpression regexZ("Z(-?\\d*\\.?\\d+)");

    QRegularExpressionMatch matchX = regexX.match(command);
    QRegularExpressionMatch matchY = regexY.match(command);
    QRegularExpressionMatch matchZ = regexZ.match(command);

    if (matchX.hasMatch()) xOffset = matchX.captured(1).toDouble();
    if (matchY.hasMatch()) yOffset = matchY.captured(1).toDouble();
    if (matchZ.hasMatch()) zOffset = matchZ.captured(1).toDouble();

    qDebug() << "Смещение координат установлено: X=" << xOffset << " Y=" << yOffset << " Z=" << zOffset;
}

void Antonov::applyToolLengthOffset(const QString& command) {
    QRegularExpression regexH("H(\\d+)");
    QRegularExpressionMatch matchH = regexH.match(command);
    if (matchH.hasMatch()) {
        int toolOffset = matchH.captured(1).toInt();
        qDebug() << "Применена коррекция длины инструмента: H" << toolOffset;
    }
}

void Antonov::setDrillCycleMode(const QString& command) {
    if (command.startsWith("G81")) {
        drillMode = PECK_DRILLING;
    }
    else if (command.startsWith("G85")) {
        drillMode = BORING;
    }
    qDebug() << "Режим сверления установлен: " << drillMode;
}

void Antonov::extractNextCoordinates(int currentStep, double& xNext, double& yNext, double& zNext, int& feedRateNext, int& spindleSpeedNext) {
    if (currentStep + 1 >= loadedProgram.size()) {
        // Если следующей команды нет, оставляем текущие значения
        xNext = xValueCurrent;
        yNext = yValueCurrent;
        zNext = zValueCurrent;
        feedRateNext = feedRate;
        spindleSpeedNext = spindleSpeed;
        return;
    }

    QString nextLine = loadedProgram[currentStep + 1];

    // Регулярные выражения для поиска координат и скоростей
    QRegularExpression regexX(R"(X(-?\d+\.?\d*))");
    QRegularExpression regexY(R"(Y(-?\d+\.?\d*))");
    QRegularExpression regexZ(R"(Z(-?\d+\.?\d*))");
    QRegularExpression regexF(R"(F(\d+))");
    QRegularExpression regexS(R"(S(\d+))");

    // Поиск X
    QRegularExpressionMatch matchX = regexX.match(nextLine);
    if (matchX.hasMatch()) {
        xNext = matchX.captured(1).toDouble();
        if (inInches) xNext *= 25.4;
    }
    else {
        xNext = xValueCurrent;
    }

    // Поиск Y
    QRegularExpressionMatch matchY = regexY.match(nextLine);
    if (matchY.hasMatch()) {
        yNext = matchY.captured(1).toDouble();
        if (inInches) yNext *= 25.4;
    }
    else {
        yNext = yValueCurrent;
    }

    // Поиск Z
    QRegularExpressionMatch matchZ = regexZ.match(nextLine);
    if (matchZ.hasMatch()) {
        zNext = matchZ.captured(1).toDouble();
        if (inInches) zNext *= 25.4;
    }
    else {
        zNext = zValueCurrent;
    }

    // Поиск F (Feed Rate)
    QRegularExpressionMatch matchF = regexF.match(nextLine);
    if (matchF.hasMatch()) {
        feedRateNext = matchF.captured(1).toInt();
    }
    else {
        feedRateNext = feedRate;
    }

    // Поиск S (Spindle Speed)
    QRegularExpressionMatch matchS = regexS.match(nextLine);
    if (matchS.hasMatch()) {
        spindleSpeedNext = matchS.captured(1).toInt();
    }
    else {
        spindleSpeedNext = spindleSpeed;
    }
}
