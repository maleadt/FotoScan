#include <QCommandLineParser>
#include <QTimer>

#include <QMessageBox>

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
                                 "Path to scan for images.");

    QCommandLineOption outputDirectoryOption(
        QStringList() << "o"
                      << "output-directory",
        "Write photos to <directory>.", "directory");
    parser.addOption(outputDirectoryOption);

    QCommandLineOption correctOption(QStringList() << "c"
                                                   << "correct",
                                     "Correct most recent results");
    parser.addOption(correctOption);

    parser.process(app);

    bool correct = parser.isSet(correctOption);
    if (correct)
        app.setMode(ProgramMode::CORRECT_RESULTS);

    const QStringList args = parser.positionalArguments();
    if (args.size() != 1) {
        QMessageBox::critical(nullptr, "Invalid usage",
                              QString("Try `%0 --help` for more information.")
                              .arg(QFileInfo(QCoreApplication::applicationFilePath()).fileName()));
        return 1;
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
