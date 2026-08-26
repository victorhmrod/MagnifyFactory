#include "PdfImageWriter.h"

#include <QFile>
#include <QImageReader>

namespace magnify::engines::pdf {

bool PdfImageWriter::writeSingleImagePdf(const QString &jpegPath, const QString &outputPdfPath,
                                          QString *errorMessage) {
    QFile jpegFile(jpegPath);
    if (!jpegFile.open(QIODevice::ReadOnly)) {
        if (errorMessage) *errorMessage = QStringLiteral("Could not open source image: %1").arg(jpegPath);
        return false;
    }
    const QByteArray jpegBytes = jpegFile.readAll();
    jpegFile.close();

    QImageReader reader(jpegPath);
    const QSize size = reader.size();
    if (!size.isValid() || jpegBytes.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("Could not read image dimensions: %1").arg(jpegPath);
        return false;
    }
    const int width = size.width();
    const int height = size.height();
    const bool grayscale = reader.imageFormat() == QImage::Format_Grayscale8;
    const QByteArray colorSpace = grayscale ? "/DeviceGray" : "/DeviceRGB";

    const QByteArray contentStream =
        QByteArray("q\n") + QByteArray::number(width) + " 0 0 " + QByteArray::number(height) + " 0 0 cm\n/Im0 Do\nQ";

    // Build each object as raw bytes so we can compute exact byte offsets for
    // the xref table — required for a PDF file to be structurally valid.
    QList<QByteArray> objects;
    objects << "<< /Type /Catalog /Pages 2 0 R >>";
    objects << "<< /Type /Pages /Kids [3 0 R] /Count 1 >>";
    objects << QByteArray("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 ") + QByteArray::number(width) + " " +
                   QByteArray::number(height) + "] /Resources << /XObject << /Im0 4 0 R >> >> /Contents 5 0 R >>";
    objects << QByteArray("<< /Type /XObject /Subtype /Image /Width ") + QByteArray::number(width) +
                   " /Height " + QByteArray::number(height) + " /ColorSpace " + colorSpace +
                   " /BitsPerComponent 8 /Filter /DCTDecode /Length " + QByteArray::number(jpegBytes.size()) +
                   " >>\nstream\n" + jpegBytes + "\nendstream";
    objects << QByteArray("<< /Length ") + QByteArray::number(contentStream.size()) + " >>\nstream\n" +
                   contentStream + "\nendstream";

    QFile out(outputPdfPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage) *errorMessage = QStringLiteral("Could not open output file for writing: %1").arg(outputPdfPath);
        return false;
    }

    QByteArray buffer;
    buffer += "%PDF-1.4\n";

    QList<qint64> offsets;
    for (int i = 0; i < objects.size(); ++i) {
        offsets << buffer.size();
        buffer += QByteArray::number(i + 1) + " 0 obj\n" + objects[i] + "\nendobj\n";
    }

    const qint64 xrefOffset = buffer.size();
    buffer += "xref\n";
    buffer += QByteArray("0 ") + QByteArray::number(objects.size() + 1) + "\n";
    buffer += "0000000000 65535 f \n";
    for (qint64 offset : offsets) {
        buffer += QByteArray::number(offset).rightJustified(10, '0') + " 00000 n \n";
    }
    buffer += QByteArray("trailer\n<< /Size ") + QByteArray::number(objects.size() + 1) + " /Root 1 0 R >>\n";
    buffer += "startxref\n" + QByteArray::number(xrefOffset) + "\n%%EOF";

    out.write(buffer);
    out.close();
    return true;
}

} // namespace magnify::engines::pdf
