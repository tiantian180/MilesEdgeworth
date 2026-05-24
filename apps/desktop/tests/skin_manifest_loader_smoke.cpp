#include "pet/manifest/PersonaStore.h"
#include "pet/manifest/SkinManifestLoader.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
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
    QStandardPaths::setTestModeEnabled(true);

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
  "clips": {
    "thinking_enter_right": {
      "file": "skin:assets/body/idle/stand.gif",
      "frameRange": [1, 4]
    }
  },
  "actions": {
    "idle_stand": {
      "variants": {
        "right": {
          "animation": "skin:assets/body/idle/stand.gif"
        }
      }
    },
    "objecting": {
      "loopMode": "onceThenHold",
      "variants": {
        "right": {
          "clip": "thinking_enter_right"
        }
      }
    }
  }
}
)JSON")), "manifest.json should be written");
    const QString filesystemPersona = QStringLiteral("Filesystem persona content\n第二行\n");
    require(writeFile(skinDir.filePath(QStringLiteral("persona.md")), filesystemPersona), "persona.md should be written");

    SkinManifest manifest = SkinManifestLoader::loadFromDirectory(dir.path());
    require(manifest.skinId == QStringLiteral("test-skin"), "loadFromDirectory should populate skinId");
    require(manifest.skinName == QStringLiteral("测试皮肤"), "loadFromDirectory should populate skinName");
    require(!manifest.builtin, "filesystem skin should not be builtin");
    require(manifest.actions.contains(QStringLiteral("idle_stand")), "filesystem manifest should load actions");
    const QString expectedAnimationUrl = QUrl::fromLocalFile(
        skinDir.filePath(QStringLiteral("assets/body/idle/stand.gif"))
    ).toString();
    require(
        manifest.actions.value(QStringLiteral("idle_stand")).variants.value(QStringLiteral("right")).url.toString() == expectedAnimationUrl,
        "skin: action URL should resolve under the selected skin root"
    );
    const ActionDefinition objecting = manifest.actions.value(QStringLiteral("objecting"));
    require(objecting.loopMode == QStringLiteral("onceThenHold"),
            "loader should preserve onceThenHold loopMode");
    const AnimationVariant objectingVariant = objecting.variants.value(QStringLiteral("right"));
    require(objectingVariant.url.toString() == expectedAnimationUrl,
            "clip variant should resolve to the clip file URL");
    require(objectingVariant.frameStart == 0 && objectingVariant.frameEnd == 3,
            "loader should convert 1-based manifest frameRange to 0-based inclusive runtime frame range");
    require(manifest.personaPrompt == filesystemPersona, "filesystem skin persona.md should load into manifest");

    QTemporaryDir personaDataDir;
    require(personaDataDir.isValid(), "persona override data dir should be valid");
    qputenv("MILES_DATA_DIR", personaDataDir.path().toUtf8());

    SkinManifest builtInMiles = SkinManifestLoader::loadFromResource(QStringLiteral(":/skins/miles-edgeworth/manifest.json"));
    require(
        builtInMiles.personaPrompt.contains(QStringLiteral("Miles Edgeworth"))
            || builtInMiles.personaPrompt.contains(QStringLiteral("御剑怜侍")),
        "built-in Miles persona should load from qrc"
    );

    QString personaError;
    const QString overridePersona = QStringLiteral("Override persona for miles-edgeworth\n");
    require(PersonaStore::writeForManifest(builtInMiles, overridePersona, &personaError), "persona override should save");
    builtInMiles = SkinManifestLoader::loadFromResource(QStringLiteral(":/skins/miles-edgeworth/manifest.json"));
    require(builtInMiles.personaPrompt == overridePersona, "persona override should win over built-in qrc persona");

    const QStringList invalidSkinIds = {
        QString(),
        QStringLiteral("bad/skin"),
        QStringLiteral("bad\\skin"),
        QStringLiteral("../escape"),
        QStringLiteral("bad..skin"),
    };
    for (const QString &invalidSkinId : invalidSkinIds) {
        SkinDescriptor invalidDescriptor;
        invalidDescriptor.id = invalidSkinId;
        invalidDescriptor.rootUrl = QUrl::fromLocalFile(skinDir.absolutePath() + QLatin1Char('/'));
        require(
            PersonaStore::readForDescriptor(invalidDescriptor).isEmpty(),
            "invalid skin id must not read persona from override or skin root"
        );

        SkinManifest invalidManifest;
        invalidManifest.skinId = invalidSkinId;
        invalidManifest.skinRootUrl = QUrl::fromLocalFile(skinDir.absolutePath() + QLatin1Char('/'));
        invalidManifest.builtin = true;
        personaError.clear();
        require(
            !PersonaStore::writeForManifest(invalidManifest, QStringLiteral("bad"), &personaError),
            "invalid skin id must not write persona override"
        );
        require(!personaError.isEmpty(), "invalid skin id write should report an error");
    }
    require(
        !QFileInfo(personaDataDir.path() + QStringLiteral("/escape.md")).exists(),
        "invalid skin id must not escape persona override directory"
    );

    qunsetenv("MILES_DATA_DIR");

    QList<SkinDescriptor> descriptors = SkinManifestLoader::discoverInDirectories(QStringList{dir.path()}, false);
    require(descriptors.size() == 1, "discoverInDirectories should find one test skin");
    require(descriptors.first().id == QStringLiteral("test-skin"), "descriptor id should come from skin.json");
    require(descriptors.first().rootUrl.isLocalFile(), "filesystem descriptor root should be a file URL");

    QTemporaryDir userDir;
    QTemporaryDir portableDir;
    require(userDir.isValid() && portableDir.isValid(), "precedence dirs should be valid");
    QDir userSkin(userDir.path() + QStringLiteral("/miles-edgeworth"));
    QDir portableSkin(portableDir.path() + QStringLiteral("/miles-edgeworth"));
    require(userSkin.mkpath(QStringLiteral(".")), "user skin dir should be created");
    require(portableSkin.mkpath(QStringLiteral(".")), "portable skin dir should be created");
    require(writeFile(userSkin.filePath(QStringLiteral("skin.json")), QStringLiteral(R"JSON(
{"id":"same-id","name":"用户版本","version":"1.0.0","manifestVersion":1}
)JSON")), "user skin.json should be written");
    require(writeFile(portableSkin.filePath(QStringLiteral("skin.json")), QStringLiteral(R"JSON(
{"id":"same-id","name":"便携版本","version":"1.0.0","manifestVersion":1}
)JSON")), "portable skin.json should be written");
    require(writeFile(userSkin.filePath(QStringLiteral("manifest.json")), QStringLiteral("{}")), "user manifest should be written");
    require(writeFile(portableSkin.filePath(QStringLiteral("manifest.json")), QStringLiteral("{}")), "portable manifest should be written");

    QList<SkinDescriptor> precedence = SkinManifestLoader::discoverInDirectories(
        QStringList{userDir.path(), portableDir.path()},
        false
    );
    require(precedence.size() == 1, "same id should be deduplicated");
    require(precedence.first().name == QStringLiteral("用户版本"), "earlier directory should win on duplicate skin id");

    QTemporaryDir incompleteOverrideDir;
    require(incompleteOverrideDir.isValid(), "incomplete override dir should be valid");
    QDir incompleteMiles(incompleteOverrideDir.path() + QStringLiteral("/miles-edgeworth"));
    require(incompleteMiles.mkpath(QStringLiteral(".")), "incomplete miles override dir should be created");
    require(writeFile(incompleteMiles.filePath(QStringLiteral("skin.json")), QStringLiteral(R"JSON(
{"id":"miles-edgeworth","name":"残缺用户版本","version":"1.0.0","manifestVersion":1}
)JSON")), "incomplete miles skin.json should be written");

    QList<SkinDescriptor> withBuiltinFallback = SkinManifestLoader::discoverInDirectories(
        QStringList{incompleteOverrideDir.path()},
        true
    );
    require(!withBuiltinFallback.isEmpty(), "built-in skin should remain discoverable");
    require(withBuiltinFallback.first().id == QStringLiteral("miles-edgeworth"), "built-in miles id should be present");
    require(withBuiltinFallback.first().builtin, "missing manifest override must not shadow built-in miles");

    QTemporaryDir invalidRootDir;
    require(invalidRootDir.isValid(), "invalid root dir should be valid");
    QDir invalidRoot(invalidRootDir.path());
    require(writeFile(invalidRoot.filePath(QStringLiteral("skin.json")), QStringLiteral(R"JSON(
{"name":"无 id 的根目录皮肤","version":"1.0.0","manifestVersion":1}
)JSON")), "invalid root skin.json should be written");
    QDir validChild(invalidRoot.filePath(QStringLiteral("valid-child")));
    require(validChild.mkpath(QStringLiteral(".")), "valid child skin dir should be created");
    require(writeFile(validChild.filePath(QStringLiteral("skin.json")), QStringLiteral(R"JSON(
{"id":"valid-child","name":"有效子皮肤","version":"1.0.0","manifestVersion":1}
)JSON")), "valid child skin.json should be written");
    require(writeFile(validChild.filePath(QStringLiteral("manifest.json")), QStringLiteral("{}")), "valid child manifest should be written");

    QList<SkinDescriptor> afterInvalidRoot = SkinManifestLoader::discoverInDirectories(
        QStringList{invalidRootDir.path()},
        false
    );
    require(afterInvalidRoot.size() == 1, "invalid root skin.json should not block child discovery");
    require(afterInvalidRoot.first().id == QStringLiteral("valid-child"), "valid child skin should still be discovered");

    return 0;
}
