#pragma once

#include <nmeasim/io/transport.hpp>

#include <QFile>
#include <QString>

namespace nmeasim::io {

/// Appends lines to a file, flushing after every write so that the file can be tailed.
class FileTransport final : public Transport {
    Q_OBJECT

public:
    /// Writes to `path`, appending to an existing file or, with `append` false, truncating it
    /// when opened.
    explicit FileTransport(QString path, bool append = true, QObject* parent = nullptr);
    ~FileTransport() override;

    [[nodiscard]] QString description() const override;
    bool open() override;
    void close() override;
    void write(const QByteArray& line) override;

    /// The file written to.
    [[nodiscard]] QString path() const { return file_.fileName(); }

private:
    QFile file_;
    bool append_;
};

}  // namespace nmeasim::io
