/********************************************************************************
** Form generated from reading UI file 'volume_control.ui'
**
** Created by: Qt User Interface Compiler version 6.11.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_VOLUME_CONTROL_H
#define UI_VOLUME_CONTROL_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_VolumeControlForm
{
public:
    QVBoxLayout *rootLayout;
    QLabel *headerLabel;
    QFrame *line;
    QLabel *masterLabel;
    QSlider *masterSlider;
    QLabel *outputLabel;
    QSlider *outputSlider;
    QLabel *gainLabel;
    QSlider *gainSlider;
    QLabel *inputSensitivityLabel;
    QSlider *inputSensitivitySlider;
    QLabel *noiseSuppressionLabel;
    QSlider *noiseSuppressionSlider;
    QFrame *line_2;
    QHBoxLayout *horizontalLayout;
    QCheckBox *autoGainCheckbox;
    QCheckBox *noiseSuppCheckbox;
    QCheckBox *echoCheckbox;
    QLabel *aecDelayLabel;
    QSlider *aecDelaySlider;
    QHBoxLayout *testMicLayout;
    QPushButton *testMicButton;
    QLabel *testStatusLabel;
    QProgressBar *micLevelBar;
    QPushButton *restoreDefaultsButton;

    void setupUi(QWidget *VolumeControlForm)
    {
        if (VolumeControlForm->objectName().isEmpty())
            VolumeControlForm->setObjectName("VolumeControlForm");
        VolumeControlForm->resize(425, 509);
        rootLayout = new QVBoxLayout(VolumeControlForm);
        rootLayout->setSpacing(12);
        rootLayout->setObjectName("rootLayout");
        rootLayout->setContentsMargins(12, 12, 12, 12);
        headerLabel = new QLabel(VolumeControlForm);
        headerLabel->setObjectName("headerLabel");

        rootLayout->addWidget(headerLabel);

        line = new QFrame(VolumeControlForm);
        line->setObjectName("line");
        line->setFrameShadow(QFrame::Plain);
        line->setFrameShape(QFrame::Shape::HLine);

        rootLayout->addWidget(line);

        masterLabel = new QLabel(VolumeControlForm);
        masterLabel->setObjectName("masterLabel");

        rootLayout->addWidget(masterLabel);

        masterSlider = new QSlider(VolumeControlForm);
        masterSlider->setObjectName("masterSlider");
        masterSlider->setMinimum(0);
        masterSlider->setMaximum(250);
        masterSlider->setValue(100);
        masterSlider->setOrientation(Qt::Horizontal);

        rootLayout->addWidget(masterSlider);

        outputLabel = new QLabel(VolumeControlForm);
        outputLabel->setObjectName("outputLabel");

        rootLayout->addWidget(outputLabel);

        outputSlider = new QSlider(VolumeControlForm);
        outputSlider->setObjectName("outputSlider");
        outputSlider->setMinimum(0);
        outputSlider->setMaximum(250);
        outputSlider->setValue(100);
        outputSlider->setOrientation(Qt::Horizontal);

        rootLayout->addWidget(outputSlider);

        gainLabel = new QLabel(VolumeControlForm);
        gainLabel->setObjectName("gainLabel");

        rootLayout->addWidget(gainLabel);

        gainSlider = new QSlider(VolumeControlForm);
        gainSlider->setObjectName("gainSlider");
        gainSlider->setMinimum(-20);
        gainSlider->setMaximum(20);
        gainSlider->setValue(0);
        gainSlider->setOrientation(Qt::Horizontal);

        rootLayout->addWidget(gainSlider);

        inputSensitivityLabel = new QLabel(VolumeControlForm);
        inputSensitivityLabel->setObjectName("inputSensitivityLabel");

        rootLayout->addWidget(inputSensitivityLabel);

        inputSensitivitySlider = new QSlider(VolumeControlForm);
        inputSensitivitySlider->setObjectName("inputSensitivitySlider");
        inputSensitivitySlider->setMinimum(0);
        inputSensitivitySlider->setMaximum(100);
        inputSensitivitySlider->setValue(45);
        inputSensitivitySlider->setOrientation(Qt::Horizontal);

        rootLayout->addWidget(inputSensitivitySlider);

        noiseSuppressionLabel = new QLabel(VolumeControlForm);
        noiseSuppressionLabel->setObjectName("noiseSuppressionLabel");

        rootLayout->addWidget(noiseSuppressionLabel);

        noiseSuppressionSlider = new QSlider(VolumeControlForm);
        noiseSuppressionSlider->setObjectName("noiseSuppressionSlider");
        noiseSuppressionSlider->setMinimum(0);
        noiseSuppressionSlider->setMaximum(100);
        noiseSuppressionSlider->setValue(65);
        noiseSuppressionSlider->setOrientation(Qt::Horizontal);

        rootLayout->addWidget(noiseSuppressionSlider);

        line_2 = new QFrame(VolumeControlForm);
        line_2->setObjectName("line_2");
        line_2->setFrameShadow(QFrame::Plain);
        line_2->setFrameShape(QFrame::Shape::HLine);

        rootLayout->addWidget(line_2);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName("horizontalLayout");
        autoGainCheckbox = new QCheckBox(VolumeControlForm);
        autoGainCheckbox->setObjectName("autoGainCheckbox");

        horizontalLayout->addWidget(autoGainCheckbox);

        noiseSuppCheckbox = new QCheckBox(VolumeControlForm);
        noiseSuppCheckbox->setObjectName("noiseSuppCheckbox");

        horizontalLayout->addWidget(noiseSuppCheckbox);

        echoCheckbox = new QCheckBox(VolumeControlForm);
        echoCheckbox->setObjectName("echoCheckbox");

        horizontalLayout->addWidget(echoCheckbox);


        rootLayout->addLayout(horizontalLayout);

        aecDelayLabel = new QLabel(VolumeControlForm);
        aecDelayLabel->setObjectName("aecDelayLabel");

        rootLayout->addWidget(aecDelayLabel);

        aecDelaySlider = new QSlider(VolumeControlForm);
        aecDelaySlider->setObjectName("aecDelaySlider");
        aecDelaySlider->setMinimum(0);
        aecDelaySlider->setMaximum(500);
        aecDelaySlider->setValue(180);
        aecDelaySlider->setOrientation(Qt::Horizontal);

        rootLayout->addWidget(aecDelaySlider);

        testMicLayout = new QHBoxLayout();
        testMicLayout->setObjectName("testMicLayout");
        testMicButton = new QPushButton(VolumeControlForm);
        testMicButton->setObjectName("testMicButton");

        testMicLayout->addWidget(testMicButton);

        testStatusLabel = new QLabel(VolumeControlForm);
        testStatusLabel->setObjectName("testStatusLabel");
        testStatusLabel->setMinimumSize(QSize(100, 0));

        testMicLayout->addWidget(testStatusLabel);


        rootLayout->addLayout(testMicLayout);

        micLevelBar = new QProgressBar(VolumeControlForm);
        micLevelBar->setObjectName("micLevelBar");
        micLevelBar->setTextVisible(false);

        rootLayout->addWidget(micLevelBar);

        restoreDefaultsButton = new QPushButton(VolumeControlForm);
        restoreDefaultsButton->setObjectName("restoreDefaultsButton");

        rootLayout->addWidget(restoreDefaultsButton);


        retranslateUi(VolumeControlForm);

        QMetaObject::connectSlotsByName(VolumeControlForm);
    } // setupUi

    void retranslateUi(QWidget *VolumeControlForm)
    {
        headerLabel->setText(QCoreApplication::translate("VolumeControlForm", "Audio Controls", nullptr));
        masterLabel->setText(QCoreApplication::translate("VolumeControlForm", "Master Volume", nullptr));
        outputLabel->setText(QCoreApplication::translate("VolumeControlForm", "Output Volume", nullptr));
        gainLabel->setText(QCoreApplication::translate("VolumeControlForm", "Gain (dB)", nullptr));
        inputSensitivityLabel->setText(QCoreApplication::translate("VolumeControlForm", "Input Sensitivity", nullptr));
        noiseSuppressionLabel->setText(QCoreApplication::translate("VolumeControlForm", "Noise Suppression", nullptr));
        autoGainCheckbox->setText(QCoreApplication::translate("VolumeControlForm", "Auto-Gain", nullptr));
        noiseSuppCheckbox->setText(QCoreApplication::translate("VolumeControlForm", "Noise Suppression", nullptr));
        echoCheckbox->setText(QCoreApplication::translate("VolumeControlForm", "Echo Cancellation", nullptr));
        aecDelayLabel->setText(QCoreApplication::translate("VolumeControlForm", "AEC Delay (ms)", nullptr));
        testMicButton->setText(QCoreApplication::translate("VolumeControlForm", "Test Mic", nullptr));
        testStatusLabel->setText(QString());
        restoreDefaultsButton->setText(QCoreApplication::translate("VolumeControlForm", "Restore Default", nullptr));
        (void)VolumeControlForm;
    } // retranslateUi

};

namespace Ui {
    class VolumeControlForm: public Ui_VolumeControlForm {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_VOLUME_CONTROL_H
