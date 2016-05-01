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
    parser.addPositionalArgument("INPUT-DIRECTORY",
                                 "Path to scan for pictures.");

    QCommandLineOption outputDirectoryOption(
        QStringList() << "o"
                      << "output-directory",
        "Write images to <directory>.", "directory");
    parser.addOption(outputDirectoryOption);

    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if (args.size() != 1) {
        qFatal("Need exactly 1 input directory");
    }
    app.setInputDir(args[0]);
    app.scan();

    QString outputDir = parser.value(outputDirectoryOption);
    if (outputDir != QString())
        app.setOutputDir(outputDir);

    QTimer::singleShot(0, &app, SLOT(onEventLoopStarted()));
    try {
        return app.exec();
    } catch (std::exception e) {
        qFatal(e.what());
    }
}
