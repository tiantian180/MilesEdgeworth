#include "pet/manifest/PersonaStore.h"
#include "pet/manifest/SkinManifestLoader.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
QStringList capturedWarnings;

void captureWarnings(QtMsgType type, const QMessageLogContext &, const QString &message)
{
    if (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg) {
        capturedWarnings.append(message);
    }
}

class ScopedMessageCapture
{
public:
    ScopedMessageCapture()
        : m_previous(qInstallMessageHandler(captureWarnings))
    {
        capturedWarnings.clear();
    }

    ~ScopedMessageCapture()
    {
        qInstallMessageHandler(m_previous);
    }

private:
    QtMessageHandler m_previous = nullptr;
};

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

void requireLocalUrl(const QUrl &url, const char *message)
{
    require(url.isValid() && url.isLocalFile(), message);
}

bool writeSkinJson(
    const QDir &skinDir,
    const QString &id,
    const QString &name,
    int skinSchemaVersion = 1,
    const QString &thumbnail = QString()
)
{
    QString thumbnailLine;
    if (!thumbnail.isEmpty()) {
        thumbnailLine = QStringLiteral(R"JSON(,
  "thumbnail": "%1")JSON").arg(thumbnail);
    }

    return writeFile(
        skinDir.filePath(QStringLiteral("skin.json")),
        QStringLiteral(R"JSON({
  "id": "%1",
  "name": "%2",
  "version": "1.0.0",
  "skinSchemaVersion": %3,
  "manifest": "manifest.json"%4
}
)JSON")
            .arg(id, name)
            .arg(skinSchemaVersion)
            .arg(thumbnailLine)
    );
}

bool writeMinimalManifest(const QDir &skinDir)
{
    return writeFile(skinDir.filePath(QStringLiteral("manifest.json")), QStringLiteral("{}"));
}

bool writeValidIdleManifest(const QDir &skinDir)
{
    return skinDir.mkpath(QStringLiteral("assets/body/idle"))
        && writeFile(
            skinDir.filePath(QStringLiteral("assets/body/idle/stand.gif")),
            QStringLiteral("fake stand gif")
        )
        && writeFile(skinDir.filePath(QStringLiteral("manifest.json")), QStringLiteral(R"JSON(
{
  "schemaVersion": 4,
  "fallbackAction": "idle_stand",
  "defaultFacing": "right",
  "states": { "idle": { "action": "idle_stand" } },
  "actions": {
    "idle_stand": {
      "variants": {
        "right": {
          "clip": "file:assets/body/idle/stand.gif"
        }
      }
    }
  }
}
)JSON"));
}

void requireAllAnimationAndAudioUrlsAreLocalFiles(const SkinManifest &manifest)
{
    for (const ActionDefinition &action : manifest.actions) {
        for (const AnimationVariant &variant : action.variants) {
            requireLocalUrl(variant.url, "action variant URL should be a local file");
        }
        for (const PhaseDefinition &phase : action.phases) {
            for (const AnimationVariant &variant : phase.variants) {
                requireLocalUrl(variant.url, "phase variant URL should be a local file");
            }
        }
    }

    for (const RecipeDefinition &recipe : manifest.recipes) {
        if (!recipe.soundUrl.isEmpty()) {
            requireLocalUrl(recipe.soundUrl, "recipe sound URL should be a local file");
        }
        for (const QUrl &url : recipe.soundUrls) {
            requireLocalUrl(url, "localized recipe sound URL should be a local file");
        }
    }
}
} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);

    const QUrl rootUrl(QStringLiteral("file:///tmp/example-skin/"));
    require(
        SkinManifestLoader::resolveSkinUrl(QStringLiteral("file:assets/body/idle.gif"), rootUrl).toString()
            == QStringLiteral("file:///tmp/example-skin/assets/body/idle.gif"),
        "file: URL should resolve under local filesystem root"
    );
    require(
        !SkinManifestLoader::resolveSkinUrl(QStringLiteral("skin:assets/body/idle.gif"), rootUrl).isValid(),
        "legacy skin: URL should be invalid"
    );
    require(
        !SkinManifestLoader::resolveSkinUrl(QStringLiteral("qrc:/pet/stand-right.gif"), rootUrl).isValid(),
        "qrc: URL should be invalid"
    );
    require(
        !SkinManifestLoader::resolveSkinUrl(QStringLiteral("file:../escape.gif"), rootUrl).isValid(),
        "file: URL must reject parent traversal"
    );
    require(
        !SkinManifestLoader::resolveSkinUrl(QStringLiteral("file:///tmp/escape.gif"), rootUrl).isValid(),
        "absolute file URL must not bypass the skin root"
    );

    QTemporaryDir dir;
    require(dir.isValid(), "temporary skin directory should be valid");
    QDir skinDir(dir.path());
    require(skinDir.mkpath(QStringLiteral("assets/body/idle")), "assets directory should be created");
    require(skinDir.mkpath(QStringLiteral("assets/audio")), "audio directory should be created");
    require(skinDir.mkpath(QStringLiteral("generated/clips")), "generated clips directory should be created");
    const QUrl skinRootUrl = QUrl::fromLocalFile(skinDir.absolutePath() + QLatin1Char('/'));
    const QUrl missingAssetUrl = SkinManifestLoader::resolveSkinUrl(
        QStringLiteral("file:assets/body/idle/missing.gif"),
        skinRootUrl
    );
    require(
        missingAssetUrl.isValid() && missingAssetUrl.isLocalFile(),
        "missing but lexically-contained file: asset URL should stay valid"
    );

    QTemporaryDir outsideAssetDir;
    require(outsideAssetDir.isValid(), "outside asset dir should be valid");
    const QString outsideAssetPath = QDir(outsideAssetDir.path()).filePath(QStringLiteral("outside.gif"));
    require(writeFile(outsideAssetPath, QStringLiteral("outside asset placeholder")),
            "outside direct asset target should be written");
    const QString evilAssetPath = skinDir.filePath(QStringLiteral("assets/body/idle/evil.gif"));
    QFile::remove(evilAssetPath);
    const bool assetSymlinkCreated = QFile::link(outsideAssetPath, evilAssetPath);
    if (assetSymlinkCreated && QFileInfo(evilAssetPath).isSymLink()) {
        require(
            !SkinManifestLoader::resolveSkinUrl(QStringLiteral("file:assets/body/idle/evil.gif"), skinRootUrl).isValid(),
            "existing direct asset symlink escaping the skin root should be invalid"
        );
    }
    QFile::remove(evilAssetPath);
    require(writeFile(skinDir.filePath(QStringLiteral("assets/body/idle/stand.gif")),
                      QStringLiteral("fake gif placeholder")),
            "stand asset should be written");
    require(writeFile(skinDir.filePath(QStringLiteral("assets/audio/objection.wav")),
                      QStringLiteral("fake audio placeholder")),
            "audio asset should be written");
    require(writeFile(skinDir.filePath(QStringLiteral("generated/clips/thinking.enter.right.gif")),
                      QStringLiteral("fake generated gif placeholder")),
            "generated clip should be written");
    require(writeSkinJson(
                skinDir,
                QStringLiteral("test-skin"),
                QStringLiteral("测试皮肤"),
                1,
                QStringLiteral("file:assets/body/idle/stand.gif")
            ),
            "skin.json should be written with skinSchemaVersion and thumbnail");
    require(writeFile(skinDir.filePath(QStringLiteral("manifest.json")), QStringLiteral(R"JSON(
{
  "schemaVersion": 4,
  "defaultFacing": "right",
  "motion": {
    "walkSpeed": 42,
    "runSpeed": 84,
    "snapDistance": 6
  },
  "states": { "idle": { "action": "idle_stand" } },
  "clips": {
    "thinking.enter.right": {
      "source": "file:assets/body/idle/stand.gif",
      "frameRange": [1, 4]
    }
  },
  "actions": {
    "idle_stand": {
      "variants": {
        "right": {
          "clip": "file:assets/body/idle/stand.gif"
        }
      }
    },
    "objecting": {
      "loopMode": "onceThenHold",
      "variants": {
        "right": {
          "clip": "thinking.enter.right"
        }
      }
    }
  },
  "recipes": {
    "thinking.holdUntilCancelled": {
      "scope": "agent",
      "sound": "file:assets/audio/objection.wav",
      "sounds": {
        "jp": "file:assets/audio/objection.wav"
      },
      "steps": [
        { "action": "objecting", "phase": "enter" },
        { "action": "objecting", "phase": "loop", "duration": "runtime" },
        { "action": "objecting", "phase": "exit" }
      ]
    },
    "pool.recipeStep": {
      "steps": [
        { "pool": "click.fallback" }
      ]
    },
    "command.returnToIdle": {
      "steps": [
        { "command": "returnToIdle" }
      ]
    },
    "command.toggleFacing": {
      "steps": [
        { "command": "toggleFacing" }
      ]
    }
  },
  "animationPools": {
    "click.fallback": {
      "entries": [
        { "command": "returnToIdle" },
        { "weight": 2 }
      ]
    },
    "menu.tea": {
      "entries": [
        { "recipe": "thinking.holdUntilCancelled" }
      ]
    },
    "idle.random": {
      "entries": [
        { "action": "idle_stand" }
      ]
    }
  },
  "skinCommands": {
    "miles.feedTea": {
      "label": "喂食红茶",
      "request": { "pool": "menu.tea" }
    }
  },
  "behaviorTriggers": {
    "runtime.started": {
      "entries": [
        { "recipe": "thinking.holdUntilCancelled" }
      ]
    },
    "idle.loopFinished": {
      "entries": [
        { "pool": "idle.random", "weight": 70 },
        { "weight": 30 }
      ]
    },
    "test.returnToIdle": {
      "entries": [
        { "command": "returnToIdle" }
      ]
    }
  }
}
)JSON")), "manifest.json should be written");

    SkinManifest manifest = SkinManifestLoader::loadFromDirectory(dir.path());
    require(manifest.skinId == QStringLiteral("test-skin"), "loadFromDirectory should populate skinId");
    require(manifest.skinName == QStringLiteral("测试皮肤"), "loadFromDirectory should populate skinName");
    require(manifest.skinRootUrl.isLocalFile(), "filesystem manifest root should be a local file URL");
    require(manifest.actions.contains(QStringLiteral("idle_stand")), "filesystem manifest should load actions");
    require(std::abs(manifest.motion.walkSpeed - 42.0) < 0.001,
            "manifest motion.walkSpeed should parse");
    require(std::abs(manifest.motion.runSpeed - 84.0) < 0.001,
            "manifest motion.runSpeed should parse");
    require(std::abs(manifest.motion.snapDistance - 6.0) < 0.001,
            "manifest motion.snapDistance should parse");
    requireAllAnimationAndAudioUrlsAreLocalFiles(manifest);

    const QString expectedAnimationUrl = QUrl::fromLocalFile(
        skinDir.filePath(QStringLiteral("assets/body/idle/stand.gif"))
    ).toString();
    require(
        manifest.actions.value(QStringLiteral("idle_stand")).variants.value(QStringLiteral("right")).url.toString()
            == expectedAnimationUrl,
        "file: action URL should resolve under the selected skin root"
    );
    const QString expectedGeneratedClipUrl = QUrl::fromLocalFile(
        skinDir.filePath(QStringLiteral("generated/clips/thinking.enter.right.gif"))
    ).toString();
    require(
        manifest.clips.value(QStringLiteral("thinking.enter.right")).generatedUrl.toString()
            == expectedGeneratedClipUrl,
        "clips.<id> should resolve to file:generated/clips/{id}.gif"
    );
    require(
        manifest.actions.value(QStringLiteral("objecting")).variants.value(QStringLiteral("right")).url.toString()
            == expectedGeneratedClipUrl,
        "bare clip variant should use the generated clip URL"
    );
    require(
        manifest.animationPools.value(QStringLiteral("click.fallback")).entries.first().request.kind
            == ActionRequestKind::ReturnToIdle,
        "animationPools entries should parse command requests"
    );
    require(
        manifest.animationPools.value(QStringLiteral("click.fallback")).entries.size() == 2
            && manifest.animationPools.value(QStringLiteral("click.fallback")).entries.at(1).request.kind
                == ActionRequestKind::None
            && manifest.animationPools.value(QStringLiteral("click.fallback")).entries.at(1).weight == 2,
        "animationPools should preserve weighted no-op skip entries"
    );
    require(
        manifest.skinCommands.value(QStringLiteral("miles.feedTea")).request.kind
            == ActionRequestKind::AnimationPool,
        "skinCommands request should parse pool dispatch"
    );
    require(
        manifest.behaviorTriggers.value(QStringLiteral("runtime.started")).entries.first().request.kind
            == ActionRequestKind::Recipe,
        "behaviorTriggers should parse recipe dispatch"
    );
    require(
        manifest.behaviorTriggers.value(QStringLiteral("idle.loopFinished")).entries.first().request.kind
            == ActionRequestKind::AnimationPool,
        "behaviorTriggers should parse pool dispatch"
    );
    require(
        manifest.behaviorTriggers.value(QStringLiteral("idle.loopFinished")).entries.at(1).request.kind
            == ActionRequestKind::None,
        "behaviorTriggers should preserve skip entries as no-op entries"
    );
    require(
        manifest.behaviorTriggers.value(QStringLiteral("test.returnToIdle")).entries.first().request.kind
            == ActionRequestKind::ReturnToIdle,
        "behaviorTriggers should parse command returnToIdle dispatch"
    );
    require(
        manifest.recipes.value(QStringLiteral("pool.recipeStep")).steps.first().request.kind
            == ActionRequestKind::AnimationPool,
        "recipe step pool key should route to AnimationPool request"
    );
    require(
        manifest.recipes.value(QStringLiteral("command.returnToIdle")).steps.first().request.kind
            == ActionRequestKind::ReturnToIdle,
        "recipe step command returnToIdle should parse"
    );
    require(
        manifest.recipes.value(QStringLiteral("command.toggleFacing")).steps.first().request.kind
            == ActionRequestKind::ToggleFacing,
        "recipe step command toggleFacing should parse"
    );

    QTemporaryDir invalidManifestDir;
    require(invalidManifestDir.isValid(), "invalid manifest dir should be valid");
    QDir invalidManifestSkin(invalidManifestDir.filePath(QStringLiteral("invalid-manifest")));
    require(invalidManifestSkin.mkpath(QStringLiteral(".")), "invalid manifest skin dir should be created");
    require(writeSkinJson(
                invalidManifestSkin,
                QStringLiteral("invalid-manifest"),
                QStringLiteral("Invalid Manifest")
            ),
            "invalid manifest skin.json should be written");
    require(writeFile(invalidManifestSkin.filePath(QStringLiteral("manifest.json")), QStringLiteral("{}")),
            "invalid manifest should be written");
    require(SkinManifestLoader::loadFromDirectory(invalidManifestDir.path()).actions.isEmpty(),
            "loadFromDirectory should not rescue present but invalid manifests");

    const QString packagedMilesManifestPath = QDir(SkinManifestLoader::appSkinDirectoryPath())
        .filePath(QStringLiteral("miles-edgeworth/manifest.json"));
    QFile packagedMilesManifest(packagedMilesManifestPath);
    require(packagedMilesManifest.open(QIODevice::ReadOnly),
            "packaged Miles manifest should be readable for fallbackManifest smoke");
    const QByteArray originalPackagedMilesManifest = packagedMilesManifest.readAll();
    packagedMilesManifest.close();
    require(writeFile(packagedMilesManifestPath, QStringLiteral("{")),
            "packaged Miles manifest should be temporarily replaceable");
    const SkinManifest damagedPackagedFallback = SkinManifestLoader::fallbackManifest();
    const bool damagedPackagedFallsBack = damagedPackagedFallback.actions.contains(QStringLiteral("idle_stand"))
        && damagedPackagedFallback.actions.value(QStringLiteral("idle_stand"))
            .variants.value(QStringLiteral("right"))
            .url.isLocalFile();
    require(writeFile(packagedMilesManifestPath, QString::fromUtf8(originalPackagedMilesManifest)),
            "packaged Miles manifest should be restored after fallbackManifest smoke");
    require(damagedPackagedFallsBack,
            "fallbackManifest should return minimal idle fallback when packaged Miles manifest is invalid");

    QTemporaryDir missingFallbackDir;
    require(missingFallbackDir.isValid(), "missing fallback dir should be valid");
    QDir missingFallbackSkin(missingFallbackDir.filePath(QStringLiteral("missing-fallback")));
    require(missingFallbackSkin.mkpath(QStringLiteral(".")), "missing fallback skin dir should be created");
    require(writeSkinJson(
                missingFallbackSkin,
                QStringLiteral("missing-fallback"),
                QStringLiteral("Missing Fallback")
            ),
            "missing fallback skin.json should be written");
    require(writeFile(missingFallbackSkin.filePath(QStringLiteral("manifest.json")), QStringLiteral(R"JSON(
{
  "schemaVersion": 4,
  "fallbackAction": "idle_stand",
  "actions": {
    "idle_stand": {
      "variants": {
        "right": {
          "clip": "file:assets/body/idle/missing.gif"
        }
      }
    }
  }
}
)JSON")), "missing fallback manifest should be written");
    require(SkinManifestLoader::loadFromDirectory(missingFallbackDir.path()).actions.isEmpty(),
            "loader should reject fallback actions whose animation resource is unavailable");

    QTemporaryDir escapingActionDir;
    require(escapingActionDir.isValid(), "escaping action dir should be valid");
    QDir escapingActionSkin(escapingActionDir.filePath(QStringLiteral("escaping-action")));
    require(escapingActionSkin.mkpath(QStringLiteral("assets/body/idle")),
            "escaping action asset dir should be created");
    require(writeSkinJson(
                escapingActionSkin,
                QStringLiteral("escaping-action"),
                QStringLiteral("Escaping Action")
            ),
            "escaping action skin.json should be written");
    require(writeFile(escapingActionSkin.filePath(QStringLiteral("assets/body/idle/stand.gif")),
                      QStringLiteral("fake stand gif")),
            "escaping action fallback asset should be written");

    QTemporaryDir outsideEscapingActionDir;
    require(outsideEscapingActionDir.isValid(), "outside escaping action dir should be valid");
    const QString outsideEscapingActionPath = QDir(outsideEscapingActionDir.path()).filePath(QStringLiteral("outside.gif"));
    require(writeFile(outsideEscapingActionPath, QStringLiteral("outside action asset")),
            "outside escaping action asset should be written");
    const QString escapingActionPath = escapingActionSkin.filePath(QStringLiteral("assets/body/idle/evil.gif"));
    const bool escapingActionSymlinkCreated = QFile::link(outsideEscapingActionPath, escapingActionPath);
    if (escapingActionSymlinkCreated && QFileInfo(escapingActionPath).isSymLink()) {
        require(writeFile(escapingActionSkin.filePath(QStringLiteral("manifest.json")), QStringLiteral(R"JSON(
{
  "schemaVersion": 4,
  "fallbackAction": "idle_stand",
  "actions": {
    "idle_stand": {
      "variants": {
        "right": {
          "clip": "file:assets/body/idle/stand.gif"
        }
      }
    },
    "evil_action": {
      "variants": {
        "right": {
          "clip": "file:assets/body/idle/evil.gif"
        }
      }
    }
  }
}
)JSON")), "escaping action manifest should be written");
        const SkinManifest escapingManifest = SkinManifestLoader::loadFromDirectory(escapingActionDir.path());
        require(escapingManifest.actions.contains(QStringLiteral("evil_action")),
                "loader should keep non-fallback escaping action assets for runtime fallback");
    }

    require(writeFile(skinDir.filePath(QStringLiteral("manifest.json")), QStringLiteral(R"JSON(
{
  "schemaVersion": 3,
  "defaultFacing": "right",
  "states": { "idle": { "action": "idle_stand" } },
  "actions": {
    "idle_stand": {
      "variants": {
        "right": {
          "animation": "file:assets/body/idle/stand.gif"
        }
      }
    }
  },
  "actionPools": {
    "legacy.pool": {
      "entries": [
        { "command": "returnToIdle" }
      ]
    }
  }
}
)JSON")), "legacy actionPools manifest should be written");
    const SkinManifest legacyPoolsManifest = SkinManifestLoader::loadFromDirectory(dir.path());
    require(
        legacyPoolsManifest.animationPools.value(QStringLiteral("legacy.pool")).entries.first().request.kind
            == ActionRequestKind::ReturnToIdle,
        "legacy actionPools entries should parse command requests"
    );

    require(writeFile(skinDir.filePath(QStringLiteral("manifest.json")), QStringLiteral(R"JSON(
{
  "schemaVersion": 3,
  "defaultFacing": "right",
  "states": { "idle": { "action": "idle_stand" } },
  "actions": {
    "idle_stand": {
      "variants": {
        "right": {
          "animation": "file:assets/body/idle/stand.gif"
        }
      }
    }
  },
  "recipes": {
    "legacy.recipe": {
      "action": "idle_stand"
    }
  },
  "actionPools": {
    "legacy.pool": {
      "entries": [
        { "type": "action", "action": "idle_stand" }
      ]
    }
  },
  "behaviorTriggers": {
    "legacy.dispatch": {
      "entries": [
        { "type": "pool", "pool": "legacy.pool" },
        { "type": "recipe", "recipe": "legacy.recipe" },
        { "type": "action", "action": "idle_stand" },
        { "type": "returnToIdle" },
        { "type": "none", "weight": 2 }
      ]
    }
  }
}
)JSON")), "legacy behaviorTriggers manifest should be written");
    const SkinManifest legacyTriggersManifest = SkinManifestLoader::loadFromDirectory(dir.path());
    const QList<BehaviorTriggerEntry> legacyTriggerEntries = legacyTriggersManifest
        .behaviorTriggers
        .value(QStringLiteral("legacy.dispatch"))
        .entries;
    require(legacyTriggerEntries.size() == 5,
            "loader should keep all legacy behaviorTriggers entries");
    require(
        legacyTriggerEntries.at(0).request.kind == ActionRequestKind::AnimationPool
            && legacyTriggerEntries.at(0).request.targetId == QStringLiteral("legacy.pool"),
        "legacy behaviorTriggers type=pool should parse as AnimationPool request"
    );
    require(
        legacyTriggerEntries.at(1).request.kind == ActionRequestKind::Recipe
            && legacyTriggerEntries.at(1).request.targetId == QStringLiteral("legacy.recipe"),
        "legacy behaviorTriggers type=recipe should parse as Recipe request"
    );
    require(
        legacyTriggerEntries.at(2).request.kind == ActionRequestKind::Action
            && legacyTriggerEntries.at(2).request.targetId == QStringLiteral("idle_stand"),
        "legacy behaviorTriggers type=action should parse as Action request"
    );
    require(
        legacyTriggerEntries.at(3).request.kind == ActionRequestKind::ReturnToIdle,
        "legacy behaviorTriggers type=returnToIdle should parse as ReturnToIdle request"
    );
    require(
        legacyTriggerEntries.at(4).request.kind == ActionRequestKind::None
            && legacyTriggerEntries.at(4).weight == 2,
        "legacy behaviorTriggers type=none should parse as weighted no-op"
    );

    require(writeFile(skinDir.filePath(QStringLiteral("manifest.json")), QStringLiteral(R"JSON(
{
  "schemaVersion": 4,
  "clips": {
    "thinking.enter.right": {
      "source": "file:assets/body/idle/stand.gif",
      "frameRange": [1, 4]
    }
  },
  "actions": {
    "idle_stand": {
      "variants": {
        "right": {
          "clip": "file:assets/body/idle/stand.gif"
        }
      }
    },
    "objecting": {
      "variants": {
        "right": {
          "clip": "thinking.enter.right"
        }
      }
    }
  }
}
)JSON")), "valid generated clip manifest should be restored");

    QFile::remove(skinDir.filePath(QStringLiteral("generated/clips/thinking.enter.right.gif")));
    SkinManifest missingGeneratedClip;
    {
        ScopedMessageCapture capture;
        missingGeneratedClip = SkinManifestLoader::loadFromDirectory(dir.path());
    }
    require(missingGeneratedClip.actions.isEmpty(),
            "loader should reject manifests whose generated clip files are missing");
    const QString warnings = capturedWarnings.join(QLatin1Char('\n'));
    require(
        warnings.contains(QStringLiteral("generated clip"), Qt::CaseInsensitive)
            && warnings.contains(QStringLiteral("thinking.enter.right")),
        "missing generated clip should produce a clear warning"
    );

    require(writeFile(skinDir.filePath(QStringLiteral("generated/clips/thinking.enter.right.gif")),
                      QStringLiteral("fake generated gif placeholder")),
            "generated clip should be restored");

    QTemporaryDir outsideClipDir;
    require(outsideClipDir.isValid(), "outside clip dir should be valid");
    const QString outsideClipPath = QDir(outsideClipDir.path()).filePath(QStringLiteral("outside.gif"));
    require(writeFile(outsideClipPath, QStringLiteral("outside generated gif placeholder")),
            "outside generated clip target should be written");
    const QString evilClipPath = skinDir.filePath(QStringLiteral("generated/clips/evil.clip.gif"));
    QFile::remove(evilClipPath);
    const bool symlinkCreated = QFile::link(outsideClipPath, evilClipPath);
    if (symlinkCreated && QFileInfo(evilClipPath).isSymLink()) {
        require(writeFile(skinDir.filePath(QStringLiteral("manifest.json")), QStringLiteral(R"JSON(
{
  "schemaVersion": 4,
  "clips": {
    "evil.clip": {
      "source": "file:assets/body/idle/stand.gif",
      "frameRange": [1, 1]
    }
  },
  "actions": {
    "evil": {
      "variants": {
        "right": {
          "clip": "evil.clip"
        }
      }
    }
  }
}
)JSON")), "symlink escape manifest should be written");
        require(SkinManifestLoader::loadFromDirectory(dir.path()).actions.isEmpty(),
                "loader should reject generated clip symlink escaping the skin root");
    }
    QFile::remove(evilClipPath);
    require(writeFile(skinDir.filePath(QStringLiteral("manifest.json")), QStringLiteral(R"JSON(
{
  "schemaVersion": 4,
  "clips": {
    "thinking.enter.right": {
      "source": "file:assets/body/idle/stand.gif",
      "frameRange": [1, 4]
    }
  },
  "actions": {
    "idle_stand": {
      "variants": {
        "right": {
          "clip": "file:assets/body/idle/stand.gif"
        }
      }
    },
    "objecting": {
      "variants": {
        "right": {
          "clip": "thinking.enter.right"
        }
      }
    }
  }
}
)JSON")), "valid manifest should be restored after generated clip symlink rejection");

    QList<SkinDescriptor> descriptors = SkinManifestLoader::discoverInDirectories(QStringList{dir.path()});
    require(descriptors.size() == 1, "discoverInDirectories should find one test skin");
    require(descriptors.first().id == QStringLiteral("test-skin"), "descriptor id should come from skin.json");
    require(descriptors.first().skinSchemaVersion == 1, "descriptor should read skinSchemaVersion");
    require(descriptors.first().rootUrl.isLocalFile(), "filesystem descriptor root should be a file URL");
    require(QFileInfo(descriptors.first().manifestPath).isAbsolute(),
            "descriptor manifestPath should be an absolute filesystem path");
    require(
        descriptors.first().thumbnailUrl.toString() == expectedAnimationUrl,
        "descriptor thumbnail should resolve through strict file: URL handling"
    );

    QTemporaryDir userDir;
    QTemporaryDir appDir;
    require(userDir.isValid() && appDir.isValid(), "precedence dirs should be valid");
    QDir userSkin(userDir.path() + QStringLiteral("/same-skin"));
    QDir appSkin(appDir.path() + QStringLiteral("/same-skin"));
    require(userSkin.mkpath(QStringLiteral(".")), "user skin dir should be created");
    require(appSkin.mkpath(QStringLiteral(".")), "app skin dir should be created");
    require(writeSkinJson(userSkin, QStringLiteral("same-id"), QStringLiteral("用户版本")),
            "user skin.json should be written");
    require(writeSkinJson(appSkin, QStringLiteral("same-id"), QStringLiteral("应用版本")),
            "app skin.json should be written");
    require(writeValidIdleManifest(userSkin), "user manifest should be written");
    require(writeValidIdleManifest(appSkin), "app manifest should be written");

    const QList<SkinDescriptor> precedence = SkinManifestLoader::discoverInDirectories(
        QStringList{userDir.path(), appDir.path()}
    );
    require(precedence.size() == 2, "same id discovery should keep ordered candidates");
    require(precedence.first().name == QStringLiteral("用户版本"),
            "user directory descriptor should stay first when duplicate ids exist");
    require(precedence.at(1).name == QStringLiteral("应用版本"),
            "app directory descriptor should stay available as a fallback candidate");

    QTemporaryDir invalidShadowUserDir;
    QTemporaryDir invalidShadowAppDir;
    require(invalidShadowUserDir.isValid() && invalidShadowAppDir.isValid(),
            "invalid shadow dirs should be valid");
    QDir invalidShadowUserSkin(invalidShadowUserDir.path() + QStringLiteral("/same-skin"));
    QDir invalidShadowAppSkin(invalidShadowAppDir.path() + QStringLiteral("/same-skin"));
    require(invalidShadowUserSkin.mkpath(QStringLiteral(".")),
            "invalid shadow user skin dir should be created");
    require(invalidShadowAppSkin.mkpath(QStringLiteral(".")),
            "invalid shadow app skin dir should be created");
    require(writeSkinJson(invalidShadowUserSkin, QStringLiteral("same-shadow"), QStringLiteral("损坏用户版本")),
            "invalid shadow user skin.json should be written");
    require(writeSkinJson(invalidShadowAppSkin, QStringLiteral("same-shadow"), QStringLiteral("应用版本")),
            "invalid shadow app skin.json should be written");
    require(writeMinimalManifest(invalidShadowUserSkin),
            "invalid shadow user manifest should be written");
    require(writeValidIdleManifest(invalidShadowAppSkin),
            "invalid shadow app manifest should be written");
    const QList<SkinDescriptor> invalidShadowDescriptors = SkinManifestLoader::discoverInDirectories(
        QStringList{invalidShadowUserDir.path(), invalidShadowAppDir.path()}
    );
    require(invalidShadowDescriptors.size() == 2,
            "discovery should remain a lightweight skin.json scan even when manifest is invalid");
    require(invalidShadowDescriptors.first().name == QStringLiteral("损坏用户版本"),
            "invalid user descriptor should stay visible for runtime fallback handling");
    require(invalidShadowDescriptors.at(1).name == QStringLiteral("应用版本"),
            "same-id app descriptor should stay available after an invalid user descriptor");

    QTemporaryDir invalidRootDir;
    require(invalidRootDir.isValid(), "invalid root dir should be valid");
    QDir invalidRoot(invalidRootDir.path());
    require(writeFile(invalidRoot.filePath(QStringLiteral("skin.json")), QStringLiteral(R"JSON(
{"name":"无 id 的根目录皮肤","version":"1.0.0","skinSchemaVersion":1,"manifest":"manifest.json"}
)JSON")), "invalid root skin.json should be written");
    QDir validChild(invalidRoot.filePath(QStringLiteral("valid-child")));
    require(validChild.mkpath(QStringLiteral(".")), "valid child skin dir should be created");
    require(writeSkinJson(validChild, QStringLiteral("valid-child"), QStringLiteral("有效子皮肤")),
            "valid child skin.json should be written");
    require(writeValidIdleManifest(validChild), "valid child manifest should be written");

    const QList<SkinDescriptor> afterInvalidRoot = SkinManifestLoader::discoverInDirectories(
        QStringList{invalidRootDir.path()}
    );
    require(afterInvalidRoot.size() == 1, "invalid root skin.json should not block child discovery");
    require(afterInvalidRoot.first().id == QStringLiteral("valid-child"),
            "valid child skin should still be discovered");

    QTemporaryDir missingManifestDir;
    require(missingManifestDir.isValid(), "missing-manifest dir should be valid");
    QDir missingManifestSkin(missingManifestDir.path() + QStringLiteral("/missing-manifest"));
    require(missingManifestSkin.mkpath(QStringLiteral(".")), "missing-manifest skin dir should be created");
    require(writeSkinJson(missingManifestSkin, QStringLiteral("missing-manifest"), QStringLiteral("缺 manifest")),
            "missing-manifest skin.json should be written");
    require(
        SkinManifestLoader::discoverInDirectories(QStringList{missingManifestDir.path()}).isEmpty(),
        "skin with missing manifest.json should be skipped"
    );

    QTemporaryDir missingSchemaDir;
    require(missingSchemaDir.isValid(), "missing-schema dir should be valid");
    QDir missingSchemaSkin(missingSchemaDir.path() + QStringLiteral("/missing-schema"));
    require(missingSchemaSkin.mkpath(QStringLiteral(".")), "missing-schema skin dir should be created");
    require(writeFile(missingSchemaSkin.filePath(QStringLiteral("skin.json")), QStringLiteral(R"JSON(
{"id":"missing-schema","name":"缺 schema","version":"1.0.0","manifest":"manifest.json"}
)JSON")), "missing-schema skin.json should be written");
    require(writeMinimalManifest(missingSchemaSkin), "missing-schema manifest should be written");
    require(
        SkinManifestLoader::discoverInDirectories(QStringList{missingSchemaDir.path()}).isEmpty(),
        "skin with missing skinSchemaVersion should be skipped"
    );

    QTemporaryDir nonIntegerSchemaDir;
    require(nonIntegerSchemaDir.isValid(), "non-integer-schema dir should be valid");
    QDir nonIntegerSchemaSkin(nonIntegerSchemaDir.path() + QStringLiteral("/non-integer-schema"));
    require(nonIntegerSchemaSkin.mkpath(QStringLiteral(".")), "non-integer-schema skin dir should be created");
    require(writeFile(nonIntegerSchemaSkin.filePath(QStringLiteral("skin.json")), QStringLiteral(R"JSON(
{"id":"non-integer-schema","name":"非整数 schema","version":"1.0.0","skinSchemaVersion":"1","manifest":"manifest.json"}
)JSON")), "non-integer-schema skin.json should be written");
    require(writeMinimalManifest(nonIntegerSchemaSkin), "non-integer-schema manifest should be written");
    require(
        SkinManifestLoader::discoverInDirectories(QStringList{nonIntegerSchemaDir.path()}).isEmpty(),
        "skin with non-integer skinSchemaVersion should be skipped"
    );

    QTemporaryDir unsupportedSchemaDir;
    require(unsupportedSchemaDir.isValid(), "unsupported-schema dir should be valid");
    QDir unsupportedSchemaSkin(unsupportedSchemaDir.path() + QStringLiteral("/unsupported-schema"));
    require(unsupportedSchemaSkin.mkpath(QStringLiteral(".")), "unsupported-schema skin dir should be created");
    require(writeSkinJson(
                unsupportedSchemaSkin,
                QStringLiteral("unsupported-schema"),
                QStringLiteral("不支持的 schema"),
                999
            ),
            "unsupported-schema skin.json should be written");
    require(writeMinimalManifest(unsupportedSchemaSkin), "unsupported-schema manifest should be written");
    require(
        SkinManifestLoader::discoverInDirectories(QStringList{unsupportedSchemaDir.path()}).isEmpty(),
        "skin with unsupported skinSchemaVersion should be skipped"
    );

    QTemporaryDir personaDataDir;
    require(personaDataDir.isValid(), "persona data dir should be valid");
    qputenv("MILES_DATA_DIR", personaDataDir.path().toUtf8());

    QDir userPersonaSkin(personaDataDir.path() + QStringLiteral("/user-persona-skin"));
    require(userPersonaSkin.mkpath(QStringLiteral(".")), "user persona skin dir should be created");
    SkinManifest userPersonaManifest;
    userPersonaManifest.skinId = QStringLiteral("user-persona");
    userPersonaManifest.skinRootUrl = QUrl::fromLocalFile(userPersonaSkin.absolutePath() + QLatin1Char('/'));
    QString personaError;
    require(PersonaStore::writeForManifest(userPersonaManifest, QStringLiteral("User persona\n"), &personaError),
            "user-dir persona should write to skin root");
    require(QFileInfo::exists(userPersonaSkin.filePath(QStringLiteral("persona.md"))),
            "user-dir persona.md should exist inside the skin root");
    const QString staleUserOverridePath = PersonaStore::overridePathForSkin(QStringLiteral("user-persona"));
    require(QDir(QFileInfo(staleUserOverridePath).absolutePath()).mkpath(QStringLiteral(".")),
            "stale user override directory should be created");
    require(writeFile(staleUserOverridePath, QStringLiteral("stale override\n")),
            "stale user override should be written");
    SkinDescriptor userPersonaDescriptor;
    userPersonaDescriptor.id = QStringLiteral("user-persona");
    userPersonaDescriptor.rootUrl = userPersonaManifest.skinRootUrl;
    require(PersonaStore::readForDescriptor(userPersonaDescriptor) == QStringLiteral("User persona\n"),
            "user-dir persona should read from skin root, not override storage");

    QDir appSkinParent(SkinManifestLoader::appSkinDirectoryPath());
    require(appSkinParent.mkpath(QStringLiteral(".")), "app skin parent should be created");
    QDir appPersonaSkin(appSkinParent.filePath(QStringLiteral("app-persona-smoke")));
    if (appPersonaSkin.exists()) {
        require(appPersonaSkin.removeRecursively(), "existing app persona smoke dir should be removable");
    }
    require(appPersonaSkin.mkpath(QStringLiteral(".")), "app persona skin dir should be created");
    require(writeFile(appPersonaSkin.filePath(QStringLiteral("persona.md")), QStringLiteral("Bundled persona\n")),
            "app-dir bundled persona should be written");

    SkinDescriptor appPersonaDescriptor;
    appPersonaDescriptor.id = QStringLiteral("app-persona-smoke");
    appPersonaDescriptor.rootUrl = QUrl::fromLocalFile(appPersonaSkin.absolutePath() + QLatin1Char('/'));
    require(PersonaStore::readForDescriptor(appPersonaDescriptor) == QStringLiteral("Bundled persona\n"),
            "app-dir skin should read bundled persona when no override exists");

    SkinManifest appPersonaManifest;
    appPersonaManifest.skinId = appPersonaDescriptor.id;
    appPersonaManifest.skinRootUrl = appPersonaDescriptor.rootUrl;
    personaError.clear();
    require(PersonaStore::writeForManifest(appPersonaManifest, QStringLiteral("Override persona\n"), &personaError),
            "app-dir persona should write to override storage");
    const QString expectedOverridePath = QDir(personaDataDir.path())
        .filePath(QStringLiteral("skin-overrides/app-persona-smoke/persona.md"));
    require(PersonaStore::overridePathForSkin(appPersonaManifest.skinId) == expectedOverridePath,
            "override path should use skin-overrides/{skinId}/persona.md");
    require(QFileInfo::exists(expectedOverridePath), "app-dir persona override file should exist");
    require(PersonaStore::readForDescriptor(appPersonaDescriptor) == QStringLiteral("Override persona\n"),
            "app-dir skin should read override before bundled persona");
    require(appPersonaSkin.removeRecursively(), "app persona smoke dir should be cleaned up");
    qunsetenv("MILES_DATA_DIR");

    return 0;
}
