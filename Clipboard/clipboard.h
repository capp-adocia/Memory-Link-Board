#pragma once

#include <qglobal.h>
#ifdef Q_OS_LINUX
	
#elif defined(Q_OS_WIN32)
	#include <windows.h>
#else
	#error "Unsupported platform"
#endif

#include <QtWidgets/QMainWindow>
#include "ui_clipboard.h"
#include <QClipboard>
#include <QDebug>
#include <QTextBrowser>
#include <QMouseEvent>
#include <QTimer>
#include <QMimeData>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QScreen>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QLabel>
#include <QListWidgetItem>
#include <QDir>
#include <QFileInfo>
#include <QStringListModel>
#include <QDesktopServices>
#include <QLineEdit>
#include <QMessageBox>
#include <QColorDialog>
#include <QFileDialog>
#include <QSettings>
#include <QPushButton>
#include <QTextStream>
#include <QDateTime>

#define LEFT 1
#define RIGHT -1
#define BUTTONSIZE QSize(24,24)

QT_BEGIN_NAMESPACE
namespace Ui { class ClipboardClass; };
QT_END_NAMESPACE

class Clipboard : public QMainWindow
{
    Q_OBJECT
public:
    Clipboard(QWidget *parent = nullptr);
    ~Clipboard();
	void LoadSettings();
	void LoadFontColor();
	void startMouseHook();
	void stopMouseHook();
    static int mouseX; // 鼠标X坐标
    static int mouseY; // 鼠标Y坐标
	enum ContentType
	{
		TXT, // 文本
		IMG, // 图片
		DOC, // 文件及文件夹
	};
	void SaveHistory(ContentType contentType, const QStringList& contents);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent* event) override;
	void keyPressEvent(QKeyEvent *event) override;
signals:

public slots:
    void onClipboardDataChanged();
	void clickMoreButton(); // 显示菜单栏
	void Turn2History(qint8 direction); // 跳转到历史记录

private:
    Ui::ClipboardClass *ui;
	QSettings settings;
    QClipboard *clipboard;
	QPoint lastPos;
	int screenWidth;
	int screenHeight;
    QStringList ImgPaths;
    QStringList DocPaths;
	QString handledText; // 处理好的文本
	QPushButton* ESCButton;
	QPushButton* HideButton;
	QPushButton* MoreButton;
	QPushButton* PreButton;
	QPushButton* NextButton;
	QPushButton* FixButton;
	bool IsFixed; // 是否固定窗口位置
	quint64 HisConCount; // 每记录一条数据就自增一次
	int HisConOffset; // 记录离当前记录的偏移量
	bool LeftHistoryMSGboxShown; // 防止多次触发
	bool RightHistoryMSGboxShown; // 防止多次触发
};

/* 中文转Unicode */
QString operator""_toUnicode(const char* CN_Str, size_t len);