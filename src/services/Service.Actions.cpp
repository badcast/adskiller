#include <QString>
#include <QMessageBox>

#include "mainwindow.h"
#include "Services.h"

std::shared_ptr<Service> _CurrentService = nullptr;

bool ServiceProvider::runService(std::shared_ptr<Service> service)
{
    closeService();
    if(service == nullptr || !service->active || service->isStarted())
    {
        return false;
    }

    PageIndex _preloadPage;
    _CurrentService = std::move(service);
    _CurrentService->reset();

    if(_CurrentService->deviceConnectType() == DeviceConnectType::ADB)
    {
        MainWindow::current->connectPhone = {};
        MainWindow::current->connectPhone.connectionType = _CurrentService->deviceConnectType();
        _preloadPage = DevicesPage;
    }
    else
    {
        _preloadPage = _CurrentService->targetPage();
    }
    MainWindow::current->showPageLoader(_preloadPage, 2000, [_CurrentService](){
        _CurrentService->start();
        return false;
    }, QString("Запуск службы\n\"%1\"").arg(_CurrentService->title));
    return true;
}

void ServiceProvider::closeService()
{
    if(_CurrentService)
    {
        _CurrentService->stop();
        _CurrentService->reset();
        _CurrentService.reset();
    }
}

std::shared_ptr<Service> ServiceProvider::currentService()
{
    return _CurrentService;
}
