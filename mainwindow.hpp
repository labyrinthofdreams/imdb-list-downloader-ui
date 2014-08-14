#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <memory>
#include <QList>
#include <QMainWindow>
#include <QRegExp>
#include <QSettings>

class QColor;
class QNetworkAccessManager;
class QNetworkCookie;
class QNetworkReply;
class QNetworkRequest;
class QString;
class QUrl;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_actionOpen_triggered();

    void networkReplyFinished(QNetworkReply *);

    void on_buttonDownload_clicked();

    void on_buttonLoadCookies_clicked();

private:
    Ui::MainWindow *ui;

    //! Handles network requests and replies
    std::unique_ptr<QNetworkAccessManager> netManager;

    //! Cookies to be sent to server
    QList<QNetworkCookie> netCookies;

    //! Requests to process
    QList<QNetworkRequest> requests;

    QSettings config;

    void nextRequest();

    int findItemRow(const QString& imdbId) const;

    void setRowColor(int row, const QColor& color);

    void setRowStatus(int row, const QString& status);

    QString parseUrlId(const QString& url) const;
};

#endif // MAINWINDOW_HPP
