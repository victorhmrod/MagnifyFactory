#pragma once

#include <QString>

namespace magnify::engines::pdf {

// Writes a minimal, single-page, valid PDF that embeds a JPEG image as-is
// (DCTDecode passthrough — no re-encoding, no external PDF library needed).
// This is the same technique tools like img2pdf use for lossless image->PDF.
class PdfImageWriter {
public:
    static bool writeSingleImagePdf(const QString &jpegPath, const QString &outputPdfPath, QString *errorMessage);
};

} // namespace magnify::engines::pdf
