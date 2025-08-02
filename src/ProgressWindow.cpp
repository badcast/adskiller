#include "mainwindow.h"

void MainWindow::showPageLoader(int pageNum, int msWait)
{
    if(pageNum == LoaderPage)
        return;

    showPage(LoaderPage);

    delayPush(msWait, [pageNum]()
    {
        showPage(pageNum);
    });
}
