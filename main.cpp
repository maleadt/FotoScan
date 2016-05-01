#include <QCommandLineParser>
#include <QTimer>

#include "scanner.hpp"

int main(int argc, char *argv[]) {
    Scanner app(argc, argv);
    QGuiApplication::setApplicationDisplayName("Foto Scanner");
    QGuiApplication::setApplicationVersion("0.1");

    QCommandLineParser parser;
    parser.setApplicationDescription("Foto Scanner");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("[PATH...]", "Path to scan for pictures.");

    parser.process(app);

    const QStringList args = parser.positionalArguments();
    for (auto arg : args)
        app.scan(arg);

    QTimer::singleShot(0, &app, SLOT(onEventLoopStarted()));
    try {
        return app.exec();
    } catch (std::exception e) {
        qFatal(e.what());
    }
}
