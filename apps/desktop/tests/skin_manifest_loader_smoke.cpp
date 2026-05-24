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
        SkinManifestLoader::resolveSkinUrl(QStringLiteral("file:assets/body/idle.gif"), rootUrl).toString()
            == QStringLiteral("file:///tmp/example-skin/assets/body/idle.gif"),
        "file: URL should resolve under file root"
    );
    require(
        SkinManifestLoader::resolveSkinUrl(QStringLiteral("skin:assets/body/idle.gif"), rootUrl).toString()
            == QStringLiteral("file:///tmp/example-skin/assets/body/idle.gif"),
        "legacy skin: URL should resolve under file root"
    );
    require(
        SkinManifestLoader::resolveSkinUrl(QStringLiteral("qrc:/pet/stand-right.gif"), rootUrl).toString()
            == QStringLiteral("qrc:/pet/stand-right.gif"),
        "absolute qrc URL should stay unchanged"
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
    require(skinDir.mkpath(QStringLiteral("generated/clips")), "generated clips directory should be created");
    require(writeFile(skinDir.filePath(QStringLiteral("generated/clips/thinking.enter.right.gif")),
                      QStringLiteral("fake gif placeholder")),
            "generated clip should be written");
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
  "schemaVersion": 4,
  "defaultFacing": "right",
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
    const QString expectedGeneratedClipUrl = QUrl::fromLocalFile(
        skinDir.filePath(QStringLiteral("generated/clips/thinking.enter.right.gif"))
    ).toString();
    require(objectingVariant.url.toString() == expectedGeneratedClipUrl,
            "bare clip variant should resolve to generated/clips/{clip}.gif");
    const RecipeDefinition thinkingRecipe = manifest.recipes.value(QStringLiteral("thinking.holdUntilCancelled"));
    require(thinkingRecipe.steps.size() == 3,
            "loader should parse runtime-controlled recipe steps");
    require(thinkingRecipe.steps.at(1).durationMode == QStringLiteral("runtime"),
            "loader should preserve duration runtime on recipe step");
    require(manifest.animationPools.contains(QStringLiteral("click.fallback")),
            "loader should parse top-level animationPools");
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
        "animationPools should preserve v4 skip entries as weighted no-op entries"
    );
    require(
        manifest.skinCommands.value(QStringLiteral("miles.feedTea")).request.kind
            == ActionRequestKind::AnimationPool,
        "skinCommands request should parse v4 pool key dispatch"
    );
    require(
        manifest.behaviorTriggers.value(QStringLiteral("runtime.started")).entries.first().request.kind
            == ActionRequestKind::Recipe,
        "behaviorTriggers should parse v4 recipe key dispatch"
    );
    require(
        manifest.behaviorTriggers.value(QStringLiteral("idle.loopFinished")).entries.first().request.kind
            == ActionRequestKind::AnimationPool,
        "behaviorTriggers should parse v4 pool key dispatch"
    );
    require(
        manifest.behaviorTriggers.value(QStringLiteral("idle.loopFinished")).entries.at(1).request.kind
            == ActionRequestKind::None,
        "behaviorTriggers should preserve v4 skip entries as no-op entries"
    );
    require(
        manifest.behaviorTriggers.value(QStringLiteral("test.returnToIdle")).entries.first().request.kind
            == ActionRequestKind::ReturnToIdle,
        "behaviorTriggers should parse v4 command returnToIdle dispatch"
    );
    require(
        manifest.recipes.value(QStringLiteral("pool.recipeStep")).steps.first().request.kind
            == ActionRequestKind::AnimationPool,
        "recipe step pool key should route to AnimationPool request"
    );
    require(
        manifest.recipes.value(QStringLiteral("pool.recipeStep")).steps.first().request.targetId
            == QStringLiteral("click.fallback"),
        "recipe step pool key should preserve pool id"
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
    require(manifest.personaPrompt == filesystemPersona, "filesystem skin persona.md should load into manifest");

    require(writeFile(skinDir.filePath(QStringLiteral("manifest.json")), QStringLiteral(R"JSON(
{
  "schemaVersion": 3,
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
    require(legacyPoolsManifest.animationPools.contains(QStringLiteral("legacy.pool")),
            "loader should keep legacy actionPools fallback");
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
          "animation": "skin:assets/body/idle/stand.gif"
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
    "../bad": {
      "source": "file:assets/body/idle/stand.gif",
      "frameRange": [1, 1]
    }
  },
  "actions": {
    "bad": { "variants": { "right": { "clip": "../bad" } } }
  }
}
)JSON")), "invalid clip name manifest should be written");
    require(SkinManifestLoader::loadFromDirectory(dir.path()).actions.isEmpty(),
            "loader should reject invalid clip names");

    require(writeFile(skinDir.filePath(QStringLiteral("manifest.json")), QStringLiteral(R"JSON(
{
  "schemaVersion": 4,
  "clips": {
    "bad.source": {
      "source": "https://example.invalid/source.gif",
      "frameRange": [1, 1]
    }
  },
  "actions": {
    "bad": { "variants": { "right": { "clip": "bad.source" } } }
  }
}
)JSON")), "non-file clip source manifest should be written");
    require(SkinManifestLoader::loadFromDirectory(dir.path()).actions.isEmpty(),
            "loader should reject non-file clip sources");

    require(writeFile(skinDir.filePath(QStringLiteral("manifest.json")), QStringLiteral(R"JSON(
{
  "schemaVersion": 4,
  "clips": {
    "bad.range": {
      "source": "file:assets/body/idle/stand.gif",
      "frameRange": [4, 1]
    }
  },
  "actions": {
    "bad": { "variants": { "right": { "clip": "bad.range" } } }
  }
}
)JSON")), "invalid frameRange manifest should be written");
    require(SkinManifestLoader::loadFromDirectory(dir.path()).actions.isEmpty(),
            "loader should reject invalid clip frameRange");

    require(writeFile(skinDir.filePath(QStringLiteral("manifest.json")), QStringLiteral(R"JSON(
{
  "schemaVersion": 4,
  "actions": {
    "bad": { "variants": { "right": { "clip": " missing.clip " } } }
  }
}
)JSON")), "unknown clip ref manifest should be written");
    require(SkinManifestLoader::loadFromDirectory(dir.path()).actions.isEmpty(),
            "loader should reject unknown bare clip refs");

    require(writeFile(skinDir.filePath(QStringLiteral("manifest.json")), QStringLiteral(R"JSON(
{
  "schemaVersion": 4,
  "defaultFacing": "right",
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
      "steps": [
        { "action": "objecting", "phase": "enter" },
        { "action": "objecting", "phase": "loop", "duration": "runtime" },
        { "action": "objecting", "phase": "exit" }
      ]
    }
  }
}
)JSON")), "valid manifest should be restored");

    QFile::remove(skinDir.filePath(QStringLiteral("generated/clips/thinking.enter.right.gif")));
    SkinManifest missingGeneratedClip = SkinManifestLoader::loadFromDirectory(dir.path());
    require(missingGeneratedClip.actions.isEmpty(),
            "loader should reject manifests whose generated clip files are missing");

    QTemporaryDir personaDataDir;
    require(personaDataDir.isValid(), "persona override data dir should be valid");
    qputenv("MILES_DATA_DIR", personaDataDir.path().toUtf8());

    SkinManifest builtInMiles = SkinManifestLoader::loadFromResource(QStringLiteral(":/skins/miles-edgeworth/manifest.json"));
    require(
        builtInMiles.actions.value(QStringLiteral("idle_stand")).variants.value(QStringLiteral("right")).url.toString()
            == QStringLiteral("qrc:/skins/miles-edgeworth/assets/body/idle/stand-right.gif"),
        "built-in Miles idle_stand should resolve file: URL to qrc"
    );
    const AnimationVariant thinkingEnterRight = builtInMiles
        .actions.value(QStringLiteral("thinking"))
        .phases.value(QStringLiteral("enter"))
        .variants.value(QStringLiteral("right"));
    require(
        thinkingEnterRight.url.toString()
            == QStringLiteral("qrc:/skins/miles-edgeworth/generated/clips/thinking.enter.right.gif"),
        "built-in Miles thinking enter should resolve to generated clip qrc URL"
    );
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
