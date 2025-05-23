#include <QtWidgets/QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    app.setApplicationName("CFPing");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("CFPing");
    
    MainWindow window;
    window.show();
    
    return app.exec();
}
