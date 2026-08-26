#include <QTest>

#include "engines/ffmpeg/FFmpegCommandBuilder.h"

using magnify::engines::ffmpeg::FFmpegCommandBuilder;

class TestFFmpegCommandBuilder : public QObject {
    Q_OBJECT
private slots:
    void audioExtractionProducesExpectedArgs() {
        FFmpegCommandBuilder builder;
        const QStringList args = builder.setInput("input.mp4")
                                      .setOutput("output.mp3")
                                      .dropVideoStream()
                                      .setAudioCodec("libmp3lame")
                                      .setAudioQuality(2)
                                      .build();

        QVERIFY(args.contains("-i"));
        QVERIFY(args.contains("input.mp4"));
        QVERIFY(args.contains("-vn"));
        QVERIFY(args.contains("libmp3lame"));
        QCOMPARE(args.last(), QStringLiteral("output.mp3"));
    }

    void streamCopyProducesCopyCodecs() {
        FFmpegCommandBuilder builder;
        const QStringList args =
            builder.setInput("input.mkv").setOutput("output.mp4").copyVideoStream().copyAudioStream().build();

        const int videoCodecIndex = args.indexOf("-c:v");
        const int audioCodecIndex = args.indexOf("-c:a");
        QVERIFY(videoCodecIndex >= 0);
        QVERIFY(audioCodecIndex >= 0);
        QCOMPARE(args.at(videoCodecIndex + 1), QStringLiteral("copy"));
        QCOMPARE(args.at(audioCodecIndex + 1), QStringLiteral("copy"));
    }

    void neverProducesAShellString() {
        // The builder must always yield a QStringList (argv-style), so a
        // value containing spaces or shell metacharacters is passed through
        // as a single, unsplit argument to QProcess — never concatenated.
        FFmpegCommandBuilder builder;
        const QStringList args =
            builder.setInput("my video; rm -rf ~.mp4").setOutput("out.mp4").copyVideoStream().build();
        QVERIFY(args.contains("my video; rm -rf ~.mp4"));
    }
};

QTEST_APPLESS_MAIN(TestFFmpegCommandBuilder)
#include "test_ffmpeg_command_builder.moc"
