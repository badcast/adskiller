/********************************************************************************
** Modern pure C++ UI declaration for MainWindow
** Converted from legacy .ui form to native Qt C++ code
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDateEdit>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListView>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTableView>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QToolBox>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QAction *actionClose;
    QAction *actionAboutUs;
    QAction *actionUsLic;
    QAction *action_WhatsApp;
    QAction *action_Qt;
    QAction *mThemeLight;
    QAction *mThemeDark;
    QAction *mThemeSystem;
    QWidget *centralwidget;
    QGridLayout *centralwidget_Layout;
    QFrame *aiToolBoxContainer;
    QHBoxLayout *horizontalLayout_ai;
    QToolBox *aiToolBox;
    QWidget *aiToolBoxPage1;
    QGridLayout *gridLayout_9;
    QPushButton *aiChatSend;
    QTextEdit *aiChatMessages;
    QTextEdit *aiChatEdit;
    QWidget *aiToolBoxPage2;
    QGridLayout *gridLayout;
    QTextEdit *aboutAi_edit;
    QPushButton *aiToolBoxToggle;
    QFrame *contentLayout;
    QGridLayout *gridLayout_10;
    QTabWidget *tabWidget;
    QWidget *page_auth;
    QGridLayout *gridLayout_asd2;
    QFrame *frame_4;
    QLabel *statusAuthText;
    QLineEdit *linePassEdit;
    QLabel *label_4;
    QCheckBox *checkAutoLogin;
    QPushButton *authButton;
    QFrame *mainapplogo;
    QLabel *label_2;
    QLineEdit *lineLoginEdit;
    QLabel *label_12;
    QLabel *label_14;
    QPushButton *butShowPass;
    QLabel *label_auth_ver;
    QWidget *page_cabinet;
    QVBoxLayout *cabinetMainLayout;
    QFrame *toplevel_up;
    QHBoxLayout *toplevel_layout_auth;
    QPushButton *logoutButton;
    QSpacerItem *horizontalSpacer_10;
    QLabel *label_6;
    QSpacerItem *horizontalSpacer_11;
    QPushButton *authpageUpdate;
    QHBoxLayout *horizontalLayout;
    QSpacerItem *horizontalSpacer_8;
    QFrame *frame_7;
    QFrame *authedMainWin;
    QVBoxLayout *verticalLayout_3;
    QFrame *frame_3;
    QLabel *labelLoginAuthed;
    QFrame *frame_5;
    QGridLayout *gridLayout_7;
    QLabel *labelCredits;
    QFrame *frame_6;
    QGridLayout *gridLayout_8;
    QLabel *labelVipDays;
    QSpacerItem *horizontalSpacer_9;
    QFrame *toplevel_up_2;
    QHBoxLayout *toplevel_layout_auth_2;
    QLabel *label_7;
    QScrollArea *scrollArea_3;
    QWidget *scrollAreaWidgetContents_3;
    QVBoxLayout *verticalLayout;
    QHBoxLayout *sss;
    QSpacerItem *horizontalSpacer_3;
    QFrame *serviceContents;
    QGridLayout *layoutSpace;
    QPushButton *button_RemoveADSMalware_19;
    QPushButton *button_RemoveADSMalware_18;
    QSpacerItem *horizontalSpacer_33;
    QTableView *authInfo;
    QWidget *page_devices;
    QHBoxLayout *hboxLayout;
    QSpacerItem *horizontalSpacer_2;
    QFrame *device_left_group;
    QVBoxLayout *vboxLayout;
    QScrollArea *scrollArea_2;
    QWidget *scrollAreaWidgetContents_2;
    QGridLayout *gridLayout_2;
    QLabel *label;
    QLabel *label_3;
    QLabel *label_5;
    QVBoxLayout *device_right_group;
    QSpacerItem *horizontalSpacer;
    QWidget *page_adsmalware;
    QGridLayout *gridLayout_asd21;
    QFrame *frame22;
    QHBoxLayout *centralVertical;
    QGridLayout *gridLayout_11;
    QListView *processLogStatus;
    QProgressBar *processBarStatus;
    QLabel *deviceLabelName;
    QGridLayout *gridLayout1;
    QFrame *frame333;
    QGridLayout *layoutShowCircle;
    QFrame *frame33;
    QVBoxLayout *progressCircleLayout;
    QLabel *malwareStatusText0;
    QPushButton *malwareReRun;
    QWidget *page_loader;
    QGridLayout *gridLayout_5;
    QFrame *frame_loader;
    QGridLayout *gridLayout_4;
    QLabel *loaderPageText;
    QVBoxLayout *loaderLayout;
    QWidget *tab;
    QFrame *toplevel_backpage;
    QHBoxLayout *toplevel_layout_auth_3;
    QPushButton *buttonBackTo;
    QLabel *label_8;
    QWidget *page_mydevices;
    QVBoxLayout *vboxLayout1;
    QHBoxLayout *verticalLayout_2;
    QLabel *label_9;
    QDateEdit *myDeviceFilterDateStart;
    QLabel *label_10;
    QDateEdit *myDeviceFilterDateEnd;
    QCheckBox *myDeviceQuaranteeFilter;
    QSpacerItem *horizontalSpacer_4;
    QPushButton *myDeviceSend;
    QTableView *myDeviceActual;
    QLabel *myDevicePageLabel;
    QWidget *page_buyvip;
    QGridLayout *gridLayout_3;
    QSpacerItem *horizontalSpacer_5;
    QGroupBox *groupBox;
    QVBoxLayout *verticalLayout_4;
    QLabel *label_13;
    QLabel *labelVipBalance;
    QComboBox *comboBoxSelectVIPDays;
    QFrame *frame;
    QHBoxLayout *horizontalLayout_4;
    QLabel *labelInfoVip;
    QSpacerItem *horizontalSpacer_7;
    QPushButton *buttonBuyVip;
    QSpacerItem *verticalSpacer_4;
    QSpacerItem *horizontalSpacer_6;
    QSpacerItem *verticalSpacer_5;
    QFrame *topcontent;
    QVBoxLayout *vboxLayout2;
    QMenuBar *menubar;
    QMenu *menu;
    QMenu *menu_2;
    QMenu *menu_3;
    QMenu *menu_4;

    void setupUi(QMainWindow *MainWindow)
    {
        if(MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->setEnabled(true);
        MainWindow->resize(1180, 680);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(MainWindow->sizePolicy().hasHeightForWidth());
        MainWindow->setSizePolicy(sizePolicy);
        MainWindow->setMinimumSize(QSize(1060, 600));
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/resources/app-logo"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        MainWindow->setWindowIcon(icon);
        MainWindow->setStyleSheet(QString::fromUtf8(""));
        actionClose = new QAction(MainWindow);
        actionClose->setObjectName("actionClose");
        actionAboutUs = new QAction(MainWindow);
        actionAboutUs->setObjectName("actionAboutUs");
        actionUsLic = new QAction(MainWindow);
        actionUsLic->setObjectName("actionUsLic");
        action_WhatsApp = new QAction(MainWindow);
        action_WhatsApp->setObjectName("action_WhatsApp");
        QIcon icon1(QIcon::fromTheme(QIcon::ThemeIcon::CallStart));
        action_WhatsApp->setIcon(icon1);
        action_Qt = new QAction(MainWindow);
        action_Qt->setObjectName("action_Qt");
        mThemeLight = new QAction(MainWindow);
        mThemeLight->setObjectName("mThemeLight");
        mThemeLight->setCheckable(true);
        mThemeLight->setChecked(true);
        mThemeLight->setIconVisibleInMenu(true);
        mThemeDark = new QAction(MainWindow);
        mThemeDark->setObjectName("mThemeDark");
        mThemeDark->setCheckable(true);
        mThemeSystem = new QAction(MainWindow);
        mThemeSystem->setObjectName("mThemeSystem");
        mThemeSystem->setCheckable(true);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");
        QSizePolicy sizePolicy1(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Expanding);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(centralwidget->sizePolicy().hasHeightForWidth());
        centralwidget->setSizePolicy(sizePolicy1);
        centralwidget_Layout = new QGridLayout(centralwidget);
        centralwidget_Layout->setSpacing(0);
        centralwidget_Layout->setObjectName("centralwidget_Layout");
        centralwidget_Layout->setContentsMargins(0, 0, 0, 0);
        aiToolBoxContainer = new QFrame(centralwidget);
        aiToolBoxContainer->setObjectName("aiToolBoxContainer");
        horizontalLayout_ai = new QHBoxLayout(aiToolBoxContainer);
        horizontalLayout_ai->setObjectName("horizontalLayout_ai");
        aiToolBox = new QToolBox(aiToolBoxContainer);
        aiToolBox->setObjectName("aiToolBox");
        aiToolBox->setMinimumSize(QSize(0, 0));
        aiToolBox->setMaximumSize(QSize(16777215, 16777215));
        aiToolBox->setFrameShape(QFrame::Shape::Panel);
        aiToolBox->setFrameShadow(QFrame::Shadow::Sunken);
        aiToolBoxPage1 = new QWidget();
        aiToolBoxPage1->setObjectName("aiToolBoxPage1");
        aiToolBoxPage1->setGeometry(QRect(0, 0, 408, 573));
        gridLayout_9 = new QGridLayout(aiToolBoxPage1);
        gridLayout_9->setObjectName("gridLayout_9");
        aiChatSend = new QPushButton(aiToolBoxPage1);
        aiChatSend->setObjectName("aiChatSend");
        aiChatSend->setMinimumSize(QSize(0, 32));

        gridLayout_9->addWidget(aiChatSend, 1, 1, 1, 1);

        aiChatMessages = new QTextEdit(aiToolBoxPage1);
        aiChatMessages->setObjectName("aiChatMessages");
        aiChatMessages->setReadOnly(true);

        gridLayout_9->addWidget(aiChatMessages, 0, 0, 1, 2);

        aiChatEdit = new QTextEdit(aiToolBoxPage1);
        aiChatEdit->setObjectName("aiChatEdit");
        aiChatEdit->setMaximumSize(QSize(16777215, 50));

        gridLayout_9->addWidget(aiChatEdit, 1, 0, 1, 1);

        aiToolBox->addItem(aiToolBoxPage1, QString::fromUtf8("\320\230\320\230 \320\220\321\201\320\270\321\201\321\202\320\265\320\275\321\202 - AdsKiller"));
        aiToolBoxPage2 = new QWidget();
        aiToolBoxPage2->setObjectName("aiToolBoxPage2");
        aiToolBoxPage2->setGeometry(QRect(0, 0, 408, 573));
        gridLayout = new QGridLayout(aiToolBoxPage2);
        gridLayout->setObjectName("gridLayout");
        aboutAi_edit = new QTextEdit(aiToolBoxPage2);
        aboutAi_edit->setObjectName("aboutAi_edit");
        aboutAi_edit->setEnabled(true);
        aboutAi_edit->setUndoRedoEnabled(false);
        aboutAi_edit->setReadOnly(true);

        gridLayout->addWidget(aboutAi_edit, 0, 0, 1, 1);

        aiToolBox->addItem(aiToolBoxPage2, QString::fromUtf8("\320\236 \320\230\320\230"));

        horizontalLayout_ai->addWidget(aiToolBox);

        aiToolBoxToggle = new QPushButton(aiToolBoxContainer);
        aiToolBoxToggle->setObjectName("aiToolBoxToggle");
        aiToolBoxToggle->setMinimumSize(QSize(24, 0));
        aiToolBoxToggle->setMaximumSize(QSize(24, 16777215));
        aiToolBoxToggle->setText(QString::fromUtf8(""));
        QIcon icon2(QIcon::fromTheme(QIcon::ThemeIcon::DocumentSend));
        aiToolBoxToggle->setIcon(icon2);

        horizontalLayout_ai->addWidget(aiToolBoxToggle);

        centralwidget_Layout->addWidget(aiToolBoxContainer, 0, 1, 2, 1);

        contentLayout = new QFrame(centralwidget);
        contentLayout->setObjectName("contentLayout");
        gridLayout_10 = new QGridLayout(contentLayout);
        gridLayout_10->setSpacing(0);
        gridLayout_10->setObjectName("gridLayout_10");
        gridLayout_10->setContentsMargins(0, 0, 0, 0);
        tabWidget = new QTabWidget(contentLayout);
        tabWidget->setObjectName("tabWidget");
        page_auth = new QWidget();
        page_auth->setObjectName("page_auth");
        gridLayout_asd2 = new QGridLayout(page_auth);
        gridLayout_asd2->setSpacing(0);
        gridLayout_asd2->setObjectName("gridLayout_asd2");
        gridLayout_asd2->setContentsMargins(0, 0, 0, 0);
        frame_4 = new QFrame(page_auth);
        frame_4->setObjectName("frame_4");
        frame_4->setMinimumSize(QSize(440, 560));
        frame_4->setMaximumSize(QSize(440, 560));
        frame_4->setFrameShape(QFrame::Shape::NoFrame);
        frame_4->setFrameShadow(QFrame::Shadow::Raised);
        statusAuthText = new QLabel(frame_4);
        statusAuthText->setObjectName("statusAuthText");
        statusAuthText->setGeometry(QRect(90, 356, 461, 51));
        statusAuthText->setAlignment(Qt::AlignmentFlag::AlignLeading | Qt::AlignmentFlag::AlignLeft | Qt::AlignmentFlag::AlignTop);
        statusAuthText->setWordWrap(true);
        linePassEdit = new QLineEdit(frame_4);
        linePassEdit->setObjectName("linePassEdit");
        linePassEdit->setGeometry(QRect(90, 294, 390, 26));
        QFont font;
        font.setPointSize(8);
        linePassEdit->setFont(font);
        linePassEdit->setMaxLength(64);
        linePassEdit->setEchoMode(QLineEdit::EchoMode::Password);
        label_4 = new QLabel(frame_4);
        label_4->setObjectName("label_4");
        label_4->setGeometry(QRect(90, 200, 361, 18));
        checkAutoLogin = new QCheckBox(frame_4);
        checkAutoLogin->setObjectName("checkAutoLogin");
        checkAutoLogin->setGeometry(QRect(90, 326, 191, 23));
        checkAutoLogin->setChecked(true);
        authButton = new QPushButton(frame_4);
        authButton->setObjectName("authButton");
        authButton->setGeometry(QRect(440, 326, 111, 26));
        mainapplogo = new QFrame(frame_4);
        mainapplogo->setObjectName("mainapplogo");
        mainapplogo->setGeometry(QRect(200, 10, 221, 141));
        mainapplogo->setStyleSheet(QString::fromUtf8("image: url(:/resources/app-logo);"));
        mainapplogo->setFrameShape(QFrame::Shape::NoFrame);
        mainapplogo->setFrameShadow(QFrame::Shadow::Plain);
        label_2 = new QLabel(frame_4);
        label_2->setObjectName("label_2");
        label_2->setGeometry(QRect(300, 330, 131, 20));
        label_2->setOpenExternalLinks(false);
        label_2->setTextInteractionFlags(Qt::TextInteractionFlag::LinksAccessibleByKeyboard | Qt::TextInteractionFlag::LinksAccessibleByMouse | Qt::TextInteractionFlag::TextBrowserInteraction | Qt::TextInteractionFlag::TextSelectableByKeyboard | Qt::TextInteractionFlag::TextSelectableByMouse);
        lineLoginEdit = new QLineEdit(frame_4);
        lineLoginEdit->setObjectName("lineLoginEdit");
        lineLoginEdit->setGeometry(QRect(90, 240, 461, 26));
        lineLoginEdit->setFont(font);
        lineLoginEdit->setMaxLength(64);
        lineLoginEdit->setEchoMode(QLineEdit::EchoMode::Normal);
        label_12 = new QLabel(frame_4);
        label_12->setObjectName("label_12");
        label_12->setGeometry(QRect(90, 216, 58, 18));
        label_14 = new QLabel(frame_4);
        label_14->setObjectName("label_14");
        label_14->setGeometry(QRect(90, 270, 58, 18));
        butShowPass = new QPushButton(frame_4);
        butShowPass->setObjectName("butShowPass");
        butShowPass->setGeometry(QRect(480, 294, 71, 26));
        label_auth_ver = new QLabel(frame_4);
        label_auth_ver->setObjectName("label_auth_ver");
        label_auth_ver->setGeometry(QRect(510, 380, 101, 20));

        gridLayout_asd2->addWidget(frame_4, 0, 0, 1, 1);

        tabWidget->addTab(page_auth, QString());
        page_cabinet = new QWidget();
        page_cabinet->setObjectName("page_cabinet");
        cabinetMainLayout = new QVBoxLayout(page_cabinet);
        cabinetMainLayout->setSpacing(0);
        cabinetMainLayout->setObjectName("cabinetMainLayout");
        cabinetMainLayout->setContentsMargins(0, 0, 0, 0);
        toplevel_up = new QFrame(page_cabinet);
        toplevel_up->setObjectName("toplevel_up");
        toplevel_up->setStyleSheet(QString::fromUtf8(""));
        toplevel_up->setFrameShape(QFrame::Shape::NoFrame);
        toplevel_up->setFrameShadow(QFrame::Shadow::Raised);
        toplevel_up->setLineWidth(0);
        toplevel_layout_auth = new QHBoxLayout(toplevel_up);
        toplevel_layout_auth->setSpacing(4);
        toplevel_layout_auth->setObjectName("toplevel_layout_auth");
        toplevel_layout_auth->setContentsMargins(0, 0, 0, 0);
        logoutButton = new QPushButton(toplevel_up);
        logoutButton->setObjectName("logoutButton");
        QIcon icon3(QIcon::fromTheme(QIcon::ThemeIcon::SystemLogOut));
        logoutButton->setIcon(icon3);

        toplevel_layout_auth->addWidget(logoutButton);

        horizontalSpacer_10 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        toplevel_layout_auth->addItem(horizontalSpacer_10);

        label_6 = new QLabel(toplevel_up);
        label_6->setObjectName("label_6");
        QFont font1;
        font1.setPointSize(14);
        font1.setBold(false);
        font1.setItalic(true);
        label_6->setFont(font1);
        label_6->setStyleSheet(
            QString::fromUtf8(
                "color: white;\n"
                "                                background: transparent"));
        label_6->setOpenExternalLinks(true);

        toplevel_layout_auth->addWidget(label_6);

        horizontalSpacer_11 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        toplevel_layout_auth->addItem(horizontalSpacer_11);

        authpageUpdate = new QPushButton(toplevel_up);
        authpageUpdate->setObjectName("authpageUpdate");
        QIcon icon4(QIcon::fromTheme(QIcon::ThemeIcon::SystemReboot));
        authpageUpdate->setIcon(icon4);

        toplevel_layout_auth->addWidget(authpageUpdate);

        cabinetMainLayout->addWidget(toplevel_up);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setSpacing(0);
        horizontalLayout->setObjectName("horizontalLayout");
        horizontalLayout->setContentsMargins(5, 5, 5, 5);
        horizontalSpacer_8 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_8);

        frame_7 = new QFrame(page_cabinet);
        frame_7->setObjectName("frame_7");
        frame_7->setMinimumSize(QSize(500, 0));
        frame_7->setMaximumSize(QSize(16777215, 120));
        frame_7->setFrameShape(QFrame::Shape::StyledPanel);
        frame_7->setFrameShadow(QFrame::Shadow::Raised);
        authedMainWin = new QFrame(frame_7);
        authedMainWin->setObjectName("authedMainWin");
        authedMainWin->setGeometry(QRect(107, 10, 300, 150));
        authedMainWin->setMinimumSize(QSize(300, 150));
        authedMainWin->setMaximumSize(QSize(300, 150));
        authedMainWin->setFrameShape(QFrame::Shape::NoFrame);
        authedMainWin->setFrameShadow(QFrame::Shadow::Raised);
        verticalLayout_3 = new QVBoxLayout(authedMainWin);
        verticalLayout_3->setSpacing(0);
        verticalLayout_3->setObjectName("verticalLayout_3");
        verticalLayout_3->setContentsMargins(2, 2, 2, 2);
        frame_3 = new QFrame(authedMainWin);
        frame_3->setObjectName("frame_3");
        QSizePolicy sizePolicy2(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(frame_3->sizePolicy().hasHeightForWidth());
        frame_3->setSizePolicy(sizePolicy2);
        frame_3->setMinimumSize(QSize(100, 0));
        frame_3->setMaximumSize(QSize(16777215, 90));
        frame_3->setStyleSheet(QString::fromUtf8("image: url(:/resources/no-avatar);"));
        frame_3->setFrameShape(QFrame::Shape::NoFrame);
        frame_3->setFrameShadow(QFrame::Shadow::Raised);

        verticalLayout_3->addWidget(frame_3);

        labelLoginAuthed = new QLabel(authedMainWin);
        labelLoginAuthed->setObjectName("labelLoginAuthed");
        labelLoginAuthed->setMaximumSize(QSize(16777215, 50));
        QFont font2;
        font2.setPointSize(16);
        font2.setBold(true);
        font2.setUnderline(false);
        labelLoginAuthed->setFont(font2);
        labelLoginAuthed->setLineWidth(0);
        labelLoginAuthed->setAlignment(Qt::AlignmentFlag::AlignCenter);

        verticalLayout_3->addWidget(labelLoginAuthed);

        frame_5 = new QFrame(frame_7);
        frame_5->setObjectName("frame_5");
        frame_5->setGeometry(QRect(330, 50, 150, 80));
        frame_5->setMinimumSize(QSize(150, 80));
        frame_5->setMaximumSize(QSize(16777215, 80));
        frame_5->setFrameShape(QFrame::Shape::NoFrame);
        frame_5->setFrameShadow(QFrame::Shadow::Raised);
        gridLayout_7 = new QGridLayout(frame_5);
        gridLayout_7->setObjectName("gridLayout_7");
        gridLayout_7->setContentsMargins(2, 2, 2, 2);
        labelCredits = new QLabel(frame_5);
        labelCredits->setObjectName("labelCredits");
        labelCredits->setMaximumSize(QSize(16777215, 50));
        QFont font3;
        font3.setPointSize(10);
        font3.setBold(true);
        font3.setUnderline(false);
        labelCredits->setFont(font3);
        labelCredits->setAutoFillBackground(false);
        labelCredits->setLineWidth(0);
        labelCredits->setAlignment(Qt::AlignmentFlag::AlignCenter);

        gridLayout_7->addWidget(labelCredits, 2, 0, 1, 1);

        frame_6 = new QFrame(frame_7);
        frame_6->setObjectName("frame_6");
        frame_6->setGeometry(QRect(20, 50, 150, 80));
        frame_6->setMinimumSize(QSize(150, 80));
        frame_6->setMaximumSize(QSize(16777215, 80));
        frame_6->setFrameShape(QFrame::Shape::NoFrame);
        frame_6->setFrameShadow(QFrame::Shadow::Raised);
        gridLayout_8 = new QGridLayout(frame_6);
        gridLayout_8->setObjectName("gridLayout_8");
        gridLayout_8->setContentsMargins(2, 2, 2, 2);
        labelVipDays = new QLabel(frame_6);
        labelVipDays->setObjectName("labelVipDays");
        labelVipDays->setMaximumSize(QSize(16777215, 50));
        labelVipDays->setFont(font3);
        labelVipDays->setLineWidth(0);
        labelVipDays->setAlignment(Qt::AlignmentFlag::AlignCenter);

        gridLayout_8->addWidget(labelVipDays, 2, 0, 1, 1);

        horizontalLayout->addWidget(frame_7);

        horizontalSpacer_9 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_9);

        cabinetMainLayout->addLayout(horizontalLayout);

        toplevel_up_2 = new QFrame(page_cabinet);
        toplevel_up_2->setObjectName("toplevel_up_2");
        toplevel_up_2->setMaximumSize(QSize(16777215, 36));
        toplevel_up_2->setStyleSheet(QString::fromUtf8("background-color: #0B0F19; border-bottom: 1px solid #1E293B;"));
        toplevel_up_2->setFrameShape(QFrame::Shape::NoFrame);
        toplevel_up_2->setFrameShadow(QFrame::Shadow::Raised);
        toplevel_layout_auth_2 = new QHBoxLayout(toplevel_up_2);
        toplevel_layout_auth_2->setObjectName("toplevel_layout_auth_2");
        toplevel_layout_auth_2->setContentsMargins(10, 5, 10, 5);
        label_7 = new QLabel(toplevel_up_2);
        label_7->setObjectName("label_7");
        label_7->setFont(font1);
        label_7->setStyleSheet(
            QString::fromUtf8(
                "color: white;\n"
                "                                        background: transparent"));
        label_7->setOpenExternalLinks(true);

        toplevel_layout_auth_2->addWidget(label_7);

        cabinetMainLayout->addWidget(toplevel_up_2);

        scrollArea_3 = new QScrollArea(page_cabinet);
        scrollArea_3->setObjectName("scrollArea_3");
        scrollArea_3->setFrameShape(QFrame::Shape::NoFrame);
        scrollArea_3->setFrameShadow(QFrame::Shadow::Raised);
        scrollArea_3->setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOn);
        scrollArea_3->setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
        scrollArea_3->setSizeAdjustPolicy(QAbstractScrollArea::SizeAdjustPolicy::AdjustToContents);
        scrollArea_3->setWidgetResizable(true);
        scrollAreaWidgetContents_3 = new QWidget();
        scrollAreaWidgetContents_3->setObjectName("scrollAreaWidgetContents_3");
        scrollAreaWidgetContents_3->setGeometry(QRect(0, 0, 704, 572));
        verticalLayout = new QVBoxLayout(scrollAreaWidgetContents_3);
        verticalLayout->setSpacing(0);
        verticalLayout->setObjectName("verticalLayout");
        verticalLayout->setContentsMargins(0, 0, 0, 0);
        sss = new QHBoxLayout();
        sss->setObjectName("sss");
        horizontalSpacer_3 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        sss->addItem(horizontalSpacer_3);

        serviceContents = new QFrame(scrollAreaWidgetContents_3);
        serviceContents->setObjectName("serviceContents");
        layoutSpace = new QGridLayout(serviceContents);
        layoutSpace->setSpacing(5);
        layoutSpace->setObjectName("layoutSpace");
        layoutSpace->setContentsMargins(5, 5, 5, 5);
        button_RemoveADSMalware_19 = new QPushButton(serviceContents);
        button_RemoveADSMalware_19->setObjectName("button_RemoveADSMalware_19");
        button_RemoveADSMalware_19->setMinimumSize(QSize(150, 200));
        button_RemoveADSMalware_19->setMaximumSize(QSize(150, 16777215));
        QFont font4;
        font4.setPointSize(11);
        font4.setBold(true);
        button_RemoveADSMalware_19->setFont(font4);
        button_RemoveADSMalware_19->setCursor(QCursor(Qt::CursorShape::PointingHandCursor));
        button_RemoveADSMalware_19->setStyleSheet(
            QString::fromUtf8(
                "\n"
                "                                          padding: 10px;\n"
                "                                          text-align: bottom center;\n"
                "                                          image: url(:/svg/services/ads-killer);\n"
                "                                          image-position: top center;\n"
                "                                        "));
        button_RemoveADSMalware_19->setFlat(false);

        layoutSpace->addWidget(button_RemoveADSMalware_19, 1, 2, 1, 1);

        button_RemoveADSMalware_18 = new QPushButton(serviceContents);
        button_RemoveADSMalware_18->setObjectName("button_RemoveADSMalware_18");
        button_RemoveADSMalware_18->setMinimumSize(QSize(150, 200));
        button_RemoveADSMalware_18->setMaximumSize(QSize(150, 16777215));
        button_RemoveADSMalware_18->setFont(font4);
        button_RemoveADSMalware_18->setCursor(QCursor(Qt::CursorShape::PointingHandCursor));
        button_RemoveADSMalware_18->setStyleSheet(
            QString::fromUtf8(
                "\n"
                "                                          padding: 10px;\n"
                "                                          text-align: bottom center;\n"
                "                                          image: url(:/svg/services/ads-killer);\n"
                "                                          image-position: top center;\n"
                "                                        "));
        button_RemoveADSMalware_18->setFlat(false);

        layoutSpace->addWidget(button_RemoveADSMalware_18, 1, 1, 1, 1);

        sss->addWidget(serviceContents);

        horizontalSpacer_33 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        sss->addItem(horizontalSpacer_33);

        verticalLayout->addLayout(sss);

        authInfo = new QTableView(scrollAreaWidgetContents_3);
        authInfo->setObjectName("authInfo");
        authInfo->setMinimumSize(QSize(0, 150));
        authInfo->setAutoScroll(true);
        authInfo->setEditTriggers(QAbstractItemView::EditTrigger::NoEditTriggers);
        authInfo->setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);
        authInfo->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);
        authInfo->setShowGrid(true);

        verticalLayout->addWidget(authInfo);

        scrollArea_3->setWidget(scrollAreaWidgetContents_3);

        cabinetMainLayout->addWidget(scrollArea_3);

        tabWidget->addTab(page_cabinet, QString());
        page_devices = new QWidget();
        page_devices->setObjectName("page_devices");
        hboxLayout = new QHBoxLayout(page_devices);
        hboxLayout->setObjectName("hboxLayout");
        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        hboxLayout->addItem(horizontalSpacer_2);

        device_left_group = new QFrame(page_devices);
        device_left_group->setObjectName("device_left_group");
        QSizePolicy sizePolicy3(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Preferred);
        sizePolicy3.setHorizontalStretch(0);
        sizePolicy3.setVerticalStretch(0);
        sizePolicy3.setHeightForWidth(device_left_group->sizePolicy().hasHeightForWidth());
        device_left_group->setSizePolicy(sizePolicy3);
        vboxLayout = new QVBoxLayout(device_left_group);
        vboxLayout->setObjectName("vboxLayout");
        scrollArea_2 = new QScrollArea(device_left_group);
        scrollArea_2->setObjectName("scrollArea_2");
        scrollArea_2->setLineWidth(0);
        scrollArea_2->setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOn);
        scrollArea_2->setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
        scrollArea_2->setWidgetResizable(true);
        scrollAreaWidgetContents_2 = new QWidget();
        scrollAreaWidgetContents_2->setObjectName("scrollAreaWidgetContents_2");
        scrollAreaWidgetContents_2->setGeometry(QRect(0, 0, 566, 437));
        gridLayout_2 = new QGridLayout(scrollAreaWidgetContents_2);
        gridLayout_2->setObjectName("gridLayout_2");
        label = new QLabel(scrollAreaWidgetContents_2);
        label->setObjectName("label");
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextInteractionFlag::LinksAccessibleByMouse | Qt::TextInteractionFlag::TextSelectableByKeyboard | Qt::TextInteractionFlag::TextSelectableByMouse);

        gridLayout_2->addWidget(label, 0, 0, 1, 1);

        label_3 = new QLabel(scrollAreaWidgetContents_2);
        label_3->setObjectName("label_3");
        label_3->setOpenExternalLinks(true);
        label_3->setTextInteractionFlags(Qt::TextInteractionFlag::TextBrowserInteraction);

        gridLayout_2->addWidget(label_3, 1, 0, 1, 1);

        scrollArea_2->setWidget(scrollAreaWidgetContents_2);

        vboxLayout->addWidget(scrollArea_2);

        label_5 = new QLabel(device_left_group);
        label_5->setObjectName("label_5");
        sizePolicy2.setHeightForWidth(label_5->sizePolicy().hasHeightForWidth());
        label_5->setSizePolicy(sizePolicy2);
        label_5->setMinimumSize(QSize(0, 81));
        label_5->setMaximumSize(QSize(16777215, 81));
        QFont font5;
        font5.setPointSize(11);
        label_5->setFont(font5);
        label_5->setAlignment(Qt::AlignmentFlag::AlignCenter);
        label_5->setWordWrap(true);

        vboxLayout->addWidget(label_5);

        hboxLayout->addWidget(device_left_group);

        device_right_group = new QVBoxLayout();
        device_right_group->setSpacing(0);
        device_right_group->setObjectName("device_right_group");

        hboxLayout->addLayout(device_right_group);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        hboxLayout->addItem(horizontalSpacer);

        tabWidget->addTab(page_devices, QString());
        page_adsmalware = new QWidget();
        page_adsmalware->setObjectName("page_adsmalware");
        gridLayout_asd21 = new QGridLayout(page_adsmalware);
        gridLayout_asd21->setSpacing(0);
        gridLayout_asd21->setObjectName("gridLayout_asd21");
        gridLayout_asd21->setContentsMargins(0, 0, 0, 0);
        frame22 = new QFrame(page_adsmalware);
        frame22->setObjectName("frame22");
        frame22->setMinimumSize(QSize(0, 0));
        centralVertical = new QHBoxLayout(frame22);
        centralVertical->setSpacing(0);
        centralVertical->setObjectName("centralVertical");
        centralVertical->setContentsMargins(0, 0, 0, 0);
        gridLayout_11 = new QGridLayout();
        gridLayout_11->setSpacing(0);
        gridLayout_11->setObjectName("gridLayout_11");
        processLogStatus = new QListView(frame22);
        processLogStatus->setObjectName("processLogStatus");
        processLogStatus->setMinimumSize(QSize(100, 0));
        processLogStatus->setTabletTracking(true);
        processLogStatus->setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOn);
        processLogStatus->setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
        processLogStatus->setEditTriggers(QAbstractItemView::EditTrigger::NoEditTriggers);
        processLogStatus->setWordWrap(true);

        gridLayout_11->addWidget(processLogStatus, 1, 1, 1, 1);

        processBarStatus = new QProgressBar(frame22);
        processBarStatus->setObjectName("processBarStatus");
        processBarStatus->setMinimumSize(QSize(0, 20));
        processBarStatus->setMaximumSize(QSize(16777215, 20));
        processBarStatus->setValue(24);

        gridLayout_11->addWidget(processBarStatus, 2, 1, 1, 1);

        deviceLabelName = new QLabel(frame22);
        deviceLabelName->setObjectName("deviceLabelName");
        deviceLabelName->setMinimumSize(QSize(0, 50));
        QFont font6;
        font6.setPointSize(12);
        font6.setBold(true);
        font6.setItalic(true);
        deviceLabelName->setFont(font6);
        deviceLabelName->setAlignment(Qt::AlignmentFlag::AlignCenter);
        deviceLabelName->setWordWrap(true);
        deviceLabelName->setTextInteractionFlags(Qt::TextInteractionFlag::NoTextInteraction);
        gridLayout1 = new QGridLayout(deviceLabelName);
        gridLayout1->setObjectName("gridLayout1");

        gridLayout_11->addWidget(deviceLabelName, 0, 1, 1, 1);

        centralVertical->addLayout(gridLayout_11);

        frame333 = new QFrame(frame22);
        frame333->setObjectName("frame333");
        frame333->setMinimumSize(QSize(0, 0));
        layoutShowCircle = new QGridLayout(frame333);
        layoutShowCircle->setSpacing(0);
        layoutShowCircle->setObjectName("layoutShowCircle");
        frame33 = new QFrame(frame333);
        frame33->setObjectName("frame33");
        frame33->setMinimumSize(QSize(400, 250));
        frame33->setMaximumSize(QSize(500, 350));
        progressCircleLayout = new QVBoxLayout(frame33);
        progressCircleLayout->setSpacing(0);
        progressCircleLayout->setObjectName("progressCircleLayout");
        progressCircleLayout->setContentsMargins(25, 25, 25, 25);

        layoutShowCircle->addWidget(frame33, 1, 0, 1, 1);

        malwareStatusText0 = new QLabel(frame333);
        malwareStatusText0->setObjectName("malwareStatusText0");
        malwareStatusText0->setMaximumSize(QSize(16777215, 100));
        malwareStatusText0->setFont(font3);
        malwareStatusText0->setAlignment(Qt::AlignmentFlag::AlignCenter);
        malwareStatusText0->setWordWrap(true);

        layoutShowCircle->addWidget(malwareStatusText0, 2, 0, 1, 1);

        malwareReRun = new QPushButton(frame333);
        malwareReRun->setObjectName("malwareReRun");
        malwareReRun->setMinimumSize(QSize(0, 35));
        malwareReRun->setMaximumSize(QSize(16777215, 16777215));
        QFont font7;
        font7.setPointSize(12);
        font7.setBold(true);
        malwareReRun->setFont(font7);

        layoutShowCircle->addWidget(malwareReRun, 3, 0, 1, 1);

        centralVertical->addWidget(frame333);

        gridLayout_asd21->addWidget(frame22, 2, 0, 1, 1);

        tabWidget->addTab(page_adsmalware, QString());
        page_loader = new QWidget();
        page_loader->setObjectName("page_loader");
        gridLayout_5 = new QGridLayout(page_loader);
        gridLayout_5->setSpacing(0);
        gridLayout_5->setObjectName("gridLayout_5");
        gridLayout_5->setContentsMargins(0, 0, 0, 0);
        frame_loader = new QFrame(page_loader);
        frame_loader->setObjectName("frame_loader");
        frame_loader->setFrameShape(QFrame::Shape::NoFrame);
        frame_loader->setLineWidth(0);
        gridLayout_4 = new QGridLayout(frame_loader);
        gridLayout_4->setObjectName("gridLayout_4");
        gridLayout_4->setVerticalSpacing(0);
        loaderPageText = new QLabel(frame_loader);
        loaderPageText->setObjectName("loaderPageText");
        loaderPageText->setMaximumSize(QSize(16777215, 80));
        QFont font8;
        font8.setPointSize(13);
        font8.setBold(true);
        font8.setUnderline(false);
        loaderPageText->setFont(font8);
        loaderPageText->setAlignment(Qt::AlignmentFlag::AlignCenter);
        loaderPageText->setWordWrap(true);

        gridLayout_4->addWidget(loaderPageText, 3, 0, 1, 1);

        loaderLayout = new QVBoxLayout();
        loaderLayout->setSpacing(0);
        loaderLayout->setObjectName("loaderLayout");
        loaderLayout->setContentsMargins(0, 0, 0, 0);

        gridLayout_4->addLayout(loaderLayout, 2, 0, 1, 1);

        gridLayout_5->addWidget(frame_loader, 0, 0, 1, 1);

        tabWidget->addTab(page_loader, QString());
        tab = new QWidget();
        tab->setObjectName("tab");
        toplevel_backpage = new QFrame(tab);
        toplevel_backpage->setObjectName("toplevel_backpage");
        toplevel_backpage->setGeometry(QRect(110, 40, 741, 40));
        toplevel_backpage->setMinimumSize(QSize(0, 40));
        toplevel_backpage->setMaximumSize(QSize(16777215, 32));
        toplevel_backpage->setStyleSheet(QString::fromUtf8("background-color: #0B0F19; border-bottom: 1px solid #1E293B;"));
        toplevel_backpage->setFrameShape(QFrame::Shape::NoFrame);
        toplevel_backpage->setFrameShadow(QFrame::Shadow::Raised);
        toplevel_backpage->setLineWidth(0);
        toplevel_layout_auth_3 = new QHBoxLayout(toplevel_backpage);
        toplevel_layout_auth_3->setSpacing(10);
        toplevel_layout_auth_3->setObjectName("toplevel_layout_auth_3");
        toplevel_layout_auth_3->setContentsMargins(4, 0, 0, 5);
        buttonBackTo = new QPushButton(toplevel_backpage);
        buttonBackTo->setObjectName("buttonBackTo");
        buttonBackTo->setMaximumSize(QSize(90, 24));
        QIcon icon5(QIcon::fromTheme(QIcon::ThemeIcon::EditUndo));
        buttonBackTo->setIcon(icon5);
        buttonBackTo->setCheckable(false);
        buttonBackTo->setFlat(false);

        toplevel_layout_auth_3->addWidget(buttonBackTo);

        label_8 = new QLabel(toplevel_backpage);
        label_8->setObjectName("label_8");
        label_8->setFont(font1);
        label_8->setLineWidth(0);
        label_8->setOpenExternalLinks(true);

        toplevel_layout_auth_3->addWidget(label_8);

        tabWidget->addTab(tab, QString());
        page_mydevices = new QWidget();
        page_mydevices->setObjectName("page_mydevices");
        vboxLayout1 = new QVBoxLayout(page_mydevices);
        vboxLayout1->setObjectName("vboxLayout1");
        verticalLayout_2 = new QHBoxLayout();
        verticalLayout_2->setObjectName("verticalLayout_2");
        label_9 = new QLabel(page_mydevices);
        label_9->setObjectName("label_9");

        verticalLayout_2->addWidget(label_9);

        myDeviceFilterDateStart = new QDateEdit(page_mydevices);
        myDeviceFilterDateStart->setObjectName("myDeviceFilterDateStart");
        myDeviceFilterDateStart->setCalendarPopup(true);

        verticalLayout_2->addWidget(myDeviceFilterDateStart);

        label_10 = new QLabel(page_mydevices);
        label_10->setObjectName("label_10");

        verticalLayout_2->addWidget(label_10);

        myDeviceFilterDateEnd = new QDateEdit(page_mydevices);
        myDeviceFilterDateEnd->setObjectName("myDeviceFilterDateEnd");
        myDeviceFilterDateEnd->setCalendarPopup(true);

        verticalLayout_2->addWidget(myDeviceFilterDateEnd);

        myDeviceQuaranteeFilter = new QCheckBox(page_mydevices);
        myDeviceQuaranteeFilter->setObjectName("myDeviceQuaranteeFilter");

        verticalLayout_2->addWidget(myDeviceQuaranteeFilter);

        horizontalSpacer_4 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        verticalLayout_2->addItem(horizontalSpacer_4);

        myDeviceSend = new QPushButton(page_mydevices);
        myDeviceSend->setObjectName("myDeviceSend");

        verticalLayout_2->addWidget(myDeviceSend);

        vboxLayout1->addLayout(verticalLayout_2);

        myDeviceActual = new QTableView(page_mydevices);
        myDeviceActual->setObjectName("myDeviceActual");
        myDeviceActual->setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOn);
        myDeviceActual->setEditTriggers(QAbstractItemView::EditTrigger::NoEditTriggers);
        myDeviceActual->setDragDropOverwriteMode(false);
        myDeviceActual->setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);
        myDeviceActual->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);
        myDeviceActual->setSortingEnabled(true);

        vboxLayout1->addWidget(myDeviceActual);

        myDevicePageLabel = new QLabel(page_mydevices);
        myDevicePageLabel->setObjectName("myDevicePageLabel");

        vboxLayout1->addWidget(myDevicePageLabel);

        tabWidget->addTab(page_mydevices, QString());
        page_buyvip = new QWidget();
        page_buyvip->setObjectName("page_buyvip");
        gridLayout_3 = new QGridLayout(page_buyvip);
        gridLayout_3->setObjectName("gridLayout_3");
        horizontalSpacer_5 = new QSpacerItem(158, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        gridLayout_3->addItem(horizontalSpacer_5, 1, 2, 1, 1);

        groupBox = new QGroupBox(page_buyvip);
        groupBox->setObjectName("groupBox");
        groupBox->setAlignment(Qt::AlignmentFlag::AlignLeading | Qt::AlignmentFlag::AlignLeft | Qt::AlignmentFlag::AlignVCenter);
        verticalLayout_4 = new QVBoxLayout(groupBox);
        verticalLayout_4->setObjectName("verticalLayout_4");
        label_13 = new QLabel(groupBox);
        label_13->setObjectName("label_13");

        verticalLayout_4->addWidget(label_13);

        labelVipBalance = new QLabel(groupBox);
        labelVipBalance->setObjectName("labelVipBalance");

        verticalLayout_4->addWidget(labelVipBalance);

        comboBoxSelectVIPDays = new QComboBox(groupBox);
        comboBoxSelectVIPDays->setObjectName("comboBoxSelectVIPDays");

        verticalLayout_4->addWidget(comboBoxSelectVIPDays);

        frame = new QFrame(groupBox);
        frame->setObjectName("frame");
        frame->setFrameShape(QFrame::Shape::StyledPanel);
        frame->setFrameShadow(QFrame::Shadow::Raised);
        horizontalLayout_4 = new QHBoxLayout(frame);
        horizontalLayout_4->setObjectName("horizontalLayout_4");
        labelInfoVip = new QLabel(frame);
        labelInfoVip->setObjectName("labelInfoVip");

        horizontalLayout_4->addWidget(labelInfoVip);

        horizontalSpacer_7 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_4->addItem(horizontalSpacer_7);

        buttonBuyVip = new QPushButton(frame);
        buttonBuyVip->setObjectName("buttonBuyVip");
        buttonBuyVip->setMinimumSize(QSize(120, 0));
        buttonBuyVip->setMaximumSize(QSize(120, 16777215));

        horizontalLayout_4->addWidget(buttonBuyVip);

        verticalLayout_4->addWidget(frame);

        gridLayout_3->addWidget(groupBox, 1, 1, 1, 1);

        verticalSpacer_4 = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        gridLayout_3->addItem(verticalSpacer_4, 2, 1, 1, 1);

        horizontalSpacer_6 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        gridLayout_3->addItem(horizontalSpacer_6, 1, 0, 1, 1);

        verticalSpacer_5 = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        gridLayout_3->addItem(verticalSpacer_5, 0, 1, 1, 1);

        tabWidget->addTab(page_buyvip, QString());

        gridLayout_10->addWidget(tabWidget, 2, 0, 1, 1);

        centralwidget_Layout->addWidget(contentLayout, 1, 0, 1, 1);

        topcontent = new QFrame(centralwidget);
        topcontent->setObjectName("topcontent");
        sizePolicy1.setHeightForWidth(topcontent->sizePolicy().hasHeightForWidth());
        topcontent->setSizePolicy(sizePolicy1);
        topcontent->setBaseSize(QSize(0, 0));
        vboxLayout2 = new QVBoxLayout(topcontent);
        vboxLayout2->setSpacing(0);
        vboxLayout2->setObjectName("vboxLayout2");

        centralwidget_Layout->addWidget(topcontent, 0, 0, 1, 1);

        MainWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName("menubar");
        menubar->setGeometry(QRect(0, 0, 1180, 23));
        menu = new QMenu(menubar);
        menu->setObjectName("menu");
        menu_2 = new QMenu(menubar);
        menu_2->setObjectName("menu_2");
        menu_3 = new QMenu(menubar);
        menu_3->setObjectName("menu_3");
        menu_4 = new QMenu(menubar);
        menu_4->setObjectName("menu_4");
        MainWindow->setMenuBar(menubar);
#if QT_CONFIG(shortcut)
        label_2->setBuddy(authButton);
        label_6->setBuddy(authpageUpdate);
        label_8->setBuddy(authpageUpdate);
#endif // QT_CONFIG(shortcut)
        QWidget::setTabOrder(linePassEdit, processLogStatus);
        QWidget::setTabOrder(processLogStatus, scrollArea_3);
        QWidget::setTabOrder(scrollArea_3, authInfo);
        QWidget::setTabOrder(authInfo, scrollArea_2);
        QWidget::setTabOrder(scrollArea_2, authButton);
        QWidget::setTabOrder(authButton, checkAutoLogin);
        QWidget::setTabOrder(checkAutoLogin, authpageUpdate);

        menubar->addAction(menu->menuAction());
        menubar->addAction(menu_3->menuAction());
        menubar->addAction(menu_2->menuAction());
        menubar->addAction(menu_4->menuAction());
        menu->addAction(actionClose);
        menu_2->addSeparator();
        menu_2->addAction(actionAboutUs);
        menu_2->addSeparator();
        menu_2->addAction(actionUsLic);
        menu_2->addAction(action_Qt);
        menu_3->addAction(action_WhatsApp);
        menu_4->addAction(mThemeLight);
        menu_4->addAction(mThemeDark);
        menu_4->addAction(mThemeSystem);

        retranslateUi(MainWindow);
        QObject::connect(actionClose, &QAction::triggered, MainWindow, qOverload<>(&QMainWindow::close));
        QObject::connect(label_2, &QLabel::linkActivated, action_WhatsApp, qOverload<>(&QAction::trigger));

        tabWidget->setCurrentIndex(5);
        button_RemoveADSMalware_19->setDefault(true);
        button_RemoveADSMalware_18->setDefault(true);
        buttonBackTo->setDefault(false);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "Ads Mobile Killer", nullptr));
        actionClose->setText(QCoreApplication::translate("MainWindow", "\320\222\321\213\321\205\320\276\320\264", nullptr));
#if QT_CONFIG(shortcut)
        actionClose->setShortcut(QCoreApplication::translate("MainWindow", "Ctrl+Q", nullptr));
#endif // QT_CONFIG(shortcut)
        actionAboutUs->setText(QCoreApplication::translate("MainWindow", "\320\236 \320\237\321\200\320\276\320\263\321\200\320\260\320\274\320\274\320\265", nullptr));
        actionUsLic->setText(QCoreApplication::translate("MainWindow", "\320\233\320\270\321\206\320\265\320\275\320\267\320\270\321\217 \320\237\320\236", nullptr));
        action_WhatsApp->setText(QCoreApplication::translate("MainWindow", "\320\241\320\262\321\217\320\267\320\260\321\202\321\214\321\201\321\217 \321\207\320\265\321\200\320\265\320\267 &WhatsApp", nullptr));
        action_WhatsApp->setIconText(QCoreApplication::translate("MainWindow", "\320\241\320\262\321\217\320\267\320\260\321\202\321\214\321\201\321\217 \321\207\320\265\321\200\320\265\320\267 WhatsApp", nullptr));
        action_Qt->setText(QCoreApplication::translate("MainWindow", "\320\236 &Qt", nullptr));
        mThemeLight->setText(QCoreApplication::translate("MainWindow", "\320\241\320\262\320\265\321\202\320\273\320\260\321\217", nullptr));
        mThemeDark->setText(QCoreApplication::translate("MainWindow", "\320\242\320\265\320\274\320\275\320\260\321\217", nullptr));
        mThemeSystem->setText(QCoreApplication::translate("MainWindow", "\320\241\320\270\321\201\321\202\320\265\320\274\320\275\321\213\320\271", nullptr));
        aiChatSend->setText(QString());
        aiChatMessages->setPlaceholderText(QCoreApplication::translate("MainWindow", "\320\230\320\230 \320\262 \320\276\320\266\320\270\320\264\320\260\320\275\320\270\320\270...", nullptr));
        aiChatEdit->setPlaceholderText(QCoreApplication::translate("MainWindow", "\320\235\320\260\320\277\320\270\321\210\320\270\321\202\320\265 \320\274\320\275\320\265...", nullptr));
        aiToolBox->setItemText(aiToolBox->indexOf(aiToolBoxPage1), QCoreApplication::translate("MainWindow", "\320\230\320\230 \320\220\321\201\320\270\321\201\321\202\320\265\320\275\321\202 - AdsKiller", nullptr));
        aboutAi_edit->setHtml(
            QCoreApplication::translate(
                "MainWindow",
                "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">\n"
                "<html><head><meta name=\"qrichtext\" content=\"1\" /><meta charset=\"utf-8\" /><style type=\"text/css\">\n"
                "p, li { white-space: pre-wrap; }\n"
                "hr { height: 1px; border-width: 0; }\n"
                "li.unchecked::marker { content: \"\\2610\"; }\n"
                "li.checked::marker { content: \"\\2612\"; }\n"
                "</style></head><body style=\" font-family:'Noto Sans'; font-size:10pt; font-weight:400; font-style:normal;\">\n"
                "<p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">\320\220\320\262\321\202\320\276\321\200 \320\274\320\276\320\264\321\203\320\273\321\217 \320\230\320\230<br /><br "
                "/>\320\272\320\276\320\274\320\260\320\275\320\264\320\260 imister.tech <br /><br />\320\240\320\260\320\267\321\200\320\260\320\261\320\276\321\202\321\207\320\270\320\272 \320\235\321\203\321\200\321\201\320\265\320\270\321\202 \320\232. (badcast) <br "
                "/>\320\224\320\270\320\267\320\260"
                "\320\271\320\275 \320\270\320\272\320\276\320\275\320\276\320\272 \320\222\320\273\320\260\320\264\320\270\320\274\320\270\321\200 (LeoJames)</p></body></html>",
                nullptr));
        aiToolBox->setItemText(aiToolBox->indexOf(aiToolBoxPage2), QCoreApplication::translate("MainWindow", "\320\236 \320\230\320\230", nullptr));
        statusAuthText->setText(QString());
        label_4->setText(
            QCoreApplication::translate(
                "MainWindow",
                "\320\222\320\262\320\265\320\264\320\270\321\202\320\265 \320\273\320\276\320\263\320\270\320\275 \320\270 \320\277\320\260\321\200\320\276\320\273\321\214 \320\264\320\273\321\217 \320\260\320\262\321\202\320\276\321\200\320\270\320\267\320\260\321\206\320\270\320\270",
                nullptr));
        checkAutoLogin->setText(QCoreApplication::translate("MainWindow", "\320\222\320\276\320\271\321\202\320\270 \320\277\321\200\320\270 \320\267\320\260\320\277\321\203\321\201\320\272\320\265\321\213", nullptr));
        authButton->setText(QCoreApplication::translate("MainWindow", "\320\222\320\276\320\271\321\202\320\270", nullptr));
        label_2->setText(QCoreApplication::translate("MainWindow", "<a href=\"index\">\320\227\320\260\320\261\321\213\320\273\320\270 \321\201\320\262\320\276\320\271 \321\202\320\276\320\272\320\265\320\275?</a>", nullptr));
        label_12->setText(QCoreApplication::translate("MainWindow", "\320\233\320\236\320\223\320\230\320\235", nullptr));
        label_14->setText(QCoreApplication::translate("MainWindow", "\320\237\320\220\320\240\320\236\320\233\320\254", nullptr));
        butShowPass->setText(QCoreApplication::translate("MainWindow", "+", nullptr));
        label_auth_ver->setText(QCoreApplication::translate("MainWindow", "\320\222\320\225\320\240\320\241\320\230\320\257...", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(page_auth), QCoreApplication::translate("MainWindow", "Auth Page", nullptr));
        logoutButton->setText(QCoreApplication::translate("MainWindow", "\320\222\321\213\320\271\321\202\320\270", nullptr));
        label_6->setText(QCoreApplication::translate("MainWindow", "\320\224\320\236\320\221\320\240\320\236 \320\237\320\236\320\226\320\220\320\233\320\236\320\222\320\220\320\242\320\254 \320\222 ADSKILLER! - <a color=white href=\"https://imister.tech\">imister.tech</a>", nullptr));
        authpageUpdate->setText(QString());
        labelLoginAuthed->setText(QCoreApplication::translate("MainWindow", "LOGIN NAME", nullptr));
        labelCredits->setText(QCoreApplication::translate("MainWindow", "50 CREDITS", nullptr));
        labelVipDays->setText(QCoreApplication::translate("MainWindow", "Vip Days", nullptr));
        label_7->setText(QCoreApplication::translate("MainWindow", "\320\241\320\225\320\240\320\222\320\230\320\241 \320\243\320\241\320\233\320\243\320\223\320\230", nullptr));
        button_RemoveADSMalware_19->setText(
            QCoreApplication::translate(
                "MainWindow",
                "\320\236\320\261\320\265\320\267\320\262\321\200\320\265\320\264\320\270\321\202\321\214\n"
                "                                          \320\240\320\265\320\272\320\273\320\260\320\274\321\203",
                nullptr));
        button_RemoveADSMalware_18->setText(
            QCoreApplication::translate(
                "MainWindow",
                "\320\236\320\261\320\265\320\267\320\262\321\200\320\265\320\264\320\270\321\202\321\214\n"
                "                                          \320\240\320\265\320\272\320\273\320\260\320\274\321\203",
                nullptr));
        tabWidget->setTabText(tabWidget->indexOf(page_cabinet), QCoreApplication::translate("MainWindow", "Cabinet", nullptr));
        label->setText(
            QCoreApplication::translate(
                "MainWindow",
                "<html><head/><body><h2 style=\" margin-top:16px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:x-large; font-weight:700;\">\320\232\320\260\320\272 "
                "\320\262\320\272\320\273\321\216\321\207\320\270\321\202\321\214 \320\276\321\202\320\273\320\260\320\264\320\272\321\203 USB \320\275\320\260 Android \320\267\320\260 3 \321\210\320\260\320\263\320\260</span></h2><h3 style=\" margin-top:14px; margin-bottom:12px; margin-left:0px; "
                "margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:large; font-weight:700;\">\320\250\320\260\320\263 1: \320\222\320\272\320\273\321\216\321\207\320\270\321\202\320\265 \321\200\320\265\320\266\320\270\320\274 "
                "\321\200\320\260\320\267\321\200\320\260\320\261\320\276\321\202\321\207\320\270\320\272\320\260</span></h3><ol style=\"margin-top: 0px; margin-bottom: 0px; margin-left: 0px; margin-right: 0px; -qt-list-indent: 1;\"><li style=\" margin-top:12px; margin-bottom:0px; margin-left:0px;"
                " margin-right:0px; -qt-block-indent:0; text-indent:0px;\">\320\237\320\265\321\200\320\265\320\271\320\264\320\270\321\202\320\265 \320\262 <span style=\" font-weight:700;\">\320\235\320\260\321\201\321\202\321\200\320\276\320\271\320\272\320\270</span>.</li><li style=\" "
                "margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">\320\235\320\260\320\271\320\264\320\270\321\202\320\265 \321\200\320\260\320\267\320\264\320\265\320\273 <span style=\" font-weight:700;\">\320\236 "
                "\321\202\320\265\320\273\320\265\321\204\320\276\320\275\320\265</span>.</li><li style=\" margin-top:0px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">\320\235\320\260\320\271\320\264\320\270\321\202\320\265 <span style=\" "
                "font-weight:700;\">\320\235\320\276\320\274\320\265\321\200 \321\201\320\261\320\276\321\200\320\272\320\270</span> \320\270 \320\275\320\260\320\266\320\274\320\270\321\202\320\265 \320\275\320\260 \320\275\320\265\320\263"
                "\320\276 <span style=\" font-weight:700;\">7 \321\200\320\260\320\267</span>. \320\222\321\213 \321\203\320\262\320\270\320\264\320\270\321\202\320\265 \321\201\320\276\320\276\320\261\321\211\320\265\320\275\320\270\320\265 \320\276 \321\202\320\276\320\274, "
                "\321\207\321\202\320\276 \320\262\321\213 \321\201\321\202\320\260\320\273\320\270 \321\200\320\260\320\267\321\200\320\260\320\261\320\276\321\202\321\207\320\270\320\272\320\276\320\274.</li></ol><h3 style=\" margin-top:14px; margin-bottom:12px; margin-left:0px; "
                "margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:large; font-weight:700;\">\320\250\320\260\320\263 2: \320\222\320\272\320\273\321\216\321\207\320\270\321\202\320\265 \320\276\321\202\320\273\320\260\320\264\320\272\321\203 \320\277\320\276 "
                "USB</span></h3><ol style=\"margin-top: 0px; margin-bottom: 0px; margin-left: 0px; margin-right: 0px; -qt-list-indent: 1;\"><li style=\" margin-top:12px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0"
                "; text-indent:0px;\">\320\222\320\265\321\200\320\275\320\270\321\202\320\265\321\201\321\214 \320\262 <span style=\" font-weight:700;\">\320\235\320\260\321\201\321\202\321\200\320\276\320\271\320\272\320\270</span>.</li><li style=\" margin-top:0px; margin-bottom:0px; "
                "margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">\320\235\320\260\320\271\320\264\320\270\321\202\320\265 \321\200\320\260\320\267\320\264\320\265\320\273 <span style=\" font-weight:700;\">\320\224\320\273\321\217 "
                "\321\200\320\260\320\267\321\200\320\260\320\261\320\276\321\202\321\207\320\270\320\272\320\276\320\262</span>.</li><li style=\" margin-top:0px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; "
                "text-indent:0px;\">\320\222\320\272\320\273\321\216\321\207\320\270\321\202\320\265 <span style=\" font-weight:700;\">\320\236\321\202\320\273\320\260\320\264\320\272\320\260 \320\277\320\276 USB</span>.</li></ol><h3 style=\" margin-top:14px; margin-bottom:12px; margin-left:0px; "
                "margin-right:0p"
                "x; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:large; font-weight:700;\">\320\250\320\260\320\263 3: \320\237\320\276\320\264\320\272\320\273\321\216\321\207\320\270\321\202\320\265 "
                "\321\203\321\201\321\202\321\200\320\276\320\271\321\201\321\202\320\262\320\276 \320\272 \320\272\320\276\320\274\320\277\321\214\321\216\321\202\320\265\321\200\321\203</span></h3><ol style=\"margin-top: 0px; margin-bottom: 0px; margin-left: 0px; margin-right: 0px; "
                "-qt-list-indent: 1;\"><li style=\" margin-top:12px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">\320\237\320\276\320\264\320\272\320\273\321\216\321\207\320\270\321\202\320\265 \320\262\320\260\321\210\320\265 "
                "Android-\321\203\321\201\321\202\321\200\320\276\320\271\321\201\321\202\320\262\320\276 \320\272 \320\272\320\276\320\274\320\277\321\214\321\216\321\202\320\265\321\200\321\203 \321\201 \320\277\320\276\320\274\320\276\321\211\321\214\321\216 <span style=\" "
                "font-weight:700;\">USB-\320\272\320\260"
                "\320\261\320\265\320\273\321\217</span>.</li><li style=\" margin-top:0px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">\320\237\320\276\320\264\321\202\320\262\320\265\321\200\320\264\320\270\321\202\320\265 "
                "\320\267\320\260\320\277\321\200\320\276\321\201 \320\275\320\260 \321\200\320\260\320\267\321\200\320\265\321\210\320\265\320\275\320\270\320\265 \320\276\321\202\320\273\320\260\320\264\320\272\320\270 \320\277\320\276 USB \320\275\320\260 "
                "\321\215\320\272\321\200\320\260\320\275\320\265 \321\203\321\201\321\202\321\200\320\276\320\271\321\201\321\202\320\262\320\260.</li></ol></body></html>",
                nullptr));
        label_3->setText(
            QCoreApplication::translate(
                "MainWindow",
                "<a href=\"https://www.anymp4.com/ru/faq/enable-usb-debugging-for-android.html\">\320\237\320\276\320\273\320\275\320\260\321\217 \320\270\320\275\321\201\321\202\321\200\321\203\320\272\321\206\320\270\321\217</a>\n"
                "                                      ",
                nullptr));
        label_5->setText(
            QCoreApplication::translate(
                "MainWindow",
                "\320\236\320\266\320\270\320\264\320\260\320\275\320\270\320\265 \320\277\320\276\320\264\320\272\320\273\321\216\321\207\320\265\320\275\320\270\321\217 \321\203\321\201\321\202\321\200\320\276\320\271\321\201\321\202\320\262\320\260 \320\277\320\276 USB...",
                nullptr));
        tabWidget->setTabText(tabWidget->indexOf(page_devices), QCoreApplication::translate("MainWindow", "adbdevice-page", nullptr));
        deviceLabelName->setText(QCoreApplication::translate("MainWindow", "DEVICE NAME", nullptr));
        malwareStatusText0->setText(QCoreApplication::translate("MainWindow", "STATUS MALWARE", nullptr));
        malwareReRun->setText(QCoreApplication::translate("MainWindow", "\320\227\320\220\320\237\320\243\320\241\320\242\320\230\320\242\320\254/\320\237\320\225\320\240\320\225\320\227\320\220\320\237\320\243\320\241\320\242\320\230\320\242\320\254", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(page_adsmalware), QCoreApplication::translate("MainWindow", "page-adskiller", nullptr));
        loaderPageText->setText(QCoreApplication::translate("MainWindow", "LOADING PAGE", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(page_loader), QCoreApplication::translate("MainWindow", "loader", nullptr));
        buttonBackTo->setText(QCoreApplication::translate("MainWindow", "\320\235\320\260\320\267\320\260\320\264", nullptr));
        label_8->setText(QCoreApplication::translate("MainWindow", "\320\241\320\233\320\243\320\226\320\221\320\220 \320\222\320\253\320\237\320\236\320\233\320\235\320\257\320\225\320\242\320\241\320\257", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tab), QCoreApplication::translate("MainWindow", "back-button-page", nullptr));
        label_9->setText(QCoreApplication::translate("MainWindow", "\320\237\320\276\320\272\320\260\320\267\320\260\321\202\321\214 \321\201", nullptr));
        label_10->setText(QCoreApplication::translate("MainWindow", "\320\237\320\276", nullptr));
        myDeviceQuaranteeFilter->setText(QCoreApplication::translate("MainWindow", "\320\242\320\276\320\273\321\214\320\272\320\276 \321\201 \320\263\320\260\321\200\320\260\320\275\321\202\320\270\320\265\320\271", nullptr));
        myDeviceSend->setText(QCoreApplication::translate("MainWindow", "\320\236\320\261\320\275\320\276\320\262\320\270\321\202\321\214", nullptr));
        myDevicePageLabel->setText(QCoreApplication::translate("MainWindow", "\320\234\320\276\320\270 \321\203\321\201\321\202\321\200\320\276\320\271\321\201\321\202\320\262\320\260 \320\270 \320\263\320\260\321\200\320\260\320\275\321\202\320\270\321\217.", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(page_mydevices), QCoreApplication::translate("MainWindow", "mydevices-page", nullptr));
        groupBox->setTitle(QCoreApplication::translate("MainWindow", "\320\236\321\204\320\276\321\200\320\274\320\273\320\265\320\275\320\270\320\265 VIP-\320\277\320\276\320\264\320\277\320\270\321\201\320\272\320\270 AdsKiller", nullptr));
        label_13->setText(
            QCoreApplication::translate(
                "MainWindow", "\320\222\321\213\320\261\320\265\321\200\320\270\321\202\320\265 \320\262\321\200\320\265\320\274\321\217 \320\264\320\265\320\271\321\201\321\202\320\262\320\270\321\217 VIP \320\260\320\272\320\272\320\260\321\203\320\275\321\202\320\260", nullptr));
        labelVipBalance->setText(QCoreApplication::translate("MainWindow", "BalanceCredits", nullptr));
        labelInfoVip->setText(QCoreApplication::translate("MainWindow", "InfoPeriods", nullptr));
        buttonBuyVip->setText(QCoreApplication::translate("MainWindow", "\320\232\320\243\320\237\320\230\320\242\320\254", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(page_buyvip), QCoreApplication::translate("MainWindow", "buyvip-page", nullptr));
        menu->setTitle(QCoreApplication::translate("MainWindow", "\320\244\320\260\320\271\320\273", nullptr));
        menu_2->setTitle(QCoreApplication::translate("MainWindow", "\320\241\320\262\320\265\320\264\320\265\320\275\320\270\321\217", nullptr));
        menu_3->setTitle(QCoreApplication::translate("MainWindow", "\320\237\320\276\320\264\320\264\320\265\321\200\320\266\320\272\320\260", nullptr));
        menu_4->setTitle(QCoreApplication::translate("MainWindow", "\320\242\320\265\320\274\320\260", nullptr));
    } // retranslateUi
};

namespace Ui
{
    class MainWindow : public Ui_MainWindow
    {
    };
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
