#include "clipboard.h"

int Clipboard::mouseX = 0;
int Clipboard::mouseY = 0;

#ifdef Q_OS_WIN32
HHOOK mouseHook = NULL;

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
#endif // Q_OS_WIN32

/* 中文转Unicode */
QString operator""_toUnicode(const char* CN_Str, size_t len) { return QString::fromLocal8Bit(CN_Str); }

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
	, FixButton(new QPushButton(ui->toolBar))
	, IsFixed(true)
	, HisConCount(0)
	, HisConOffset(0)
	, LeftHistoryMSGboxShown(false)
	, RightHistoryMSGboxShown(false)
{
    ui->setupUi(this);
	setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
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

	/* 获取设备屏幕的宽高 */
	screenWidth = QGuiApplication::primaryScreen()->geometry().width();
	screenHeight = QGuiApplication::primaryScreen()->geometry().height();
	
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
	connect(PreButton, &QPushButton::clicked, [&] {
		Turn2History(LEFT);
	});

	/* 点击下一个记录按钮 */
	connect(NextButton, &QPushButton::clicked, [&] {
		Turn2History(RIGHT);
	});
	/* 点击固定按钮 */
	connect(FixButton, &QPushButton::clicked, this, [&] {
		IsFixed = !IsFixed;
		IsFixed ? stopMouseHook() : startMouseHook();
		IsFixed ? FixButton->setIcon(QIcon(":/Img/fixed.png")) : FixButton->setIcon(QIcon(":/Img/unfixed.png"));
		IsFixed ? FixButton->setToolTip("已固定"_toUnicode) : FixButton->setToolTip("未固定"_toUnicode);
		settings.setValue("IsFixed", IsFixed);
	});
	/* 文本匹配 */
	connect(ui->SearchEdit, &QLineEdit::textChanged, this, [&] {
		// 清空之前样式
		ui->textBrowser->setText(handledText);
		QTextDocument *doc = ui->textBrowser->document();
		QTextCursor cursor = doc->find(ui->SearchEdit->text());

		while (!cursor.isNull()) {
			QTextCharFormat format;
			format.setBackground(Qt::darkCyan);
			cursor.mergeCharFormat(format);
			cursor = doc->find(ui->SearchEdit->text(), cursor.position() + ui->SearchEdit->text().length());
		}
	});
	// 搜索存有bug
	//connect(ui->SearchEdit, &QLineEdit::returnPressed, this, [&] {
	//	QTextDocument *doc = ui->textBrowser->document();
	//	QTextCursor cursor = doc->find(ui->SearchEdit->text(), ui->textBrowser->textCursor().position());
	//	if (!cursor.isNull()) {
	//		ui->textBrowser->setTextCursor(cursor);
	//		ui->textBrowser->ensureCursorVisible();
	//	}
	//});

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
			QAction *openAction = menu.addAction("打开文件路径"_toUnicode);
			QAction *startAction = menu.addAction("启动文件"_toUnicode);
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
	stopMouseHook();
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
	ui->toolBar->addWidget(FixButton);

	ui->statusBar->setFixedHeight(12);
	ui->toolBar->setFont(textFont);
	
	ESCButton->setFixedSize(BUTTONSIZE);
	ESCButton->setToolTip("退出"_toUnicode);
	ESCButton->setText("X");
	HideButton->setFixedSize(BUTTONSIZE);
	HideButton->setText("-");
	HideButton->setToolTip("最小化"_toUnicode);
	MoreButton->setFixedSize(BUTTONSIZE);
	MoreButton->setText("?");
	MoreButton->setToolTip("更多"_toUnicode);
	PreButton->setFixedSize(BUTTONSIZE);
	PreButton->setText("<");
	PreButton->setToolTip("前一次记录"_toUnicode);
	NextButton->setFixedSize(BUTTONSIZE);
	NextButton->setText(">");
	NextButton->setToolTip("后一次记录"_toUnicode);
	FixButton->setFixedSize(BUTTONSIZE);

	if (!settings.contains("IsFixed")) settings.setValue("IsFixed", IsFixed);
	IsFixed = settings.value("IsFixed").toBool();
	IsFixed ? stopMouseHook() : startMouseHook();
	IsFixed ? FixButton->setIcon(QIcon(":/Img/fixed.png")) : FixButton->setIcon(QIcon(":/Img/unfixed.png"));
	IsFixed ? FixButton->setToolTip("已固定"_toUnicode) : FixButton->setToolTip("未固定"_toUnicode);

	if(settings.contains("historyContentCount")) HisConCount = settings.value("historyContentCount").toULongLong();

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
	QPalette DocPalette = ui->DoclistView->palette();
	DocPalette.setColor(QPalette::Text, CurrentTextColor);
	ui->DoclistView->setPalette(DocPalette);
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

	ui->centralWidget->setStyleSheet("QWidget#centralWidget{border-image:url(:/Img/bgi1Border.png);}");
	ui->statusBar->setStyleSheet(("QWidget#statusBar{background-image:url(:/Img/bgi1Border.png);}"));
	ui->toolBar->setStyleSheet(("QWidget#toolBar{background-image:url(:/Img/bgi1Border.png);}"));
	ui->textBrowser->setFont(textFont);
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
	ui->SearchEdit->setPlaceholderText("请输入需要匹配的文本，\"Ctrl + F\" 可以开启或关闭搜索栏"_toUnicode);
}

void Clipboard::LoadFontColor()
{
	// 加载字体颜色
	QPalette TextPalette = ui->textBrowser->palette();
	QPalette DocPalette = ui->DoclistView->palette();
	QPalette ImgPalette = ui->ImgListWidget->palette();
	QString CurrentTextColor = settings.value("CurrentTextColor").toString();
	TextPalette.setColor(QPalette::Text, CurrentTextColor);
	DocPalette.setColor(QPalette::Text, CurrentTextColor);
	ImgPalette.setColor(QPalette::Text, CurrentTextColor);
	ui->textBrowser->setPalette(TextPalette);
	ui->DoclistView->setPalette(DocPalette);
	ui->ImgListWidget->setPalette(ImgPalette);
}

void Clipboard::startMouseHook()
{
#ifdef Q_OS_WIN32
	/* 开启鼠标钩子 */
	if (!mouseHook) {
		mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseEvent, GetModuleHandle(NULL), 0);
		if (!mouseHook) QMessageBox::warning(this, "error!", "Failed to install mouse hook", QMessageBox::Ok);
	}
#endif // Q_OS_WIN32
}

void Clipboard::stopMouseHook()
{
#ifdef Q_OS_WIN32
	/* 关闭鼠标钩子 */
	if (mouseHook) {
		UnhookWindowsHookEx(mouseHook);
		mouseHook = NULL;
	}
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
		
		int GapX = Clipboard::mouseX - static_cast<int>(screenWidth / 2); // 计算鼠标和屏幕中心点之间差距
		int GapY = Clipboard::mouseY - static_cast<int>(screenHeight / 2);
		
		int newPointX = static_cast<int>(screenWidth / 2) - GapX;
		int newPointY = static_cast<int>(screenHeight / 2) - GapY;
		if (!IsFixed)
		{
			// 当新坐标超出屏幕的左上角
			if (newPointX + width() > screenWidth && newPointY + height() > screenHeight) move(screenWidth - width(), screenHeight - height()); 
			// 当新坐标超出屏幕的右上角
			else if (newPointX < 0 && newPointY < 0) move(0, 0);
			// 当新坐标超出屏幕左侧
			else if (newPointX < 0) move(0, newPointY);
			// 当新坐标超出屏幕右侧
			else if (newPointX + width() > screenWidth) move(screenWidth - width(), newPointY); 
			// 当新坐标超出屏幕上侧
			else if (newPointY < 0) move(newPointX, 0); 
			// 当新坐标超出屏幕下侧
			else if (newPointY + height() > screenHeight) move(newPointX, screenHeight - height());
			else move(newPointX, newPointY);
		}
		/* 重置记录偏移量 */
		HisConOffset = 0;
		settings.setValue("historyContentCount", ++HisConCount);
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

				QStringList modifiedDocPaths = DocPaths; // copy
				if (!modifiedDocPaths.isEmpty()) modifiedDocPaths[0] = "DOC" + currentDateTimeString; // 保存时需要带上 DOC
				SaveHistory(ContentType::DOC, modifiedDocPaths);
				return;
			}
			// 2.图片区 全为图片
			ui->stackedWidget_2->setCurrentWidget(ui->ImagePage);

			QListWidgetItem *currentTimeItem = new QListWidgetItem(ui->ImgListWidget);
			currentTimeItem->setText(currentDateTimeString);
			ui->ImgListWidget->addItem(currentTimeItem);
			ImgPaths.insert(0, currentDateTimeString);

			QString CurrentTextColor = settings.value("CurrentTextColor").toString();
			QPalette ImgPalette = ui->ImgListWidget->palette();
			ImgPalette.setColor(QPalette::Text, CurrentTextColor);
			ui->ImgListWidget->setPalette(ImgPalette);

			for (const QString& imgPath : ImgPaths) {
				QListWidgetItem *item = new QListWidgetItem(ui->ImgListWidget);
				QPixmap pixmap(imgPath);
				item->setIcon(QIcon(pixmap));
				ui->ImgListWidget->addItem(item);
				ui->ImgListWidget->addItem(item);
			}
			ImgPaths[0] = "IMG" + currentDateTimeString;
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
			mimeTextList.append("TXT" + currentDateTimeString);
			mimeTextList.append(mimeText);
			SaveHistory(ContentType::TXT, mimeTextList);

			handledText = currentDateTimeString + "\n" + mimeText;
			ui->textBrowser->setText(handledText);
		}
		// 设置字体颜色
		LoadFontColor();
	}

}

void Clipboard::clickMoreButton()
{
	QMenu menu;
	// 添加菜单项
	QAction *HelpAction = menu.addAction("帮助"_toUnicode);
	QAction *ChangeTextColorAction = menu.addAction("更改文本颜色"_toUnicode);
	QAction *ChangeBackgroundImageAction = menu.addAction("更改背景图片"_toUnicode);

	/* 帮助 */
	connect(HelpAction, &QAction::triggered, this, [&]() {
		// 功能暂定
		QMessageBox::warning(this, "提示"_toUnicode, "通过Ctrl+F可以启动文本匹配功能"_toUnicode, QMessageBox::Ok);
	});

	/* 更改文本颜色 */
	connect(ChangeTextColorAction, &QAction::triggered, this, [&]() {
		QPalette TextPalette = ui->textBrowser->palette();
		QPalette DocPalette = ui->DoclistView->palette();
		QPalette ImgPalette = ui->ImgListWidget->palette();
		QColor textColor = TextPalette.color(QPalette::Text);

		QColor color = QColorDialog::getColor(textColor, this, "选择一种文本颜色"_toUnicode);
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
		QAction *BackgroundImg1Action = backgroundImgMenu.addAction("背景1"_toUnicode);
		QAction *BackgroundImg2Action = backgroundImgMenu.addAction("背景2"_toUnicode);
		QAction *customizeAction = backgroundImgMenu.addAction("自定义"_toUnicode);
		BackgroundImg1Action->setCheckable(true);
		BackgroundImg2Action->setCheckable(false);
		customizeAction->setCheckable(false);
		/* 背景1 */
		connect(BackgroundImg1Action, &QAction::triggered, this, [&]() {
			ui->textBrowser->setStyleSheet("QWidget#textBrowser{border-image:url(:/Img/bgi1.png);}");
			ui->ImgListWidget->setStyleSheet("QWidget#ImgListWidget{border-image:url(:/Img/bgi1.png);}");
			ui->DoclistView->setStyleSheet("QWidget#DoclistView{border-image:url(:/Img/bgi1.png);}");

			settings.setValue("CurrentBackgroundImgIndex", 1);
			LoadFontColor();
		});
		/* 背景2 */
		connect(BackgroundImg2Action, &QAction::triggered, this, [&]() {
			ui->textBrowser->setStyleSheet("QWidget#textBrowser{border-image:url(:/Img/bgi2.png);}");
			ui->ImgListWidget->setStyleSheet("QWidget#ImgListWidget{border-image:url(:/Img/bgi2.png);}");
			ui->DoclistView->setStyleSheet("QWidget#DoclistView{border-image:url(:/Img/bgi2.png);}");

			settings.setValue("CurrentBackgroundImgIndex", 2);
			LoadFontColor();
		});
		/* 自定义 */
		connect(customizeAction, &QAction::triggered, this, [&]() {
			QString imagePath = QFileDialog::getOpenFileName(this, "选择你的背景图片"_toUnicode, QDir::homePath(), "Images (*.png *.jpg)");
			if (!imagePath.isEmpty()) {
				QString textBrowserWidgetStyleSheet = "QWidget#textBrowser{border-image:url(" + imagePath + ");}";
				QString ImgListWidgetStyleSheet = "QWidget#textBrowser{border-image:url(" + imagePath + ");}";
				QString DoclistViewStyleSheet = "QWidget#textBrowser{border-image:url(" + imagePath + ");}";
				ui->textBrowser->setStyleSheet(textBrowserWidgetStyleSheet);
				ui->ImgListWidget->setStyleSheet(ImgListWidgetStyleSheet);
				ui->DoclistView->setStyleSheet(DoclistViewStyleSheet);

				settings.setValue("CurrentBackgroundImgIndex", 0);
				settings.setValue("CurrentBackgroundImgPath", imagePath);
				LoadFontColor();
			}
		});

		backgroundImgMenu.exec(menu.mapToGlobal(QPoint(0, 0)));
	});

	menu.exec(MoreButton->mapToGlobal(QPoint(0, MoreButton->height())));
}

void Clipboard::Turn2History(qint8 direction)
{
	if (!isVisible()) return;
	if (direction == LEFT)
	{
		if (LeftHistoryMSGboxShown) return;
		if (RightHistoryMSGboxShown) RightHistoryMSGboxShown = false;
		// 如果翻到第零条记录
		if (HisConCount + HisConOffset == 1)
		{

			LeftHistoryMSGboxShown = true;
			QMessageBox::warning(this, "翻到头了"_toUnicode, "这已经是第一条记录了!"_toUnicode, QMessageBox::Ok);
			return;
		}
		--HisConOffset;
	}
	else if (direction == RIGHT)
	{
		if (RightHistoryMSGboxShown) return;
		if (LeftHistoryMSGboxShown) LeftHistoryMSGboxShown = false;
		// 如果翻到第最后一条记录 即当前位置就是最新记录
		if (HisConOffset == 0)
		{
			RightHistoryMSGboxShown = true;
			QMessageBox::warning(this, "翻到尾了"_toUnicode, "这已经是最后一条记录了!"_toUnicode, QMessageBox::Ok);
			return;
		}
		++HisConOffset;
	}

	QFile file("ContentHistory.txt");
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
	QTextStream in(&file);

	bool IsReadLining = false;
	QString line;
	QDateTime currentDateTime = QDateTime::currentDateTime();
	QString currentYear = currentDateTime.toString("yyyy");
	QString contentLines; // 用于存储组合的行
	quint64 currentHistoryLine = 0;

	while (!in.atEnd()) {
		line = in.readLine();
		// 如果以年份开头就自增 跳过 直到遇到当前保存的记录数
		if ((line.startsWith("TXT" + currentYear) ||
			line.startsWith("IMG" + currentYear) ||
			line.startsWith("DOC" + currentYear)) && !IsReadLining)
		{
			if (line.startsWith("TXT"))
			{
				int commaIndex = line.indexOf(',');
				if (commaIndex != -1) line[commaIndex] = '\n';
			}
			if (++currentHistoryLine == HisConCount + HisConOffset)
			{
				// 开始读取这一行，并且继续向下读取，直到遇到以年份开头
				contentLines += line;
				IsReadLining = true;
				continue;
			}
		}
		if (IsReadLining)
		{
			if (line.startsWith("TXT" + currentYear) ||
				line.startsWith("IMG" + currentYear) ||
				line.startsWith("DOC" + currentYear)) break;
			contentLines += "\n" + line;
		}
	}

	QString hisConType = contentLines.left(3);
	QStringList contentLinesList; // 保存切分号的数据
	contentLines = contentLines.mid(3); // 去除前三个字符

	// 判断是哪种类型的记录
	if (hisConType == "TXT")
	{
		ui->stackedWidget_2->setCurrentWidget(ui->TextPage);
		handledText = contentLines;
		ui->textBrowser->setText(contentLines);
	}
	else if (hisConType == "IMG")
	{
		ui->stackedWidget_2->setCurrentWidget(ui->ImagePage);
		// 释放之前的分配的内存
		while (ui->ImgListWidget->count() > 0) delete ui->ImgListWidget->takeItem(0);
		contentLinesList = contentLines.split(',');
		QListWidgetItem *currentTimeItem = new QListWidgetItem(ui->ImgListWidget);
		for (const QString& imgPath : contentLinesList) {
			QListWidgetItem *item = new QListWidgetItem(ui->ImgListWidget);
			QPixmap pixmap(imgPath);
			item->setIcon(QIcon(pixmap));
			ui->ImgListWidget->addItem(item);
		}
		currentTimeItem->setText(contentLinesList.at(0));
		QString CurrentTextColor = settings.value("CurrentTextColor").toString();
		QPalette ImgPalette = ui->ImgListWidget->palette();
		ImgPalette.setColor(QPalette::Text, CurrentTextColor);
		ui->ImgListWidget->setPalette(ImgPalette);

		ui->ImgListWidget->addItem(currentTimeItem);
	}
	else if (hisConType == "DOC")
	{
		ui->stackedWidget_2->setCurrentWidget(ui->DocumentPage);
		// 释放之前分配的内存
		QStringListModel* model = static_cast<QStringListModel*>(ui->DoclistView->model());
		if (model) delete model;
		DocPaths.clear();
		contentLinesList = contentLines.split(',');
		for (const QString& docPath : contentLinesList) DocPaths.append(docPath);
		ui->DoclistView->setModel(new QStringListModel(DocPaths, this));
	}
	// 文本匹配
	QTextDocument *doc = ui->textBrowser->document();
	QTextCursor cursor = doc->find(ui->SearchEdit->text());

	while (!cursor.isNull()) {
		QTextCharFormat format;
		format.setBackground(Qt::darkCyan);
		cursor.mergeCharFormat(format);
		cursor = doc->find(ui->SearchEdit->text(), cursor.position() + ui->SearchEdit->text().length());
	}

	file.close();
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
		case Clipboard::TXT:
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
	else QMessageBox::warning(this, "error!", "不能保存历史记录，请检查文件!"_toUnicode, QMessageBox::Ok);
	
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
	/* ctrl + F 文本匹配 */
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
	/* ctrl + Z 返回上一次记录 */
	if (event->modifiers() == Qt::ControlModifier &&
		event->key() == Qt::Key_Z)
	{
		Turn2History(LEFT);
	}
	/* ctrl + Y 返回下一次记录 */
	if (event->modifiers() == Qt::ControlModifier &&
		event->key() == Qt::Key_Y)
	{
		Turn2History(RIGHT);
	}
	/* Esc 隐藏窗口 */
	if (event->key() == Qt::Key_Escape) 
	{
		setVisible(false);
	}
	QWidget::keyPressEvent(event);

}