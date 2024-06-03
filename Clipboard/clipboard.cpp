#include "clipboard.h"

int Clipboard::mouseX = 0;
int Clipboard::mouseY = 0;
Clipboard* Clipboard::staticThis = nullptr;

#ifdef Q_OS_WIN32
HHOOK mouseHook = NULL;
HHOOK keyboardHook = NULL;


LRESULT CALLBACK MouseEvent(int nCode, WPARAM wParam, LPARAM lParam)
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

LRESULT CALLBACK KeyEvent(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode >= 0 && wParam == WM_KEYDOWN)
	{
		KBDLLHOOKSTRUCT* pKeyboardStruct = (KBDLLHOOKSTRUCT*)lParam;
		/* Ctrl + Shift + V || Ctrl + V */
		if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState(VK_SHIFT) & 0x8000) && (pKeyboardStruct->vkCode == 'V') || 
			(GetAsyncKeyState(VK_CONTROL) & 0x8000) && (pKeyboardStruct->vkCode == 'V'))
		{
			Clipboard::staticThis->setVisible(false);
		}
		/* Ctrl + Z */
		if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) && (pKeyboardStruct->vkCode == 'Z'))
		{
			// 撤回 ,当点击撤回键时，显示上一次复制的内容

		}
		/* Ctrl + Y */
		if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) && (pKeyboardStruct->vkCode == 'Y'))
		{
			// 恢复
		}
	}
	return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
}

#endif // Q_OS_WIN32

Clipboard::Clipboard(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::ClipboardClass())
	, lastPos(0,0)
	, settings("config.ini", QSettings::IniFormat)
	, ESCButton(new QPushButton(ui->toolBar))
	, HideButton(new QPushButton(ui->toolBar))
	, MoreButton(new QPushButton(ui->toolBar))
	, PreButton(new QPushButton(ui->toolBar))
	, NextButton(new QPushButton(ui->toolBar))
{
    ui->setupUi(this);
	setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
	staticThis = this;
	/* 初始化配置 */
	LoadSettings();

#ifdef Q_OS_WIN32

	/* 创建系统托盘菜单（仅WIN32） */
	QSystemTrayIcon *trayIcon = new QSystemTrayIcon(QIcon(":/Img/health.png"),this);
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

#endif // Q_OS_WIN32

	/* 获取设备屏幕的宽度 */
	screenWidth = QGuiApplication::primaryScreen()->geometry().width();

	/* 启动钩子 */
	startHook();
	
	/* 剪切板 */
	clipboard = QApplication::clipboard();

	ui->textBrowser->setText(clipboard->text());

	QTimer::singleShot(0, this, [&] {
		setVisible(false);
	});
	
	connect(clipboard, &QClipboard::dataChanged, this, &Clipboard::onClipboardDataChanged);

	connect(ESCButton, &QPushButton::clicked, this, [&] {
		QApplication::exit();
	});
	connect(HideButton, &QPushButton::clicked, this, [&] {
		setVisible(false);
	});
	/* 点击更多按钮 */
	connect(MoreButton, &QPushButton::clicked, this, &Clipboard::clickMoreButton);

	/* 点击上一个记录按钮 */
	connect(PreButton, &QPushButton::clicked, this, &Clipboard::Turn2PreHistory);

	/* 点击下一个记录按钮 */
	connect(NextButton, &QPushButton::clicked, this, &Clipboard::Turn2NextHistory);

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
		if (index.row() == 0) return; // 跳过第一项
		QString itemPath = DocPaths.at(index.row());
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
		if (index.row() == 0) return; // 跳过第一项
		if (index.isValid()) {
			QMenu menu;
			// 添加菜单项
			QAction *openAction = menu.addAction(QString::fromLocal8Bit("打开文件路径"));
			QAction *startAction = menu.addAction(QString::fromLocal8Bit("启动文件"));
			QString itemPath = DocPaths.at(index.row());

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
#ifdef Q_OS_WIN32
	if (mouseHook)
	{
		UnhookWindowsHookEx(mouseHook);
		mouseHook = NULL;
	}
	if (keyboardHook)
	{
		UnhookWindowsHookEx(keyboardHook);
		keyboardHook = NULL;
	}
#endif // Q_OS_WIN32

    delete ui;
}

void Clipboard::LoadSettings()
{
	QFont textFont("SimSun", 13);
	textFont.setWeight(QFont::DemiBold);

	ui->toolBar->addWidget(ESCButton);
	ui->toolBar->addWidget(HideButton);
	ui->toolBar->addWidget(MoreButton);
	ui->toolBar->addWidget(PreButton);
	ui->toolBar->addWidget(NextButton);

	ui->statusBar->setFixedHeight(12);

	ui->toolBar->setFont(textFont);

	ESCButton->setFixedSize(30,30);
	ESCButton->setText("X");
	HideButton->setFixedSize(30, 30);
	HideButton->setText("-");
	MoreButton->setFixedSize(30, 30);
	MoreButton->setText("?");
	PreButton->setFixedSize(30, 30);
	PreButton->setText("<");
	NextButton->setFixedSize(30, 30);
	NextButton->setText(">");
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
	// 设置字体颜色
	QPalette TextPalette = ui->textBrowser->palette();
	QPalette DocPalette = ui->DoclistView->palette();
	QPalette ImgPalette = ui->ImgListWidget->palette();
	if (!settings.contains("CurrentBackgroundImgIndex")) {
		settings.setValue("CurrentBackgroundImgIndex", 1);
	}
	if (!settings.contains("CurrentBackgroundImgPath")) {
		settings.setValue("CurrentBackgroundImgPath", "");
	}
	if (!settings.contains("CurrentTextColor")) {
		settings.setValue("CurrentTextColor", "#d0d0d0");
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
	TextPalette.setColor(QPalette::Text, CurrentTextColor);
	DocPalette.setColor(QPalette::Text, CurrentTextColor);
	ImgPalette.setColor(QPalette::Text, CurrentTextColor);

	ui->DoclistView->setPalette(DocPalette);
	ui->ImgListWidget->setPalette(ImgPalette);
	ui->centralWidget->setStyleSheet("QWidget#centralWidget{border-image:url(:/Img/bgi1Border.png);}");
	ui->statusBar->setStyleSheet(("QWidget#statusBar{background-image:url(:/Img/bgi1Border.png);}"));
	ui->toolBar->setStyleSheet(("QWidget#toolBar{background-image:url(:/Img/bgi1Border.png);}"));
	ui->textBrowser->setFont(textFont);
	ui->textBrowser->setPalette(TextPalette);
	ui->textBrowser->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);

	ui->DoclistView->setFont(textFont);

	ui->ImgListWidget->setFont(textFont);

	HideButton->setStyleSheet(buttonStyleSheet);
	ESCButton->setStyleSheet(buttonStyleSheet);
	MoreButton->setStyleSheet(buttonStyleSheet);
	PreButton->setStyleSheet(buttonStyleSheet);
	NextButton->setStyleSheet(buttonStyleSheet);

	ui->textBrowser->setOpenExternalLinks(true);
	ui->SearchEdit->setVisible(false);
	ui->SearchEdit->setPlaceholderText(QString::fromLocal8Bit("请输入需要匹配的文本，\"Ctrl + F\" 可以开启或关闭搜索栏"));
}

// 开启鼠标钩子
void Clipboard::startHook()
{
#ifdef Q_OS_WIN32
	/* 鼠标钩子 */
	mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseEvent, GetModuleHandle(NULL), 0);
	if (mouseHook == NULL) QMessageBox::warning(QApplication::activeWindow(), "error!", "Failed to install mouse hook", QMessageBox::Ok);
	/* 键盘钩子 */
	keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyEvent, GetModuleHandle(NULL), 0);
	if (keyboardHook == NULL) QMessageBox::warning(QApplication::activeWindow(), "error!", "Failed to install keyboard hook", QMessageBox::Ok);

#endif // Q_OS_WIN32

}

void Clipboard::onClipboardDataChanged()
{
	if (!ui->textBrowser->hasFocus()) /* 只有当焦点在外部时才触发 */
	{
		const QMimeData* mimeData = clipboard->mimeData();
		QDateTime currentDateTime = QDateTime::currentDateTime();
		QString currentDateTimeString = currentDateTime.toString("yyyy-MM-dd hh:mm:ss");
		show();

		if (Clipboard::mouseX - width() / 2 < 0) move(0, 30);
		else if (Clipboard::mouseX - width() / 2 + width() > screenWidth) move(screenWidth - width(), 30);
		else move(Clipboard::mouseX - width() / 2, 30);

		// 图片区 or 文件区
		if (mimeData->hasFormat("text/uri-list")) {
			// 如果后缀不是图片类型的那么就以文件列表的形式展示出来

			QStringList paths = mimeData->text().split("\n", Qt::SkipEmptyParts);
			
			ImgPaths.clear();
			if (ui->ImgListWidget->count() > 0) qDeleteAll(ui->ImgListWidget->findItems(QString(), Qt::MatchContains));

			QSet<QString> validSuffixes = { "jpg", "jpeg", "png", "bmp" ,"webp","svg" };
			bool IsAllImg = true; // 是否全是图片
			for (const QString& path : paths) {
				QUrl url(QUrl::fromUserInput(path));
				QString localFilePath = url.toLocalFile();
				QFile file(localFilePath);
				ImgPaths.append(localFilePath);
				// 获取文件后缀
				QFileInfo fileInfo(localFilePath);
				QString fileSuffix = fileInfo.suffix();

				if (!validSuffixes.contains(fileSuffix) && IsAllImg)
				{
					IsAllImg = false;
				}
			}
			// 1.文件区 部分或全部都不是图片
			QStringList docPaths = ImgPaths;
			DocPaths.clear();
			if (!IsAllImg)
			{
				ui->stackedWidget_2->setCurrentWidget(ui->DocumentPage);

				DocPaths.append(currentDateTimeString);
				for (const QString& docPath : docPaths) DocPaths.append(docPath);
				ui->DoclistView->setModel(new QStringListModel(DocPaths, this));
				// 保存的是QStringList DocPaths
				SaveHistory(ContentType::DOC, DocPaths);
				return;
			}
			// 2.图片区 全为图片
			ui->stackedWidget_2->setCurrentWidget(ui->ImagePage);
			QListWidgetItem *currentTimeItem = new QListWidgetItem(ui->ImgListWidget);
			currentTimeItem->setText(currentDateTimeString);
			ui->ImgListWidget->addItem(currentTimeItem);
			ImgPaths.insert(0, currentDateTimeString);

			for (const QString& imgPath : ImgPaths) {
				QListWidgetItem *item = new QListWidgetItem(ui->ImgListWidget);
				QPixmap pixmap(imgPath);
				item->setIcon(QIcon(pixmap));
				ui->ImgListWidget->addItem(item);
			}
			SaveHistory(ContentType::IMG, ImgPaths);
		}
		// 3.文字区
		else if (mimeData->hasText() && !mimeData->hasFormat("text/uri-list"))
		{
			ui->stackedWidget_2->setCurrentWidget(ui->TextPage);
			QRegularExpression urlRegex("(https?|ftp)://([A-Za-z0-9.-]+)(/[^\\s]*)?");
			QRegularExpression emailRegex("\\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Z|a-z]{2,}\\b");

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

			QStringList mimeTextList;
			mimeTextList.append(currentDateTimeString);
			mimeTextList.append(mimeText);
			SaveHistory(ContentType::TEXT, mimeTextList);

			handledText = "<html><pre>" + currentDateTimeString + "\n" + mimeText + "</pre></html>";
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
		QMessageBox::warning(QApplication::activeWindow(), QString::fromLocal8Bit("提示"), QString::fromLocal8Bit("通过Ctrl+F可以启动文本匹配功能"), QMessageBox::Ok);
	});

	/* 更改文本颜色 */
	connect(ChangeTextColorAction, &QAction::triggered, this, [&]() {
		QPalette TextPalette = ui->textBrowser->palette();
		QPalette DocPalette = ui->DoclistView->palette();
		QPalette ImgPalette = ui->ImgListWidget->palette();
		QColor textColor = TextPalette.color(QPalette::Text);

		QColor color = QColorDialog::getColor(textColor, this, QString::fromLocal8Bit("选择一种文本颜色"));
		if (color.isValid()) {
			TextPalette.setColor(QPalette::Text, color);
			DocPalette.setColor(QPalette::Text, color);
			ImgPalette.setColor(QPalette::Text, color);
			ui->textBrowser->setPalette(TextPalette);
			ui->DoclistView->setPalette(DocPalette);
			ui->ImgListWidget->setPalette(ImgPalette);
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

	menu.exec(MoreButton->mapToGlobal(QPoint(0, MoreButton->height())));
}

/* 跳转上次记录 */
void Clipboard::Turn2PreHistory()
{
	QFile file("ContentHistory.txt");
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

	QStringList contentList;
	QTextStream in(&file);

	bool readLines = false;
	QString line;
	QDateTime currentDateTime = QDateTime::currentDateTime();
	QString currentYear = currentDateTime.toString("yyyy");
	QString combinedLines; // 用于存储组合的行

	while (!in.atEnd()) {
		line = in.readLine();

		if (line.startsWith(currentYear) && readLines) {
			// 遇到以年份开头的行且已经开始读取，停止读取
			contentList.append(combinedLines);
			combinedLines.clear();
			readLines = false;
		}
		if (readLines) {
			int commaIndex = line.indexOf(',');
			if (commaIndex != -1) {
				line[commaIndex] = '\n';
			}
			combinedLines += line;
		}

		if (line.startsWith(currentYear)) {
			int commaIndex = line.indexOf(',');
			if (commaIndex != -1) {
				line[commaIndex] = '\n';
			}
			readLines = true;
			combinedLines += line;
		}
	}
	/* 如果只有一条记录 */
	if (!combinedLines.isEmpty()) {
		contentList.append(combinedLines);
	}

	qDebug() << contentList.at(0);
	handledText = "<html><pre>" + contentList.at(0) + "</pre></html>";
	ui->textBrowser->setHtml(handledText);
	file.close();
}

/* 跳转到下次记录 */
void Clipboard::Turn2NextHistory()
{

}

/* 保存复制记录 */
void Clipboard::SaveHistory(ContentType contentType, const QStringList& contents)
{
	QFile file("ContentHistory.txt");
	if (file.open(QIODevice::Append | QIODevice::Text))
	{
		QTextStream out(&file);
		switch (contentType)
		{
		case Clipboard::TEXT:
			out << contents.join(",");
			break;
		case Clipboard::IMG:
			out << contents.join(",");
			break;
		case Clipboard::DOC:
			out << contents.join(",");
			break;
		default:
			break;
		}
		out << "\n";
	}
	else QMessageBox::warning(QApplication::activeWindow(), "error!", QString::fromLocal8Bit("不能保存历史记录，请检查文件!"), QMessageBox::Ok);
	
	file.close();
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