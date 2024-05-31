#include "clipboard.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Clipboard w;
    w.show();
    return a.exec();
}
