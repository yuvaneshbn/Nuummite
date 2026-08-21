/********************************************************************************
** Form generated from reading UI file 'main_window.ui'
**
** Created by: Qt User Interface Compiler version 6.11.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAIN_WINDOW_H
#define UI_MAIN_WINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindowForm
{
public:
    QVBoxLayout *rootLayout;
    QHBoxLayout *topToolbarLayout;
    QLabel *roomLabel;
    QComboBox *roomCombo;
    QPushButton *joinLeaveButton;
    QPushButton *refreshButton;
    QCheckBox *themeSwitch;
    QSpacerItem *topSpacer;
    QLabel *connectionIndicator;
    QHBoxLayout *contentLayout;
    QGroupBox *participantsGroup;
    QVBoxLayout *participantsLayout;
    QLineEdit *searchInput;
    QListWidget *participantList;
    QLabel *countLabel;
    QGroupBox *activeSpeakersGroup;
    QVBoxLayout *activeLayout;
    QLabel *titleLabel;
    QLabel *activeSpeakersLabel;
    QListWidget *speakerLogList;
    QLabel *systemAudioLabel;
    QProgressBar *systemLevelBar;
    QGroupBox *myControlsGroup;
    QVBoxLayout *myControlsLayout;
    QScrollArea *controlsScrollArea;
    QWidget *controlsScrollContent;
    QVBoxLayout *controlsPlaceholderLayout;
    QLabel *controlsHint;
    QHBoxLayout *bottomToolbarLayout;
    QSpacerItem *bottomSpacer;
    QPushButton *muteButton;
    QPushButton *broadcastButton;
    QPushButton *settingsButton;
    QLabel *warningLabel;
    QStatusBar *mainStatusBar;

    void setupUi(QWidget *MainWindowForm)
    {
        if (MainWindowForm->objectName().isEmpty())
            MainWindowForm->setObjectName("MainWindowForm");
        MainWindowForm->resize(810, 481);
        rootLayout = new QVBoxLayout(MainWindowForm);
        rootLayout->setObjectName("rootLayout");
        topToolbarLayout = new QHBoxLayout();
        topToolbarLayout->setObjectName("topToolbarLayout");
        roomLabel = new QLabel(MainWindowForm);
        roomLabel->setObjectName("roomLabel");

        topToolbarLayout->addWidget(roomLabel);

        roomCombo = new QComboBox(MainWindowForm);
        roomCombo->setObjectName("roomCombo");

        topToolbarLayout->addWidget(roomCombo);

        joinLeaveButton = new QPushButton(MainWindowForm);
        joinLeaveButton->setObjectName("joinLeaveButton");

        topToolbarLayout->addWidget(joinLeaveButton);

        refreshButton = new QPushButton(MainWindowForm);
        refreshButton->setObjectName("refreshButton");

        topToolbarLayout->addWidget(refreshButton);

        themeSwitch = new QCheckBox(MainWindowForm);
        themeSwitch->setObjectName("themeSwitch");
        themeSwitch->setChecked(true);

        topToolbarLayout->addWidget(themeSwitch);

        topSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        topToolbarLayout->addItem(topSpacer);

        connectionIndicator = new QLabel(MainWindowForm);
        connectionIndicator->setObjectName("connectionIndicator");

        topToolbarLayout->addWidget(connectionIndicator);


        rootLayout->addLayout(topToolbarLayout);

        contentLayout = new QHBoxLayout();
        contentLayout->setObjectName("contentLayout");
        participantsGroup = new QGroupBox(MainWindowForm);
        participantsGroup->setObjectName("participantsGroup");
        participantsLayout = new QVBoxLayout(participantsGroup);
        participantsLayout->setObjectName("participantsLayout");
        searchInput = new QLineEdit(participantsGroup);
        searchInput->setObjectName("searchInput");

        participantsLayout->addWidget(searchInput);

        participantList = new QListWidget(participantsGroup);
        participantList->setObjectName("participantList");

        participantsLayout->addWidget(participantList);

        countLabel = new QLabel(participantsGroup);
        countLabel->setObjectName("countLabel");

        participantsLayout->addWidget(countLabel);


        contentLayout->addWidget(participantsGroup);

        activeSpeakersGroup = new QGroupBox(MainWindowForm);
        activeSpeakersGroup->setObjectName("activeSpeakersGroup");
        activeLayout = new QVBoxLayout(activeSpeakersGroup);
        activeLayout->setObjectName("activeLayout");
        titleLabel = new QLabel(activeSpeakersGroup);
        titleLabel->setObjectName("titleLabel");

        activeLayout->addWidget(titleLabel);

        activeSpeakersLabel = new QLabel(activeSpeakersGroup);
        activeSpeakersLabel->setObjectName("activeSpeakersLabel");

        activeLayout->addWidget(activeSpeakersLabel);

        speakerLogList = new QListWidget(activeSpeakersGroup);
        speakerLogList->setObjectName("speakerLogList");

        activeLayout->addWidget(speakerLogList);

        systemAudioLabel = new QLabel(activeSpeakersGroup);
        systemAudioLabel->setObjectName("systemAudioLabel");

        activeLayout->addWidget(systemAudioLabel);

        systemLevelBar = new QProgressBar(activeSpeakersGroup);
        systemLevelBar->setObjectName("systemLevelBar");
        systemLevelBar->setTextVisible(false);

        activeLayout->addWidget(systemLevelBar);


        contentLayout->addWidget(activeSpeakersGroup);

        myControlsGroup = new QGroupBox(MainWindowForm);
        myControlsGroup->setObjectName("myControlsGroup");
        myControlsLayout = new QVBoxLayout(myControlsGroup);
        myControlsLayout->setObjectName("myControlsLayout");
        controlsScrollArea = new QScrollArea(myControlsGroup);
        controlsScrollArea->setObjectName("controlsScrollArea");
        controlsScrollArea->setFrameShape(QFrame::NoFrame);
        controlsScrollArea->setWidgetResizable(true);
        controlsScrollContent = new QWidget();
        controlsScrollContent->setObjectName("controlsScrollContent");
        controlsPlaceholderLayout = new QVBoxLayout(controlsScrollContent);
        controlsPlaceholderLayout->setObjectName("controlsPlaceholderLayout");
        controlsHint = new QLabel(controlsScrollContent);
        controlsHint->setObjectName("controlsHint");

        controlsPlaceholderLayout->addWidget(controlsHint);

        controlsScrollArea->setWidget(controlsScrollContent);

        myControlsLayout->addWidget(controlsScrollArea);


        contentLayout->addWidget(myControlsGroup);


        rootLayout->addLayout(contentLayout);

        bottomToolbarLayout = new QHBoxLayout();
        bottomToolbarLayout->setObjectName("bottomToolbarLayout");
        bottomSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        bottomToolbarLayout->addItem(bottomSpacer);

        muteButton = new QPushButton(MainWindowForm);
        muteButton->setObjectName("muteButton");

        bottomToolbarLayout->addWidget(muteButton);

        broadcastButton = new QPushButton(MainWindowForm);
        broadcastButton->setObjectName("broadcastButton");

        bottomToolbarLayout->addWidget(broadcastButton);

        settingsButton = new QPushButton(MainWindowForm);
        settingsButton->setObjectName("settingsButton");

        bottomToolbarLayout->addWidget(settingsButton);


        rootLayout->addLayout(bottomToolbarLayout);

        warningLabel = new QLabel(MainWindowForm);
        warningLabel->setObjectName("warningLabel");

        rootLayout->addWidget(warningLabel);

        mainStatusBar = new QStatusBar(MainWindowForm);
        mainStatusBar->setObjectName("mainStatusBar");

        rootLayout->addWidget(mainStatusBar);


        retranslateUi(MainWindowForm);

        QMetaObject::connectSlotsByName(MainWindowForm);
    } // setupUi

    void retranslateUi(QWidget *MainWindowForm)
    {
        MainWindowForm->setWindowTitle(QCoreApplication::translate("MainWindowForm", "Nuummite - Voice Chat Client", nullptr));
        roomLabel->setText(QCoreApplication::translate("MainWindowForm", "Room:", nullptr));
        joinLeaveButton->setText(QCoreApplication::translate("MainWindowForm", "Leave Room", nullptr));
        refreshButton->setText(QCoreApplication::translate("MainWindowForm", "Refresh List", nullptr));
        themeSwitch->setText(QCoreApplication::translate("MainWindowForm", "Dark", nullptr));
#if QT_CONFIG(tooltip)
        themeSwitch->setToolTip(QCoreApplication::translate("MainWindowForm", "Toggle light/dark theme", nullptr));
#endif // QT_CONFIG(tooltip)
        connectionIndicator->setText(QCoreApplication::translate("MainWindowForm", "Connected", nullptr));
        participantsGroup->setTitle(QCoreApplication::translate("MainWindowForm", "Participants", nullptr));
        searchInput->setPlaceholderText(QCoreApplication::translate("MainWindowForm", "Search by name/ID...", nullptr));
        countLabel->setText(QCoreApplication::translate("MainWindowForm", "0 / 0 shown", nullptr));
        activeSpeakersGroup->setTitle(QCoreApplication::translate("MainWindowForm", "Active Speakers", nullptr));
        titleLabel->setText(QCoreApplication::translate("MainWindowForm", "Nuummite - Voice Connected", nullptr));
        activeSpeakersLabel->setText(QCoreApplication::translate("MainWindowForm", "----------------", nullptr));
        systemAudioLabel->setText(QCoreApplication::translate("MainWindowForm", "System Audio Level", nullptr));
        myControlsGroup->setTitle(QCoreApplication::translate("MainWindowForm", "My Controls", nullptr));
        controlsHint->setText(QCoreApplication::translate("MainWindowForm", "Embed VolumeControl form here", nullptr));
        muteButton->setText(QCoreApplication::translate("MainWindowForm", "Mute Mic", nullptr));
        broadcastButton->setText(QCoreApplication::translate("MainWindowForm", "Broadcast", nullptr));
        settingsButton->setText(QCoreApplication::translate("MainWindowForm", "Settings", nullptr));
        warningLabel->setText(QString());
    } // retranslateUi

};

namespace Ui {
    class MainWindowForm: public Ui_MainWindowForm {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAIN_WINDOW_H
