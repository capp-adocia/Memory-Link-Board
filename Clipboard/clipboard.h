#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_clipboard.h"
#include <QClipboard>
#include <QDebug>
#include <QTextBrowser>
#include <QMouseEvent>
#include <QTimer>
#include <QMimeData>
#include <windows.h>
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
    void startMouseHook();
    static int mouseX; // 鼠标X坐标
    static int mouseY; // 鼠标Y坐标

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent* event) override;
	void keyPressEvent(QKeyEvent *event) override;

signals:

public slots:
    void onClipboardDataChanged();
	void clickMoreButton(); // 显示菜单栏

private:
    Ui::ClipboardClass *ui;
	QSettings settings;
    QClipboard *clipboard;
    QPoint lastPos;
	int screenWidth;
    QStringList imgPaths;
    QStringList docNames;
	QString handledText; // 处理好的文本
};

LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam);
