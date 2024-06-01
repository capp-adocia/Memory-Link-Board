#include "clipboard.h"

HHOOK mouseHook;
int Clipboard::mouseX = 0;
int Clipboard::mouseY = 0;

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
	, settings("config.ini", QSettings::IniFormat)
{
    ui->setupUi(this);
	setWindowFlags(Qt::Window | Qt::CustomizeWindowHint | Qt::WindowStaysOnTopHint);

	/* 初始化配置 */
	LoadSettings();

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
	/* 点击更多按钮 */
	connect(ui->MoreButton, &QPushButton::clicked, this, &Clipboard::clickMoreButton);

	/* 文本匹配 */
	connect(ui->SearchEdit, &QLineEdit::textChanged, this, [&] {
		// 清空之前样式
		ui->textBrowser->setHtml(handledText);
		QTextDocument *doc = ui->textBrowser->document();
		QTextCursor cursor = doc->find(ui->SearchEdit->text());

		while (!cursor.isNull()) {
			QTextCharFormat format;
			format.setBackground(Qt::darkCyan);
			cursor.mergeCharFormat(format);
			cursor = doc->find(ui->SearchEdit->text(), cursor.position() + ui->SearchEdit->text().length());
		}
	});
	connect(ui->SearchEdit, &QLineEdit::returnPressed, this, [&] {
		QTextDocument *doc = ui->textBrowser->document();
		QTextCursor cursor = doc->find(ui->SearchEdit->text(), ui->textBrowser->textCursor().position());
		if (!cursor.isNull()) {
			ui->textBrowser->setTextCursor(cursor);
			ui->textBrowser->ensureCursorVisible();
		}
	});

	/* 设置图片项 */ 
	ui->ImgListWidget->setViewMode(QListWidget::IconMode);
	ui->ImgListWidget->setIconSize(QSize(300, 300));

	/* 设置文件项 */
	ui->DoclistView->setEditTriggers(QAbstractItemView::NoEditTriggers);
	ui->DoclistView->setSpacing(3);
	ui->DoclistView->setContextMenuPolicy(Qt::CustomContextMenu);
	ui->DoclistView->setUniformItemSizes(true);

	/* 左键点击文件项（打开文件路径） */
	connect(ui->DoclistView, &QListView::clicked, this, [=](const QModelIndex &index) {
		QString itemPath = docNames.at(index.row());

		QFileInfo info(itemPath);
		if (info.isDir()) {
			// 点击项是文件夹
			QDesktopServices::openUrl(QUrl::fromLocalFile(itemPath));
		}
		else {
			// 点击项是文件
			QString folderPath = info.dir().path();
			QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath));
		}
	});

	/* 右键点击文件项 */
	connect(ui->DoclistView, &QListView::customContextMenuRequested, this, [=](const QPoint &pos) {
		QModelIndex index = ui->DoclistView->indexAt(pos);
		if (index.isValid()) {
			QMenu menu;
			// 添加菜单项
			QAction *openAction = menu.addAction(QString::fromLocal8Bit("打开文件路径"));
			QAction *startAction = menu.addAction(QString::fromLocal8Bit("启动文件"));
			QString itemPath = docNames.at(index.row());

			/* 打开文件路径 */
			connect(openAction, &QAction::triggered, this, [&]() {
				QFileInfo info(itemPath);
				if (info.isDir()) {
					QDesktopServices::openUrl(QUrl::fromLocalFile(itemPath));
				}
				else {
					QString folderPath = info.dir().path();
					QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath));
				}
			});

			/* 启动文件 */
			connect(startAction, &QAction::triggered, this, [&]() {
				QDesktopServices::openUrl(QUrl::fromLocalFile(itemPath));
			});
			
			menu.exec(ui->DoclistView->mapToGlobal(pos));
		}
	});

}

Clipboard::~Clipboard()
{
    delete ui;
}

void Clipboard::LoadSettings()
{
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
	QFont textFont("SimSun", 13);
	textFont.setWeight(QFont::DemiBold);
	// 设置字体颜色
	QPalette palette = ui->DoclistView->palette();
	if (!settings.contains("CurrentBackgroundImgIndex")) {
		settings.setValue("CurrentBackgroundImgIndex", 1);
	}
	if (!settings.contains("CurrentBackgroundImgPath")) {
		settings.setValue("CurrentBackgroundImgPath", "");
	}
	if (!settings.contains("CurrentTextColor")) {
		settings.setValue("CurrentTextColor", "#87bbff");
	}
	int CurrentBackgroundImgIndex = settings.value("CurrentBackgroundImgIndex").toInt();
	QString CurrentBackgroundImgPath = settings.value("CurrentBackgroundImgPath").toString();
	QString CurrentTextColor = settings.value("CurrentTextColor").toString();
	/* 自定义图片 */
	if (CurrentBackgroundImgIndex == 0)
	{
		ui->textBrowser->setStyleSheet(QString("QWidget#textBrowser{border-image:url(%1);}").arg(CurrentBackgroundImgPath));
		ui->ImgListWidget->setStyleSheet(QString("QWidget#ImgListWidget{border-image:url(%1);}").arg(CurrentBackgroundImgPath));
		ui->DoclistView->setStyleSheet(QString("QWidget#DoclistView{border-image:url(%1);}").arg(CurrentBackgroundImgPath));
	}
	else
	{
		ui->textBrowser->setStyleSheet(QString("QWidget#textBrowser{border-image:url(:/Img/bgi%1.png);}").arg(CurrentBackgroundImgIndex));
		ui->ImgListWidget->setStyleSheet(QString("QWidget#ImgListWidget{border-image:url(:/Img/bgi%1.png);}").arg(CurrentBackgroundImgIndex));
		ui->DoclistView->setStyleSheet(QString("QWidget#DoclistView{border-image:url(:/Img/bgi%1.png);}").arg(CurrentBackgroundImgIndex));
	}
	palette.setColor(QPalette::Text, CurrentTextColor);
	
	ui->centralWidget->setStyleSheet("QWidget#centralWidget{border-image:url(:/Img/bgi1Border.png);}");
	ui->textBrowser->setFont(textFont);
	ui->textBrowser->setPalette(palette);

	ui->DoclistView->setFont(textFont);
	ui->DoclistView->setPalette(palette);

	ui->HideButton->setStyleSheet(buttonStyleSheet);
	ui->ESCButton->setStyleSheet(buttonStyleSheet);
	ui->MoreButton->setStyleSheet(buttonStyleSheet);
	ui->textBrowser->setOpenExternalLinks(true);
	ui->SearchEdit->setVisible(false);
	ui->SearchEdit->setPlaceholderText(QString::fromLocal8Bit("请输入需要匹配的文本，\"Ctrl + F\" 可以开启或关闭搜索栏"));
}

// 开启鼠标钩子
void Clipboard::startMouseHook()
{
	mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseProc, GetModuleHandle(NULL), 0);
	if (mouseHook == NULL) QMessageBox::warning(nullptr, "error!", "Failed to install mouse hook", QMessageBox::Ok);
}

void Clipboard::onClipboardDataChanged()
{
	if (!ui->textBrowser->hasFocus()) /* 只有当焦点在外部时才触发 */
	{
		const QMimeData* mimeData = clipboard->mimeData();
		if (Clipboard::mouseX - width() / 2 < 0) move(0, 30);
		else if (Clipboard::mouseX - width() / 2 + width() > screenWidth) move(screenWidth - width(), 30);
		else move(Clipboard::mouseX - width() / 2, 30);
		show();
		// 图片区 or 文件区
		if (mimeData->hasFormat("text/uri-list")) {
			// 如果后缀不是图片类型的那么就以文件列表的形式展示出来

			QStringList paths = mimeData->text().split("\n", Qt::SkipEmptyParts);
			imgPaths.clear();
			if (ui->ImgListWidget->count() > 0) qDeleteAll(ui->ImgListWidget->findItems(QString(), Qt::MatchContains));

			QSet<QString> validSuffixes = { "jpg", "jpeg", "png", "bmp" ,"webp","svg" };
			bool IsAllImg = true; // 是否全是图片
			for (const QString& path : paths) {
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

				for (const QString& DocPath : DocPaths) docNames.append(DocPath);

				ui->DoclistView->setModel(new QStringListModel(docNames, this));
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
			//QRegularExpression englishWordPattern("\\b[A-Za-z]+\\b");

			QString mimeText = (mimeData->*(&QMimeData::text))();

			/* URLs */
			QRegularExpressionMatchIterator urlMatchIterator = urlRegex.globalMatch(mimeText);
			while (urlMatchIterator.hasNext()) {
				QRegularExpressionMatch match = urlMatchIterator.next();
				QString url = match.captured();
				QString link = QString("<a href='%1'>%1</a>").arg(url);
				mimeText.replace(url, link);
			}

			/* Emails */
			QRegularExpressionMatchIterator emailMatchIterator = emailRegex.globalMatch(mimeText);
			while (emailMatchIterator.hasNext()) {
				QRegularExpressionMatch match = emailMatchIterator.next();
				QString email = match.captured();
				QString emailLink = QString("<a href='mailto:%1'>%1</a>").arg(email);
				mimeText.replace(email, emailLink);
			}

			handledText = "<html><pre>" + mimeText + "</pre></html>";
			ui->textBrowser->setHtml(handledText);

		}
	}

}

void Clipboard::clickMoreButton()
{
	QMenu menu;
	// 添加菜单项
	QAction *HelpAction = menu.addAction(QString::fromLocal8Bit("帮助"));
	QAction *ChangeTextColorAction = menu.addAction(QString::fromLocal8Bit("更改文本颜色"));
	QAction *ChangeBackgroundImageAction = menu.addAction(QString::fromLocal8Bit("更改背景图片"));


	/* 帮助 */
	connect(HelpAction, &QAction::triggered, this, [&]() {
		// 功能暂定
		QMessageBox::warning(nullptr, QString::fromLocal8Bit("提示"), QString::fromLocal8Bit("通过Ctrl+F可以启动文本匹配功能"), QMessageBox::Ok);
	});

	/* 更改文本颜色 */
	connect(ChangeTextColorAction, &QAction::triggered, this, [&]() {
		QPalette palette = ui->textBrowser->palette();
		QColor textColor = palette.color(QPalette::Text);

		QColor color = QColorDialog::getColor(textColor, this, QString::fromLocal8Bit("选择一种文本颜色"));
		if (color.isValid()) {
			palette.setColor(QPalette::Text, color);
			ui->textBrowser->setPalette(palette);
			settings.setValue("CurrentTextColor", color);
		}
	});

	/* 更改背景图片 */
	connect(ChangeBackgroundImageAction, &QAction::triggered, this, [&]() {
		QMenu backgroundImgMenu;
		QAction *BackgroundImg1Action = backgroundImgMenu.addAction(QString::fromLocal8Bit("背景1"));
		QAction *BackgroundImg2Action = backgroundImgMenu.addAction(QString::fromLocal8Bit("背景2"));
		QAction *customizeAction = backgroundImgMenu.addAction(QString::fromLocal8Bit("自定义"));
		BackgroundImg1Action->setCheckable(true);
		BackgroundImg2Action->setCheckable(false);
		customizeAction->setCheckable(false);
		/* 背景1 */
		connect(BackgroundImg1Action, &QAction::triggered, this, [&]() {
			ui->textBrowser->setStyleSheet("QWidget#textBrowser{border-image:url(:/Img/bgi1.png);}");
			ui->ImgListWidget->setStyleSheet("QWidget#ImgListWidget{border-image:url(:/Img/bgi1.png);}");
			ui->DoclistView->setStyleSheet("QWidget#DoclistView{border-image:url(:/Img/bgi1.png);}");

			settings.setValue("CurrentBackgroundImgIndex", 1);

		});
		/* 背景2 */
		connect(BackgroundImg2Action, &QAction::triggered, this, [&]() {
			ui->textBrowser->setStyleSheet("QWidget#textBrowser{border-image:url(:/Img/bgi2.png);}");
			ui->ImgListWidget->setStyleSheet("QWidget#ImgListWidget{border-image:url(:/Img/bgi2.png);}");
			ui->DoclistView->setStyleSheet("QWidget#DoclistView{border-image:url(:/Img/bgi2.png);}");

			settings.setValue("CurrentBackgroundImgIndex", 2);
		});
		/* 自定义 */
		connect(customizeAction, &QAction::triggered, this, [&]() {
			QString imagePath = QFileDialog::getOpenFileName(this, QString::fromLocal8Bit("选择你的背景图片"), QDir::homePath(), "Images (*.png *.jpg)");
			if (!imagePath.isEmpty()) {
				QString textBrowserWidgetStyleSheet = "QWidget#textBrowser{border-image:url(" + imagePath + ");}";
				QString ImgListWidgetStyleSheet = "QWidget#textBrowser{border-image:url(" + imagePath + ");}";
				QString DoclistViewStyleSheet = "QWidget#textBrowser{border-image:url(" + imagePath + ");}";
				ui->textBrowser->setStyleSheet(textBrowserWidgetStyleSheet);
				ui->ImgListWidget->setStyleSheet(ImgListWidgetStyleSheet);
				ui->DoclistView->setStyleSheet(DoclistViewStyleSheet);

				settings.setValue("CurrentBackgroundImgIndex", 0);
				settings.setValue("CurrentBackgroundImgPath", imagePath);
			}
		});

		backgroundImgMenu.exec(menu.mapToGlobal(QPoint(0, 0)));
	});

	menu.exec(ui->MoreButton->mapToGlobal(QPoint(0, ui->MoreButton->height())));
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
void Clipboard::wheelEvent(QWheelEvent* event)
{
	int delta = event->angleDelta().y();
	// 调节图片项的大小
	if (delta > 0) ui->ImgListWidget->setIconSize(ui->ImgListWidget->iconSize() + QSize(20,20));
	else ui->ImgListWidget->setIconSize(ui->ImgListWidget->iconSize() - QSize(20, 20));

	event->accept();
}

void Clipboard::keyPressEvent(QKeyEvent *event)
{
	if (event->modifiers() == Qt::ControlModifier &&
		event->key() == Qt::Key_F &&
		(ui->stackedWidget_2->currentWidget() == ui->TextPage)) 
	{
		/* 切换显示搜索栏 */
		if (ui->SearchEdit->isVisible())
		{
			ui->SearchEdit->setVisible(false);
			ui->textBrowser->setFocus();
			return;
		}
		ui->SearchEdit->setVisible(true);
		ui->SearchEdit->setFocus();
		return;
	}
	QWidget::keyPressEvent(event);
}