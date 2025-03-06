#ifndef Antonov_H
#define Antonov_H

#include <QMainWindow>
#include <QListWidgetItem>
#include <QTimer>
#include <QLabel>
#include <QSet>
#include <QGraphicsScene>
#include <stack>
#include <QTableWidget>
#include <QDebug>
#include <QGuiApplication>
#include <QPushButton>
#include <QDockWidget>
#include <QGraphicsPolygonItem>
#include <QGraphicsPathItem>
#include <QPainterPath>
#include <QQueue>
#include <QString>
#include <QThread>

QT_BEGIN_NAMESPACE
namespace Ui { class AntonovClass; }
QT_END_NAMESPACE

class Antonov : public QMainWindow {
    Q_OBJECT

        enum MockType {
        MOCK_BASIC,
        MOCK_ARC,
        MOCK_COMPLEX
    };

public:
    Antonov(QWidget* parent = nullptr);
    ~Antonov();

private slots:
    void updateTime();
    void handleNumeration();
    void handleSmenaEkrana();
    void handleMmDyum();
    void handleChangeSK();
    void handlePriv();
    void handleStartKadr();
    void handleKorrekt();
    void handleSmesh();
    void handleBackK2();
    void moveSelection(int rowOffset, int colOffset, QTableWidget* table);
    void handleMoveUp();
    void handleMoveDown();
    void handleMoveLeft();
    void handleMoveRight();
    void handleSelectCell();
    void handleConfirm();
    void handleCancel();
    void handleBack();
    void handleStart();
    void keyPressEvent(QKeyEvent* event);
    void updateLanguageLabel();
    void handleStop();
    void handleReset();
    void loadMockProgram(MockType type);
    void handleResetAlarm();
    void loadProgram();
    void logEvent(const QString& message);
    void loadRealProgram();
    void handleSpindleSpeedChange();
    void handleFeedRateChange();
    void updateProgressBar();
    void updateProgramStatus();
    void updateUnits();

private:
    Ui::AntonovClass* ui;
    QTimer* timer;
    QGraphicsScene* scene = nullptr;
    QGraphicsPolygonItem* tool = nullptr;
    QGraphicsPathItem* pathItem = nullptr;
    QPainterPath toolPath;
    QTimer* moveTimer;
    QPointF targetPosition;
    QList<QPointF> trajectoryPoints;
    QQueue<QString> commandQueue;
    QTimer* stepTimer;
    QTimer* toolTimer;
    int currentPointIndex = 0;
    std::stack<int> pageHistory;

    int feedUnitIndex;
    int programProgress;
    int currentStep;
    QTimer* simulationTimer;
    int currentRow = 0;
    int currentColumn = 0;
    bool programRunning;
    bool inInches;
    int currentProgramRow;
    int spindleSpeed;
    int feedRate;

    // Переменные для плоскостей (G17, G18, G19)
    QString activePlane;
    const QString XY_PLANE = "XY";
    const QString XZ_PLANE = "XZ";
    const QString YZ_PLANE = "YZ";

    // Смещения координат (G54-G59)
    double xOffset, yOffset, zOffset;

    // Компенсация инструмента (G40, G41, G42)
    double toolCompensation;
    const double LEFT = -1.0;
    const double RIGHT = 1.0;

    // Режимы сверления (G81-G89)
    QString drillMode;
    const QString PECK_DRILLING = "PECK";
    const QString BORING = "BORING";

    double feedRateMultiplier;
    double spindleSpeedMultiplier;
    double xValueCurrent;
    double yValueCurrent;
    double zValueCurrent;
    double xValueFinal;
    double yValueFinal;
    double zValueFinal;
    double currentX = 0;
    double currentY = 0;
    QStringList loadedProgram;

    QString coordinateSystem;
    QString feedUnit;
    QString interfaceLanguage;
    QString currentSK;
    QString previousValue;
    QPushButton* button_docker;
    QDockWidget* dockWidget;
    QDockWidget* dockWidget_2;
    QDockWidget* dockWidget_3;
    QDockWidget* dockWidget_4;
    QDockWidget* dockWidget_5;
    QDockWidget* dockWidget_6;
    QDockWidget* dockWidget_7;
    QDockWidget* dockWidget_8;
    bool startFromRowEnabled;

    void switchToCorrectionScreen();
    void switchToMainScreen();
    void handleCorrectionUp();
    void handleCorrectionDown();
    void handleCorrectionSelect();
    void handleCorrectionConfirm();
    void handleCorrectionCancel();
    void updateStatusBar(int value);
    void highlightCurrentProgramRow(int row);
    void analyzeProgram();
    void extractCoordinatesAndSpeed(const QString& line, bool isNext = false);
    void drawArc(double xStart, double yStart, double xEnd, double yEnd, double radius, bool clockwise);
    bool extractArcData(const QString& command, double& x, double& y, double& r);
    void parseAndDrawTrajectory(const QString& line);
    void updateToolPosition(double x, double y);
    void startAnimation();
    void moveToolSmoothly(double xEnd, double yEnd);
    void updateToolAnimation();
    void drawToolPath(double x, double y);
    void drawToolPath(double xStart, double yStart, double xEnd, double yEnd);
    void clearToolPath();
    void loadGCode(const QStringList& commands);
    void executeNextStep();
    bool extractCoordinates(const QString& command, double& x, double& y, double& z);
    bool parseGCode(const QString& command, double& x, double& y);
    bool parseArcGCode(const QString& command, double& x, double& y, double& r);
    void moveTriangleTo(double x, double y);
    void on_startButton_clicked();
    void processCommand(const QString& command);
    double extractCoordinate(const QString& command, QChar axis);
    void updateToolStep();
    void resetCoordinateOffsets();
    void updateCoordinatesDisplay();
    void simulateNextStep();
    void returnToReferencePoint(const QString& command);
    void applyToolCompensation(const QString& command);
    void setCoordinateOffsets(const QString& command);
    void applyToolLengthOffset(const QString& command);
    void setDrillCycleMode(const QString& mode);
    void animateToolMovement(double xStart, double yStart, double xEnd, double yEnd);
    void extractNextCoordinates(int currentStep, double& xNext, double& yNext, double& zNext, int& feedRateNext, int& spindleSpeedNext);
    void displayActiveFunctions();
    bool analyzeProgramForErrors();
    void changeCoordinateSystem();
    void drawLineTo(double x, double y);
    void drawArcTo(double xEnd, double yEnd, double radius, bool clockwise);
    void changeFeedUnit();
    void switchToSecondScreen();
    void syncLanguage();
    void updateInfoLine(const QString& message);
    void startExecution();

    // G-коды и специальные команды
    int extractDwellTime(const QString& command);  // G4 (задержка)
    void cancelToolCompensation();  // G40 (отключение коррекции инструмента)
};

#endif // Antonov_H
