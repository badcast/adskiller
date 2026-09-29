#include <QString>
#include <QMessageBox>

#include "MainWindow.h"
#include "Services.h"

std::shared_ptr<Service> _CurrentService = nullptr;

bool ServiceProvider::runService(std::shared_ptr<Service> service)
{
    if(service == nullptr || !service->active || service->isStarted())
    {
        return false;
    }

    if(!service->isOnlineService())
    {
        service->sendCheckPull();
    }

    PageIndex _preloadPage;
    _CurrentService = std::move(service);
    _CurrentService->stop();

    if((_CurrentService->deviceConnectType() == DeviceConnectType::ADB || _CurrentService->deviceConnectType() == DeviceConnectType::Apple) && _CurrentService->uuid() != IDServiceAppleIpswString)
    {
        MainWindow::current->connectPhone = {};
        MainWindow::current->connectPhone.connectionType = _CurrentService->deviceConnectType();
        _preloadPage = DevicesPage;
    }
    else
    {
        _preloadPage = _CurrentService->targetPage();
    }

    MainWindow::current->showPageLoader(
        _preloadPage,
        1500,
        []()
        {
            _CurrentService->start();
            return true;
        },
        QString("Запуск службы\n\"%1\"").arg(_CurrentService->title));
    return true;
}

void ServiceProvider::closeService()
{
    if(_CurrentService)
    {
        currentService()->stop();
        _CurrentService.reset();
    }
}

std::shared_ptr<Service> ServiceProvider::currentService()
{
    return _CurrentService;
}
