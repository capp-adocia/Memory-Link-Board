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
	/* ��ʼ����ʽ */
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

	/* ����ϵͳ���̲˵� */
	QSystemTrayIcon *trayIcon = new QSystemTrayIcon(QIcon("C:\\Users\\26364\\Desktop\\MC_res\\health.png"),this);
	QMenu *trayMenu = new QMenu(this);
	QAction *exitAction = new QAction("Exit", this);
	trayMenu->addSeparator();
	trayMenu->addAction(exitAction);

	trayIcon->setContextMenu(trayMenu);
	trayIcon->show();

	connect(trayIcon, &QSystemTrayIcon::activated, this, [=](QSystemTrayIcon::ActivationReason reason){
		//��������ͼ��
		if (reason == QSystemTrayIcon::Trigger) isVisible() ? hide() : show();
	});
	connect(exitAction, &QAction::triggered, [&]() {
		QApplication::exit();
	});

	/* ��ȡ�豸��Ļ�Ŀ�� */
	screenWidth = QGuiApplication::primaryScreen()->geometry().width();

	/* �������� */
	startMouseHook();
	
	/* ���а� */
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

	/* ����ͼƬ�� */ 
	ui->ImgListWidget->setViewMode(QListWidget::IconMode);
	ui->ImgListWidget->setIconSize(QSize(300, 300));

	/* �����ļ��� */
	fileModel = new QFileSystemModel(this);
	proxyModel = new QSortFilterProxyModel(this);

	// false --����ʾ�����������ļ�
	fileModel->setNameFilterDisables(false);
	proxyModel->setSourceModel(fileModel);
	ui->DoclistView->setModel(proxyModel);


}

Clipboard::~Clipboard()
{
    delete ui;
}

// ������깳��
void Clipboard::startMouseHook()
{
	mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseProc, GetModuleHandle(NULL), 0);
	if (mouseHook == NULL) qDebug() << "Failed to install mouse hook";
}

// ֹͣ��깳��
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
	// ͼƬ�� or �ļ���
	if (mimeData->hasFormat("text/uri-list")) {
		// �����׺����ͼƬ���͵���ô�����ļ��б����ʽչʾ����

		QStringList paths = mimeData->text().split("\n", Qt::SkipEmptyParts);
		imgPaths.clear();
		docNames.clear();
		if (ui->ImgListWidget->count() > 0) qDeleteAll(ui->ImgListWidget->findItems(QString(), Qt::MatchContains));

		QSet<QString> validSuffixes = { "jpg", "jpeg", "png", "bmp" ,"webp","svg"};
		bool IsAllImg = true; // �Ƿ�ȫ��ͼƬ
		for(const QString& path : paths) {
			QUrl url(QUrl::fromUserInput(path));
			QString localFilePath = url.toLocalFile();
			QFile file(localFilePath);
			imgPaths.append(localFilePath);
			// ��ȡ�ļ���׺
			QFileInfo fileInfo(localFilePath);
			QString fileSuffix = fileInfo.suffix();

			if (!validSuffixes.contains(fileSuffix) && IsAllImg)
			{
				IsAllImg = false;
			}
		}
		// ���ֻ�ȫ��������ͼƬ��
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
		// ȫΪͼƬ
		ui->stackedWidget_2->setCurrentWidget(ui->ImagePage);
		for (const QString& imgPath : imgPaths) {
			QListWidgetItem *item = new QListWidgetItem(ui->ImgListWidget);
			QPixmap pixmap(imgPath);
			item->setIcon(QIcon(pixmap));
			ui->ImgListWidget->addItem(item);
		}

	}
	// ������
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
		
		
		// ������ʽ����ӡ����
		//for (const QString& format : mimeData->formats()) {
		//	qDebug() << "Format:" << format;

		//	// ��ӡÿ�ָ�ʽ��Ӧ������
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
		/* ���ȫ����������ڵ����� */
		lastPos = event->globalPos() - frameGeometry().topLeft();
	}
}

void Clipboard::mouseMoveEvent(QMouseEvent *event)
{
	if (event->buttons() & Qt::LeftButton) {
		move(event->globalPos() - lastPos);
	}
}

/* ��ȡ���ֹ���ֵ */ 
void Clipboard::wheelEvent(QWheelEvent* event) {
	int delta = event->angleDelta().y();
	// ����ͼƬ��Ĵ�С
	if (delta > 0) ui->ImgListWidget->setIconSize(ui->ImgListWidget->iconSize() + QSize(20,20));
	else ui->ImgListWidget->setIconSize(ui->ImgListWidget->iconSize() - QSize(20, 20));

	event->accept();
}