#include "Services.h"
#include "mainwindow.h"
#include "Identity.h"

// Mock Network
Network::Network(const Network &other) : QObject(), manager(nullptr) {}
Network::~Network() {}
void Network::pullServiceUUID(const QString &uuid, const QJsonObject &request, ServiceOperation so) {}
void Network::pullFetchVersion(bool populate) {}
bool Network::checkNet() const { return true; }
void Network::setNetworkTimeout(int value) {}
bool Network::isAuthed() const { return true; }
bool Network::pending() const { return false; }
void Network::onAuthJWTFinished() {}
void Network::onFetchServices() {}
void Network::onPullServiceList() {}
void Network::onPullServiceUUID() {}
void Network::onFetchingVersion() {}
void Network::authJWT(const QString &login, const QString &password) {}
void Network::authJWT(const QString &token) {}
void Network::fetchServices() {}
void Network::pullServiceList() {}

// MOC for Network (since it inherits QObject and we didn't run AUTOMOC on a mock header)
// Actually, it's easier to just include moc_network.cpp if it was generated, but it's generated for the real network.h.
// Since network.h has Q_OBJECT, the linker expects staticMetaObject.
// We can just define a minimal staticMetaObject if we must, or better yet, since network.h is in AUTOMOC, CMake already generated mocs_compilation.cpp for ServiceTests!
// Yes, CMake generated `mocs_compilation.cpp.o` which includes moc_network.cpp because we included `network.h`?
// Wait, `network.h` is NOT in the `ServiceTests` sources. So AUTOMOC did NOT generate it.
// I should add `src/network.cpp` to the test executable? No, `network.cpp` requires `QNetworkAccessManager` and other things, but it's self-contained!
