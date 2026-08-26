#include <QTest>

#include "core/FormatRegistry.h"

using magnify::core::FormatCategory;
using magnify::core::FormatRegistry;

class TestFormatRegistry : public QObject {
    Q_OBJECT
private slots:
    void knowsBuiltinVideoFormats() {
        QCOMPARE(FormatRegistry::instance().categoryOf("mp4"), FormatCategory::Video);
        QCOMPARE(FormatRegistry::instance().categoryOf("mkv"), FormatCategory::Video);
    }

    void knowsBuiltinAudioFormats() {
        QCOMPARE(FormatRegistry::instance().categoryOf("mp3"), FormatCategory::Audio);
        QCOMPARE(FormatRegistry::instance().categoryOf("wav"), FormatCategory::Audio);
    }

    void unknownExtensionReturnsUnknownCategory() {
        QCOMPARE(FormatRegistry::instance().categoryOf("xyz123"), FormatCategory::Unknown);
    }

    void lookupIsCaseInsensitive() {
        const auto *descriptor = FormatRegistry::instance().findByExtension("MP4");
        QVERIFY(descriptor != nullptr);
        QCOMPARE(descriptor->extension, QStringLiteral("mp4"));
    }
};

QTEST_APPLESS_MAIN(TestFormatRegistry)
#include "test_format_registry.moc"
