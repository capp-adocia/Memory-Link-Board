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
#include <QStandardItem>
#include <QListWidgetItem>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QSortFilterProxyModel>

QT_BEGIN_NAMESPACE
namespace Ui { class ClipboardClass; };
QT_END_NAMESPACE

class Clipboard : public QMainWindow
{
    Q_OBJECT
public:
    Clipboard(QWidget *parent = nullptr);
    ~Clipboard();
	void startMouseHook();
	void stopMouseHook();
	static int mouseX; // 鼠标位置x
	static int mouseY; // 鼠标位置y
	static Clipboard *wid; //定义一个静态的Clipboard类对象

protected:
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void wheelEvent(QWheelEvent* event) override;

signals:

public slots:
	void onClipboardDataChanged();

private:   
	Ui::ClipboardClass *ui;
	QClipboard *clipboard;
	QPoint lastPos;
	int screenWidth;
	QStringList imgPaths;
	QStringList docNames;
	QFileSystemModel *fileModel;
	QSortFilterProxyModel *proxyModel;
};

LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam);