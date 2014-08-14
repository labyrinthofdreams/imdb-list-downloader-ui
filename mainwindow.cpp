#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include <QByteArray>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QRegExp>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QTableWidgetItem>
#include <QTextStream>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkCookie>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QUrl>
#include <QVariant>
#include "mainwindow.hpp"
#include "sfcsv/sfcsv.h"
#include "ui_mainwindow.h"

static QString parseImdbId(const QString& text) {
    static const QRegExp imdbIdRx("ur([0-9]{7,8})");

    if(imdbIdRx.indexIn(text) == -1) {
        return {};
    }

    return imdbIdRx.cap(1);
}

static QString parseListId(const QString& text) {
    static const QRegExp listIdRx("ls([0-9]{9})");

    if(listIdRx.indexIn(text) == -1) {
        return {};
    }

    return listIdRx.cap(1);
}

static QStringList parseCsv(const QString& in) {
    QStringList result;
    std::vector<std::string> parsed;
    const std::string line = in.toStdString();
    sfcsv::parse_line(line, std::back_inserter(parsed), ',');
    for(auto&& res : parsed) {
        result.append(QString::fromStdString(res));
    }

    return result;
}

static QList<QNetworkCookie> parseCookies(const QString& cookies) {
    QList<QNetworkCookie> result;

    const QStringList pairs = cookies.split("; ");
    if(pairs.size() == 1) {
        return {};
    }

    for(const QString& pair : pairs) {
        const QStringList kv = pair.split("=");
        if(kv.size() == 1) {
            return {};
        }

        QByteArray key, value;
        key.append(kv.at(0));
        value.append(kv.at(1));

        const QNetworkCookie cookie(key, value);
        result.append(cookie);
    }

    return result;
}

static QStringList parseFile(const QString& in) {
    QStringList results;
    QFile inFile(in);
    if(!inFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        throw std::runtime_error("Unable to open file");
    }

    QTextStream stream(&inFile);
    stream.setCodec("UTF-8");
    while(!stream.atEnd()) {
        const QString line = stream.readLine().remove("\n");
        results << line;
    }

    return results;
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    netManager(new QNetworkAccessManager),
    config("IMDbDownloaderConfig.ini", QSettings::IniFormat)
{
    ui->setupUi(this);

    connect(netManager.get(),   SIGNAL(finished(QNetworkReply *)),
            this,               SLOT(networkReplyFinished(QNetworkReply*)));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_actionOpen_triggered()
{
    const QString openedFile = QFileDialog::getOpenFileName(this, tr("Open..."),
                                                            config.value("last_csv").toString());
    if(openedFile.isEmpty()) {
        return;
    }

    config.setValue("last_csv", openedFile);

    const QStringList lines = parseFile(openedFile);
    const QString header = lines.first();
    const QStringList headerParsed = parseCsv(header);

    ui->plainTextLog->clear();
    ui->tableCsv->clear();
    ui->tableCsv->setColumnCount(headerParsed.size() + 1);
    ui->tableCsv->setHorizontalHeaderLabels(QStringList() << "List Name" << "URL" << "Status");
    ui->tableCsv->setRowCount(lines.size());

    for(int row = 0; row < lines.size(); ++row) {
        const QStringList columns = parseCsv(lines.at(row));
        const QString username = columns.at(0);
        const QString url = columns.at(1);

        ui->tableCsv->setItem(row, 0, new QTableWidgetItem(username));
        ui->tableCsv->setItem(row, 1, new QTableWidgetItem(url));
        ui->tableCsv->setItem(row, 2, new QTableWidgetItem("Waiting"));


        const QString urlId = parseUrlId(url);
        QString requestUrl;
        if(ui->radioRatings->isChecked()) {
            // IMDb ratings
            requestUrl = QString("http://www.imdb.com/list/export?list_id=ratings&author_id=ur%1").arg(urlId);
        }
        else {
            // IMDb lists
            requestUrl = QString("http://www.imdb.com/list/export?list_id=ls%1&author_id=ur27588704").arg(urlId);
        }

        if(!requestUrl.isEmpty()) {
            requests.append(QNetworkRequest(requestUrl));
        }

        qDebug() << "Added" << username << url << requestUrl;

    }

    ui->plainTextLog->appendPlainText(tr("Enqueued %1 URLs").arg(requests.size()));
}

void MainWindow::networkReplyFinished(QNetworkReply *reply)
{
    // Remove highlight from current
    const int row = findItemRow(parseUrlId(reply->url().toDisplayString()));

    if(reply->error()) {
        setRowColor(row, QColor(Qt::red));

        setRowStatus(row, tr("Failed. Retrying..."));

        // Enqueue the request
        requests.append(QNetworkRequest(reply->url()));

        // Start another request
        nextRequest();
        return;
    }

    setRowStatus(row, tr("Saved"));
    setRowColor(row, QColor(Qt::green));

    const QString url = reply->url().toDisplayString();
    const QString data = reply->readAll(); 

    // Find who this imdb id belongs to
    const QString imdbId = parseUrlId(url);
    if(row != -1) {
        const QString username = ui->tableCsv->item(row, 0)->text().trimmed();
        QFile out(QString("%1/%2.csv").arg(config.value("save_to").toString()).arg(username));
        out.open(QIODevice::WriteOnly | QIODevice::Text);

        QTextStream stream(&out);
        stream.setCodec("UTF-8");
        stream << data;
    }

    reply->deleteLater();

    // Start another request
    nextRequest();
}

void MainWindow::on_buttonDownload_clicked()
{
    const QString savePath = QFileDialog::getExistingDirectory(this, tr("Save to..."),
                                                               config.value("save_to").toString());
    if(savePath.isEmpty()) {
        return;
    }

    config.setValue("save_to", savePath);

    for(int i = 0, last = ui->spinBoxSimReqs->value(); i < last; ++i) {
        nextRequest();
    }
}

void MainWindow::on_buttonLoadCookies_clicked()
{
    const QString openedFile =
            QFileDialog::getOpenFileName(this, tr("Cookies"),
                                         config.value("last_cookies").toString());
    if(openedFile.isEmpty()) {
        return;
    }

    config.setValue("last_cookies", openedFile);

    QFile in(openedFile);
    if(!in.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ui->plainTextLog->appendPlainText(tr("Failed to open cookies file. Insufficient permissions?"));
        return;
    }

    const QString cookies = in.readAll();
    netCookies = parseCookies(cookies);
    if(netCookies.empty()) {
        ui->plainTextLog->appendPlainText(tr("Invalid cookie format"));

        return;
    }

    ui->plainTextLog->appendPlainText(tr("Loaded cookies."));
}

void MainWindow::nextRequest()
{
    if(requests.empty()) {
        return;
    }

    QVariant cookieVar;
    cookieVar.setValue(netCookies);

    // Makes sure files are only overwritten if explicitly specified
    // or to request files that do not exist
    while(!requests.empty()) {
        QNetworkRequest req = requests.takeFirst();
        req.setHeader(QNetworkRequest::CookieHeader, cookieVar);

        const QUrl url = req.url();
        const int row = findItemRow(parseUrlId(url.toDisplayString()));

        const QString username = ui->tableCsv->item(row, 0)->text();
        const QString savePath = QString("%1/%2.csv").arg(config.value("save_to").toString()).arg(username);
        if(ui->checkBoxOverwrite->isChecked() || !QFile::exists(savePath)) {
            // Highlight current
            setRowColor(row, QColor(Qt::yellow));
            setRowStatus(row, tr("Downloading"));

            netManager->get(req);

            break;
        }
        else {
            setRowStatus(row, tr("Skipped"));
        }
    }
}

int MainWindow::findItemRow(const QString& imdbId) const
{
    int foundRow = -1;
    const auto users = ui->tableCsv->rowCount();
    for(int row = 0; row < users; ++row) {
        auto current = ui->tableCsv->item(row, 1);
        const QString imdbUrl = current->text();

        const QString userImdbId = parseUrlId(imdbUrl);
        if(imdbId == userImdbId) {
            foundRow = ui->tableCsv->row(current);
            break;
        }
    }

    return foundRow;
}

QString MainWindow::parseUrlId(const QString& url) const
{
    QString id;
    if(ui->radioRatings->isChecked()) {
        id = parseImdbId(url);
    }
    else {
        id = parseListId(url);
    }

    return id;
}

void MainWindow::setRowStatus(const int row, const QString& status)
{
    ui->tableCsv->item(row, 2)->setText(status);
}

void MainWindow::setRowColor(const int row, const QColor& color)
{
    const QBrush brush(color);
    for(int column = 0, last = ui->tableCsv->columnCount(); column < last; ++column) {
        ui->tableCsv->item(row, column)->setBackground(brush);
    }
}
