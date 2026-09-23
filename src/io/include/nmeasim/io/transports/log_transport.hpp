#pragma once

#include <nmeasim/io/transport.hpp>

#include <QFile>
#include <QString>

namespace nmeasim::io {

/// Records sentences to a log file in the format of ADR 0012: a `#` header when the file is
/// new, then one line per sentence prefixed with the wall-clock UTC time. Flushed after
/// every line so the file can be tailed.
class LogTransport final : public Transport {
    Q_OBJECT

public:
    /// Records to `path`. With `append` false the first open truncates the file; later opens
    /// continue it.
    explicit LogTransport(QString path, bool append = true, QObject* parent = nullptr);
    ~LogTransport() override;

    [[nodiscard]] QString description() const override;
    bool open() override;
    void close() override;
    void write(const QByteArray& line) override;

    /// The log file written to.
    [[nodiscard]] QString path() const { return file_.fileName(); }
    /// Name written to the `# profile:` header line; empty omits the line.
    void set_profile_name(const QString& name) { profile_name_ = name; }
    /// Sentences recorded since construction, header lines excluded.
    [[nodiscard]] qint64 lines_written() const noexcept { return lines_written_; }

private:
    QFile file_;
    bool append_;
    QString profile_name_;
    qint64 lines_written_{0};
};

}  // namespace nmeasim::io
