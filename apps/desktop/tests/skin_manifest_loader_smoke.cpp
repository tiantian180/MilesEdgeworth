#include "pet/manifest/SkinManifestLoader.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

#include <cstdlib>
#include <iostream>

namespace {
bool writeFile(const QString &path, const QString &content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream stream(&file);
    stream << content;
    return true;
}

void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << "\n";
        std::exit(1);
    }
}
} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    const QUrl rootUrl(QStringLiteral("file:///tmp/example-skin/"));
    require(
        SkinManifestLoader::resolveSkinUrl(QStringLiteral("skin:assets/body/idle.gif"), rootUrl).toString()
            == QStringLiteral("file:///tmp/example-skin/assets/body/idle.gif"),
        "skin: URL should resolve under file root"
    );
    require(
        SkinManifestLoader::resolveSkinUrl(QStringLiteral("qrc:/pet/stand-right.gif"), rootUrl).toString()
            == QStringLiteral("qrc:/pet/stand-right.gif"),
        "absolute qrc URL should stay unchanged"
    );
    require(
        !SkinManifestLoader::resolveSkinUrl(QStringLiteral("skin:../escape.gif"), rootUrl).isValid(),
        "skin: URL must reject parent traversal"
    );

    QTemporaryDir dir;
    require(dir.isValid(), "temporary skin directory should be valid");
    QDir skinDir(dir.path());
    require(skinDir.mkpath(QStringLiteral("assets/body/idle")), "assets directory should be created");
    require(writeFile(skinDir.filePath(QStringLiteral("skin.json")), QStringLiteral(R"JSON(
{
  "id": "test-skin",
  "name": "测试皮肤",
  "version": "1.0.0",
  "manifestVersion": 1,
  "thumbnail": "skin:assets/body/idle/stand.gif"
}
)JSON")), "skin.json should be written");
    require(writeFile(skinDir.filePath(QStringLiteral("manifest.json")), QStringLiteral(R"JSON(
{
  "defaultFacing": "right",
  "states": { "idle": { "action": "idle_stand" } },
  "actions": {
    "idle_stand": {
      "variants": {
        "right": {
          "animation": "skin:assets/body/idle/stand.gif"
        }
      }
    }
  }
}
)JSON")), "manifest.json should be written");

    SkinManifest manifest = SkinManifestLoader::loadFromDirectory(dir.path());
    require(manifest.skinId == QStringLiteral("test-skin"), "loadFromDirectory should populate skinId");
    require(manifest.skinName == QStringLiteral("测试皮肤"), "loadFromDirectory should populate skinName");
    require(!manifest.builtin, "filesystem skin should not be builtin");
    require(manifest.actions.contains(QStringLiteral("idle_stand")), "filesystem manifest should load actions");
    const QString expectedAnimationUrl = QUrl::fromLocalFile(
        skinDir.filePath(QStringLiteral("assets/body/idle/stand.gif"))
    ).toString();
    require(
        manifest.actions.value(QStringLiteral("idle_stand")).variants.value(QStringLiteral("right")).toString() == expectedAnimationUrl,
        "skin: action URL should resolve under the selected skin root"
    );

    QList<SkinDescriptor> descriptors = SkinManifestLoader::discoverInDirectories(QStringList{dir.path()}, false);
    require(descriptors.size() == 1, "discoverInDirectories should find one test skin");
    require(descriptors.first().id == QStringLiteral("test-skin"), "descriptor id should come from skin.json");
    require(descriptors.first().rootUrl.isLocalFile(), "filesystem descriptor root should be a file URL");

    return 0;
}
