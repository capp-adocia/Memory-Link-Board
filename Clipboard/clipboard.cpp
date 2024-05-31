#include "clipboard.h"

HHOOK mouseHook;
int Clipboard::mouseX = 0;
int Clipboard::mouseY = 0;
Clipboard *Clipboard::wid = nullptr;

LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode >= 0)
	{
		MSLLHOOKSTRUCT* mouseInfo = (MSLLHOOKSTRUCT*)lParam;
		int mouseX = mouseInfo->pt.x;
		int mouseY = mouseInfo->pt.y;

		
		Clipboard::mouseX = mouseX;
		Clipboard::mouseY = mouseY;
	}
	return CallNextHookEx(mouseHook, nCode, wParam, lParam);
}

Clipboard::Clipboard(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::ClipboardClass())
{
    ui->setupUi(this);
	wid = this;

	setWindowFlags(Qt::Window | Qt::CustomizeWindowHint | Qt::WindowStaysOnTopHint);
	/* 初始化样式 */
	QString buttonStyleSheet = "QPushButton{"
		"color:white;"
		"background-color: rgb(109, 109, 109);"
		"border-left:4px solid rgb(184, 184, 185);"
		"border-top:4px solid rgb(184, 184, 185);"
		"border-right:4px solid rgb(88, 88, 88);"
		"border-bottom:4px solid rgb(88, 88, 88);"
		"}"
		"QPushButton:hover{"
		"color:white;"
		"background-color: rgb(109, 109, 109);"
		"border:5px solid rgb(224, 226, 224);"
		"}";
	ui->centralWidget->setStyleSheet("QWidget#centralWidget{border-image:url(:/Img/BillboardUp.png);}");
	ui->textBrowser->setStyleSheet("QWidget#textBrowser{border-image:url(:/Img/TextBillboardUp.png);}");
	ui->ImgListWidget->setStyleSheet("QWidget#ImgListWidget{border-image:url(:/Img/TextBillboardUp.png);}");
	ui->DoclistView->setStyleSheet("QWidget#DoclistView{border-image:url(:/Img/TextBillboardUp.png);}");
	
	ui->HideButton->setStyleSheet(buttonStyleSheet);
	ui->ESCButton->setStyleSheet(buttonStyleSheet);
	ui->textBrowser->setOpenExternalLinks(true);

	/* 创建系统托盘菜单 */
	QSystemTrayIcon *trayIcon = new QSystemTrayIcon(QIcon("C:\\Users\\26364\\Desktop\\MC_res\\health.png"),this);
	QMenu *trayMenu = new QMenu(this);
	QAction *exitAction = new QAction("Exit", this);
	trayMenu->addSeparator();
	trayMenu->addAction(exitAction);

	trayIcon->setContextMenu(trayMenu);
	trayIcon->show();

	connect(trayIcon, &QSystemTrayIcon::activated, this, [=](QSystemTrayIcon::ActivationReason reason){
		//单击托盘图标
		if (reason == QSystemTrayIcon::Trigger) isVisible() ? hide() : show();
	});
	connect(exitAction, &QAction::triggered, [&]() {
		QApplication::exit();
	});

	/* 获取设备屏幕的宽度 */
	screenWidth = QGuiApplication::primaryScreen()->geometry().width();

	/* 启动钩子 */
	startMouseHook();
	
	/* 剪切板 */
	clipboard = QApplication::clipboard();

	ui->textBrowser->setText(clipboard->text());

	QTimer::singleShot(0, this, [&] {
		hide();
	});

	connect(clipboard, &QClipboard::dataChanged, this, &Clipboard::onClipboardDataChanged);
	connect(ui->HideButton, &QPushButton::clicked, this, [&] {
		hide();
	});
	connect(ui->ESCButton, &QPushButton::clicked, this, [&] {
		QApplication::exit();
	});

	/* 设置图片项 */ 
	ui->ImgListWidget->setViewMode(QListWidget::IconMode);
	ui->ImgListWidget->setIconSize(QSize(300, 300));

	/* 设置文件项 */
	fileModel = new QFileSystemModel(this);
	proxyModel = new QSortFilterProxyModel(this);

	// false --仅显示符合条件的文件
	fileModel->setNameFilterDisables(false);
	proxyModel->setSourceModel(fileModel);
	ui->DoclistView->setModel(proxyModel);


}

Clipboard::~Clipboard()
{
    delete ui;
}

// 开启鼠标钩子
void Clipboard::startMouseHook()
{
	mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseProc, GetModuleHandle(NULL), 0);
	if (mouseHook == NULL) qDebug() << "Failed to install mouse hook";
}

// 停止鼠标钩子
void Clipboard::stopMouseHook()
{
	UnhookWindowsHookEx(mouseHook);
	mouseHook = NULL;
}

void Clipboard::onClipboardDataChanged()
{
	const QMimeData* mimeData = clipboard->mimeData();
	if(Clipboard::mouseX - width() / 2 < 0) move(0, 30);
	else if (Clipboard::mouseX - width() / 2 + width() > screenWidth) move(screenWidth - width(), 30);
	else move(Clipboard::mouseX - width() / 2, 30);
	show();
	// 图片区 or 文件区
	if (mimeData->hasFormat("text/uri-list")) {
		// 如果后缀不是图片类型的那么就以文件列表的形式展示出来

		QStringList paths = mimeData->text().split("\n", Qt::SkipEmptyParts);
		imgPaths.clear();
		docNames.clear();
		if (ui->ImgListWidget->count() > 0) qDeleteAll(ui->ImgListWidget->findItems(QString(), Qt::MatchContains));

		QSet<QString> validSuffixes = { "jpg", "jpeg", "png", "bmp" ,"webp","svg"};
		bool IsAllImg = true; // 是否全是图片
		for(const QString& path : paths) {
			QUrl url(QUrl::fromUserInput(path));
			QString localFilePath = url.toLocalFile();
			QFile file(localFilePath);
			imgPaths.append(localFilePath);
			// 获取文件后缀
			QFileInfo fileInfo(localFilePath);
			QString fileSuffix = fileInfo.suffix();

			if (!validSuffixes.contains(fileSuffix) && IsAllImg)
			{
				IsAllImg = false;
			}
		}
		// 部分或全部都不是图片的
		QStringList DocPaths = imgPaths;
		docNames.clear();
		if (!IsAllImg)
		{
			ui->stackedWidget_2->setCurrentWidget(ui->DocumentPage);

			for (const QString& DocPath : DocPaths)
			{
				QFileInfo fileInfo(DocPath);
				QString fileName = fileInfo.fileName();
				docNames.append(fileName);
			}
			fileModel->setNameFilters(QStringList() << docNames);
			QFileInfo fileInfo(DocPaths.at(0));
			QString directoryPath = fileInfo.path();
			fileModel->setRootPath(directoryPath);
			ui->DoclistView->setRootIndex(proxyModel->mapFromSource(fileModel->index(directoryPath)));

			return;
		}
		// 全为图片
		ui->stackedWidget_2->setCurrentWidget(ui->ImagePage);
		for (const QString& imgPath : imgPaths) {
			QListWidgetItem *item = new QListWidgetItem(ui->ImgListWidget);
			QPixmap pixmap(imgPath);
			item->setIcon(QIcon(pixmap));
			ui->ImgListWidget->addItem(item);
		}

	}
	// 文字区
	else if (mimeData->hasText() && !mimeData->hasFormat("text/uri-list"))
	{
		ui->stackedWidget_2->setCurrentWidget(ui->TextPage);
		QRegularExpression urlRegex("(https?|ftp)://([A-Za-z0-9.-]+)(/[^\\s]*)?");
		QRegularExpression emailRegex("\\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Z|a-z]{2,}\\b");

		QString htmlContent = mimeData->text();

		/* URLs */
		QRegularExpressionMatchIterator urlMatchIterator = urlRegex.globalMatch(htmlContent);
		while (urlMatchIterator.hasNext()) {
			QRegularExpressionMatch match = urlMatchIterator.next();
			QString url = match.captured();
			QString link = QString("<a href='%1'>%1</a>").arg(url);
			htmlContent.replace(url, link);
		}

		/* Emails */
		QRegularExpressionMatchIterator emailMatchIterator = emailRegex.globalMatch(htmlContent);
		while (emailMatchIterator.hasNext()) {
			QRegularExpressionMatch match = emailMatchIterator.next();
			QString email = match.captured();
			QString emailLink = QString("<a href='mailto:%1'>%1</a>").arg(email);
			htmlContent.replace(email, emailLink);
		}
		ui->textBrowser->setHtml(htmlContent);
		
		
		// 遍历格式并打印数据
		//for (const QString& format : mimeData->formats()) {
		//	qDebug() << "Format:" << format;

		//	// 打印每种格式对应的数据
		//	if (mimeData->hasFormat(format)) {
		//		const QByteArray data = mimeData->data(format);
		//		qDebug() << "Data:" << data;
		//	}
		//}
	}
	

}

void Clipboard::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		/* 鼠标全局坐标减窗口的坐标 */
		lastPos = event->globalPos() - frameGeometry().topLeft();
	}
}

void Clipboard::mouseMoveEvent(QMouseEvent *event)
{
	if (event->buttons() & Qt::LeftButton) {
		move(event->globalPos() - lastPos);
	}
}

/* 获取滚轮滚动值 */ 
void Clipboard::wheelEvent(QWheelEvent* event) {
	int delta = event->angleDelta().y();
	// 调节图片项的大小
	if (delta > 0) ui->ImgListWidget->setIconSize(ui->ImgListWidget->iconSize() + QSize(20,20));
	else ui->ImgListWidget->setIconSize(ui->ImgListWidget->iconSize() - QSize(20, 20));

	event->accept();
}