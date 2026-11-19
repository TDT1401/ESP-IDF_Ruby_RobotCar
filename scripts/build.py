#!/usr/bin/env python3

import sys
import os
import json
import zipfile
import argparse
import re
import subprocess
from pathlib import Path
from typing import Any, Optional
from pathlib import Path

# Switch to project root directory
os.chdir(Path(__file__).resolve().parent.parent)

################################################################################
# Common utility functions
################################################################################


_DEFAULT_IDF_VERSION = (6, 0, 2)

_LITE_WAKE_WORD_TARGETS = {"esp32c3", "esp32c5", "esp32c6"}
_AFE_WAKE_WORD_TARGETS = {"esp32s3", "esp32p4", "esp32s31"}
_ESP_WAKE_WORD_TARGETS = {"esp32", *_LITE_WAKE_WORD_TARGETS}
_WAKE_WORD_TARGETS = (
    "esp32",
    "esp32c3",
    "esp32c5",
    "esp32c6",
    "esp32s3",
    "esp32p4",
    "esp32s31",
)
_WAKE_WORD_MODEL_PATTERN = re.compile(r"^wn9[sl]?_[a-z0-9_]+$")
_ESP_SR_KCONFIG = Path(
    "managed_components/espressif__esp-sr/Kconfig.projbuild"
)


def _emit_build_stage(stage: str) -> None:
    """Emit a machine-readable stage marker for cloud build runners."""
    enabled = os.environ.get("XIAOZHI_BUILD_STAGES", "").strip().casefold()
    if enabled in {"1", "true", "yes", "on"}:
        print(f"XIAOZHI_STAGE {stage}", flush=True)


def get_project_version() -> Optional[str]:
    """Read set(PROJECT_VER "x.y.z") from root CMakeLists.txt"""
    with Path("CMakeLists.txt").open(encoding='utf-8') as f:
        for line in f:
            if line.startswith("set(PROJECT_VER"):
                return line.split("\"")[1]
    return None

def _get_idf_command() -> list[str]:
    """Get the command used to invoke the active ESP-IDF."""
    idf_path = os.environ.get("IDF_PATH")

    if idf_path:
        idf_py = Path(idf_path) / "tools" / "idf.py"
        if idf_py.is_file():
            return [sys.executable, str(idf_py)]

    raise RuntimeError(
        "ESP-IDF environment is not initialized correctly. "
        "IDF_PATH/tools/idf.py was not found."
    )

def _run_idf(*args: str, preview: bool = False) -> None:
    command = _get_idf_command()
    if preview:
        command.append("--preview")
    command.extend(args)
    if subprocess.run(command, check=False).returncode != 0:
        print(f"{' '.join(command)} failed", file=sys.stderr)
        sys.exit(1)


def merge_bin(preview: bool = False) -> None:
    _run_idf("merge-bin", preview=preview)


def zip_bin(name: str, version: str) -> None:
    """Zip build/merged-binary.bin to releases/v{version}_{name}.zip"""
    out_dir = Path("releases")
    out_dir.mkdir(exist_ok=True)
    output_path = out_dir / f"v{version}_{name}.zip"

    if output_path.exists():
        output_path.unlink()

    with zipfile.ZipFile(output_path, "w", compression=zipfile.ZIP_DEFLATED) as zipf:
        zipf.write("build/merged-binary.bin", arcname="merged-binary.bin")
    print(f"zip bin to {output_path} done")


def _get_manufacturer(cfg: dict) -> Optional[str]:
    """Read manufacturer from build_config.json"""
    m = cfg.get("manufacturer")
    if isinstance(m, str) and m.strip():
        return m.strip()
    return None


_REPORTED_IDENTIFIER_PATTERN = re.compile(r"^[a-z0-9.-]+$")


def _validate_reported_identifier(value: object, field: str) -> str:
    """Validate an OTA-reported board type or name."""
    if not isinstance(value, str) or not value:
        raise ValueError(f"missing non-empty {field}")
    if not _REPORTED_IDENTIFIER_PATTERN.fullmatch(value):
        raise ValueError(
            f"{field} {value!r} must contain only lowercase letters, "
            'digits, "." and "-"'
        )
    return value


def _get_reported_type(cfg: dict) -> str:
    """Read the compatibility-sensitive board type from build_config.json."""
    return _validate_reported_identifier(cfg.get("type"), 'top-level "type"')


def _get_reported_name(build: dict) -> str:
    """Read the compatibility-sensitive board name from a build entry."""
    return _validate_reported_identifier(build.get("name"), 'build "name"')


def _get_full_name(manufacturer: Optional[str], name: str) -> str:
    """Return the artifact name without duplicating an existing manufacturer prefix."""
    prefix = f"{manufacturer}-" if manufacturer else ""
    return name if not prefix or name.startswith(prefix) else f"{prefix}{name}"


def _normalize_p4x_release_name(name: str) -> str:
    """Represent P4X as a chip-family segment instead of a trailing suffix."""
    if name.endswith("-p4x"):
        name_without_suffix = name[:-4]
        if "-p4-" in name_without_suffix:
            return name_without_suffix.replace("-p4-", "-p4x-", 1)
    return name


def _get_release_full_name(
    manufacturer: Optional[str],
    build: dict,
) -> str:
    """Return manufacturer + board name for the release artifact."""
    release_name = _get_reported_name(build)
    release_name = _normalize_p4x_release_name(release_name)
    return _get_full_name(manufacturer, release_name)


def _normalize_language(language: str) -> str:
    """Normalize a locale accepted by the build interface."""
    supported_languages = _collect_languages()
    languages_by_casefold = {
        supported.casefold(): supported
        for supported in supported_languages
    }
    normalized = language.strip().replace("_", "-").casefold()
    try:
        return languages_by_casefold[normalized]
    except KeyError as error:
        supported = ", ".join(supported_languages)
        raise ValueError(
            f"Unsupported language {language!r}. Supported values: {supported}"
        ) from error


def _collect_languages(
    cmake_path: Path = Path("main/CMakeLists.txt"),
    kconfig_path: Path = Path("main/Kconfig.projbuild"),
    locales_dir: Path = Path("main/assets/locales"),
) -> list[str]:
    """Read configured locale mappings and validate their build resources."""
    for path in (cmake_path, kconfig_path, locales_dir):
        if not path.exists():
            raise RuntimeError(f"Language configuration source not found: {path}")

    cmake = cmake_path.read_text(encoding="utf-8")
    mappings = re.findall(
        r"(?:if|elseif)\(CONFIG_LANGUAGE_([A-Z_]+)\)\s*"
        r'set\(LANG_DIR "([^"]+)"\)',
        cmake,
    )
    if not mappings:
        raise RuntimeError(f"No language mappings found in {cmake_path}")

    kconfig = kconfig_path.read_text(encoding="utf-8")
    errors: list[str] = []
    languages: list[str] = []
    for symbol, language in mappings:
        if not re.search(
            rf"^\s*config LANGUAGE_{re.escape(symbol)}$",
            kconfig,
            re.MULTILINE,
        ):
            errors.append(f"CONFIG_LANGUAGE_{symbol} is missing from {kconfig_path}")
        if not (locales_dir / language).is_dir():
            errors.append(f"locale directory is missing: {locales_dir / language}")
        if language in languages:
            errors.append(f"duplicate language mapping: {language}")
        else:
            languages.append(language)

    if errors:
        details = "\n".join(f"  - {error}" for error in errors)
        raise RuntimeError(f"Invalid language configuration:\n{details}")
    return languages


def _language_sdkconfig_option(language: str) -> tuple[str, str]:
    """Return the normalized locale and its Kconfig assignment."""
    normalized = _normalize_language(language)
    symbol = normalized.replace("-", "_").upper()
    return normalized, f"CONFIG_LANGUAGE_{symbol}=y"


def _collect_wake_words(
    kconfig_path: Path = _ESP_SR_KCONFIG,
) -> list[dict[str, object]]:
    """Read available WakeNet models from the resolved ESP-SR component."""
    if not kconfig_path.exists():
        raise RuntimeError(
            f"{kconfig_path} was not found. Resolve managed components with "
            "'idf.py reconfigure' before listing wake words."
        )

    content = kconfig_path.read_text(encoding="utf-8")
    config_matches = list(re.finditer(
        r"^\s*config SR_WN_([A-Z0-9_]+)\s*$",
        content,
        re.MULTILINE,
    ))
    wake_words: list[dict[str, object]] = []
    for index, match in enumerate(config_matches):
        block_end = (
            config_matches[index + 1].start()
            if index + 1 < len(config_matches)
            else len(content)
        )
        block = content[match.end():block_end]
        label_match = re.search(r'^\s*bool "([^"]+)"\s*$', block, re.MULTILINE)
        if not label_match:
            continue

        model = match.group(1).lower()
        label = label_match.group(1)
        suffix = f"({model})"
        phrase = (
            label[:-len(suffix)].strip()
            if label.endswith(suffix)
            else label
        )
        targets = (
            _WAKE_WORD_TARGETS
            if model.startswith("wn9s_")
            else (
                "esp32",
                "esp32s3",
                "esp32p4",
                "esp32s31",
            )
        )
        wake_words.append({
            "model": model,
            "phrase": phrase,
            "targets": list(targets),
        })

    if not wake_words:
        raise RuntimeError(f"No WakeNet models found in {kconfig_path}")
    return wake_words


def _print_wake_word_list(wake_words: list[dict[str, object]]) -> None:
    """Print a human-readable table of available wake-word models."""
    model_width = max(
        len("MODEL"),
        *(len(str(item["model"])) for item in wake_words),
    )
    phrase_width = max(
        len("PHRASE"),
        *(len(str(item["phrase"])) for item in wake_words),
    )
    print(f"{'MODEL':<{model_width}}  {'PHRASE':<{phrase_width}}  TARGETS")
    for item in wake_words:
        targets = ",".join(str(target) for target in item["targets"])
        print(
            f"{str(item['model']):<{model_width}}  "
            f"{str(item['phrase']):<{phrase_width}}  {targets}"
        )
    print("\nSpecial values: nihaoxiaozhi, disabled")


def _enabled_default_wake_word_symbols(target: str) -> list[str]:
    """Return wake-word models enabled by the project and target defaults."""
    symbols: list[str] = []
    for path in (Path("sdkconfig.defaults"), Path(f"sdkconfig.defaults.{target}")):
        if not path.exists():
            continue
        for line in path.read_text(encoding="utf-8").splitlines():
            match = re.fullmatch(r"(CONFIG_SR_WN_[A-Z0-9_]+)=y", line.strip())
            if match and match.group(1) not in symbols:
                symbols.append(match.group(1))
    return symbols


def _board_supports_wake_word(
    target: str,
    sdkconfig_append: list[str],
) -> bool:
    """Return whether one board variant satisfies the wake-word Kconfig deps."""
    if target in _LITE_WAKE_WORD_TARGETS:
        return True
    if target not in _AFE_WAKE_WORD_TARGETS | {"esp32"}:
        return False

    defaults: list[str] = []
    for path in (Path("sdkconfig.defaults"), Path(f"sdkconfig.defaults.{target}")):
        if not path.exists():
            continue
        defaults.extend(
            line.strip()
            for line in path.read_text(encoding="utf-8").splitlines()
            if line.strip().startswith("CONFIG_") and "=" in line
        )
    assignments = _sdkconfig_assignments(
        _merge_sdkconfig_options(defaults, sdkconfig_append)
    )
    return assignments.get("CONFIG_SPIRAM") == "y"


def _wake_word_sdkconfig_options(
    wake_word: str,
    target: str,
) -> tuple[str, list[str], list[str]]:
    """Map a wake-word model to implementation and model Kconfig options."""
    normalized = wake_word.strip().casefold().replace("-", "_")
    if normalized == "nihaoxiaozhi":
        normalized = (
            "wn9s_nihaoxiaozhi"
            if target in _LITE_WAKE_WORD_TARGETS
            else "wn9_nihaoxiaozhi_tts"
        )

    model_symbols = _enabled_default_wake_word_symbols(target)
    options = [f"{symbol}=n" for symbol in model_symbols]
    options.extend([
        "CONFIG_WAKE_WORD_DISABLED=n",
        "CONFIG_USE_ESP_WAKE_WORD=n",
        "CONFIG_USE_AFE_WAKE_WORD=n",
        "CONFIG_USE_CUSTOM_WAKE_WORD=n",
    ])

    if normalized == "disabled":
        options.append("CONFIG_WAKE_WORD_DISABLED=y")
        return normalized, options, ["CONFIG_WAKE_WORD_DISABLED"]

    if target not in _ESP_WAKE_WORD_TARGETS | _AFE_WAKE_WORD_TARGETS:
        raise ValueError(f"Wake-word selection is not supported for target {target}")
    if not _WAKE_WORD_MODEL_PATTERN.fullmatch(normalized):
        raise ValueError(
            f"Invalid wake word {wake_word!r}. Use 'disabled', "
            "'nihaoxiaozhi', or an ESP-SR model name such as "
            "'wn9_jarvis_tts'."
        )
    if target in _LITE_WAKE_WORD_TARGETS and not normalized.startswith("wn9s_"):
        raise ValueError(
            f"Target {target} supports WakeNet9s models only; "
            f"{normalized!r} is not compatible"
        )

    implementation = (
        "CONFIG_USE_AFE_WAKE_WORD"
        if target in _AFE_WAKE_WORD_TARGETS
        else "CONFIG_USE_ESP_WAKE_WORD"
    )
    model_symbol = f"CONFIG_SR_WN_{normalized.upper()}"
    options.extend((f"{implementation}=y", f"{model_symbol}=y"))
    return normalized, options, [implementation, model_symbol]

################################################################################
# project build configuration
################################################################################

_BOARDS_DIR = Path("main/boards")
_BUILD_CONFIG = Path("config/build_config.json")
_PROJECT_BOARD = "robotcar"
_BOARD_CONFIG_SYMBOL = "CONFIG_BOARD_TYPE_BREAD_COMPACT_WIFI_LCD"

_DISPLAY_STYLE_SYMBOLS = {
    "default": "CONFIG_USE_DEFAULT_MESSAGE_STYLE",
    "wechat": "CONFIG_USE_WECHAT_MESSAGE_STYLE",
    "emote": "CONFIG_USE_EMOTE_MESSAGE_STYLE",
}
_DYNAMIC_CAMERA_MIRROR_BOARD_CONFIGS = {
    # These boards intentionally change orientation at runtime according to the
    # detected sensor or persisted device state. A compile-time override would
    # be misleading because that runtime decision would win afterwards.
    "CONFIG_BOARD_TYPE_DF_S3_AI_CAM",
    "CONFIG_BOARD_TYPE_ESP_SPARKBOT",
    "CONFIG_BOARD_TYPE_M5STACK_ATOM_S3R_CAM_M12_ECHO_BASE",
    "CONFIG_BOARD_TYPE_SEEED_STUDIO_SENSECAP_WATCHER",
}
_OPTIONAL_CAMERA_ENABLE_SYMBOLS = {
    # ESP-VOCAT only constructs EspVideo when its optional USB UVC transport
    # is enabled. Do not advertise mirror controls for the camera-less default
    # build, but expose them automatically for an explicitly enabled variant.
    "CONFIG_BOARD_TYPE_ESP_VOCAT": "CONFIG_ESP_VIDEO_ENABLE_USB_UVC_VIDEO_DEVICE",
}
# Match both `new Esp32Camera` and `new (std::nothrow) Esp32Camera` (and EspVideo).
_COMMON_CAMERA_CONSTRUCTOR_RE = re.compile(
    r"\bnew(?:\s*\(\s*std::nothrow\s*\))?\s+Esp(?:32Camera|Video)\b"
)


def _sdkconfig_assignments(options: list[str]) -> dict[str, str]:
    """Return the final value for each CONFIG_* assignment in a fragment."""
    assignments: dict[str, str] = {}
    for option in options:
        key, separator, value = option.strip().partition("=")
        if not separator or not key.startswith("CONFIG_"):
            raise ValueError(f"Invalid sdkconfig assignment: {option!r}")
        assignments[key] = value
    return assignments


def _kconfig_choice(
    name: str,
    kconfig_path: Path = Path("main/Kconfig.projbuild"),
) -> dict[str, Any]:
    """Read a named project choice without trying to reimplement Kconfig."""
    content = kconfig_path.read_text(encoding="utf-8")
    start = re.search(rf"^choice\s+{re.escape(name)}\s*$", content, re.MULTILINE)
    if not start:
        raise ValueError(f"Kconfig choice {name} was not found in {kconfig_path}")
    end = re.search(r"^endchoice\s*$", content[start.end():], re.MULTILINE)
    if not end:
        raise ValueError(f"Kconfig choice {name} has no endchoice")
    block = content[start.end():start.end() + end.start()]
    entries: list[dict[str, str]] = []
    matches = list(re.finditer(r"^\s*config\s+([A-Za-z0-9_]+)\s*$", block, re.MULTILINE))
    for index, match in enumerate(matches):
        entry_end = matches[index + 1].start() if index + 1 < len(matches) else len(block)
        entry = block[match.end():entry_end]
        label = re.search(r'^\s*bool\s+"([^"]+)"', entry, re.MULTILINE)
        if label:
            entries.append({"value": match.group(1), "label": label.group(1)})
    default = re.search(r"^\s*default\s+([A-Za-z0-9_]+)", block, re.MULTILINE)
    return {
        "entries": entries,
        "default": default.group(1) if default else None,
        "board_configs": {
            f"CONFIG_{symbol}"
            for symbol in re.findall(r"\b(BOARD_TYPE_[A-Za-z0-9_]+)\b", block)
        },
        "block": block,
    }


def _kconfig_has_definition(
    kind: str,
    name: str,
    kconfig_path: Path = Path("main/Kconfig.projbuild"),
) -> bool:
    """Check whether an optional choice or config still exists in Kconfig."""
    return bool(
        re.search(
            rf"^\s*{re.escape(kind)}\s+{re.escape(name)}\s*$",
            kconfig_path.read_text(encoding="utf-8"),
            re.MULTILINE,
        )
    )


def _kconfig_config_board_dependencies(
    symbol: str,
    kconfig_path: Path = Path("main/Kconfig.projbuild"),
) -> set[str]:
    """Return CONFIG_BOARD_TYPE_* names mentioned by one config entry."""
    content = kconfig_path.read_text(encoding="utf-8")
    start = re.search(rf"^\s*config\s+{re.escape(symbol)}\s*$", content, re.MULTILINE)
    if not start:
        raise ValueError(f"Kconfig config {symbol} was not found in {kconfig_path}")
    remainder = content[start.end():]
    end = re.search(r"^\s*(?:config\s+|endchoice\s*$|endmenu\s*$)", remainder, re.MULTILINE)
    block = remainder[:end.start()] if end else remainder
    return {
        f"CONFIG_{item}"
        for item in re.findall(r"\b(BOARD_TYPE_[A-Za-z0-9_]+)\b", block)
    }


def _board_source_text() -> str:
    board_dir = _BOARDS_DIR
    parts: list[str] = []
    for path in sorted(board_dir.rglob("*")):
        if path.suffix in {".c", ".cc", ".cpp", ".h", ".hpp"}:
            parts.append(path.read_text(encoding="utf-8", errors="replace"))
    return "\n".join(parts)


def _selected_choice_default(
    choice: dict[str, Any],
    assignments: dict[str, str],
) -> str:
    for entry in choice["entries"]:
        if assignments.get(f"CONFIG_{entry['value']}") == "y":
            return str(entry["value"])
    default = choice.get("default")
    if not default:
        raise ValueError("Exposed Kconfig choice has no default")
    return str(default)


def _build_option_definitions(
    target: str,
    board_config: str,
    build: dict[str, Any],
) -> list[dict[str, Any]]:
    """Describe the curated, board-compatible options exposed to callers."""
    sdkconfig_append = build.get("sdkconfig_append", [])
    if not isinstance(sdkconfig_append, list) or not all(isinstance(item, str) for item in sdkconfig_append):
        raise ValueError(f"build {build.get('name')!r} sdkconfig_append must be a string list")
    assignments = _sdkconfig_assignments(sdkconfig_append)
    source = _board_source_text()
    definitions: list[dict[str, Any]] = []

    for choice_name in ("DISPLAY_OLED_TYPE", "DISPLAY_LCD_TYPE"):
        if not _kconfig_has_definition("choice", choice_name):
            continue
        choice = _kconfig_choice(choice_name)
        if board_config not in choice["board_configs"]:
            continue
        entries = [
            entry
            for entry in choice["entries"]
            if entry["value"] != "LCD_CUSTOM"
            or assignments.get("CONFIG_LCD_CUSTOM") == "y"
        ]
        definitions.append({
            "key": "display_model",
            "type": "select",
            "default": _selected_choice_default(choice, assignments),
            "choices": entries,
        })
        break

    # Message styles are implemented by the color LCD display path. OLED and
    # no-display boards deliberately do not expose a selector that has no effect.
    if (
        re.search(r"\b[A-Za-z0-9_]*LcdDisplay\b", source)
        and _kconfig_has_definition("choice", "DISPLAY_STYLE")
    ):
        style_choice = _kconfig_choice("DISPLAY_STYLE")
        emote_boards = _kconfig_config_board_dependencies("USE_EMOTE_MESSAGE_STYLE")
        style_choices = [
            {"value": "default", "label": "Default"},
            {"value": "wechat", "label": "WeChat"},
        ]
        if board_config in emote_boards:
            style_choices.append({"value": "emote", "label": "Emote animation"})
        style_default = "default"
        selected_style = _selected_choice_default(style_choice, assignments)
        for value, symbol in _DISPLAY_STYLE_SYMBOLS.items():
            if symbol == f"CONFIG_{selected_style}":
                style_default = value
                break
        definitions.extend((
            {
                "key": "display_style",
                "type": "select",
                "default": style_default,
                "choices": style_choices,
            },
            {
                "key": "multiline_chat",
                "type": "boolean",
                "default": assignments.get("CONFIG_USE_MULTILINE_CHAT_MESSAGE") == "y",
            },
        ))

    aec_boards = (
        _kconfig_config_board_dependencies("USE_DEVICE_AEC")
        if _kconfig_has_definition("config", "USE_DEVICE_AEC")
        else set()
    )
    if board_config in aec_boards:
        definitions.append({
            "key": "aec_mode",
            "type": "select",
            "default": "device" if assignments.get("CONFIG_USE_DEVICE_AEC") == "y" else "off",
            "choices": [
                {"value": "off", "label": "Off"},
                {"value": "device", "label": "Device-side AEC"},
            ],
        })

    # ESP32-P4 obtains networking through a companion chip and cannot enable
    # the local ESP-BluFi stack selected by this project option.
    if target != "esp32p4" and ("wifi_board.h" in source or re.search(r"\bWifiBoard\b", source)):
        definitions.append({
            "key": "wifi_provisioning",
            "type": "select",
            "default": (
                "blufi"
                if assignments.get("CONFIG_USE_ESP_BLUFI_WIFI_PROVISIONING") == "y"
                and assignments.get("CONFIG_USE_HOTSPOT_WIFI_PROVISIONING") == "n"
                else "hotspot"
            ),
            "choices": [
                {"value": "hotspot", "label": "Wi-Fi hotspot"},
                {"value": "blufi", "label": "ESP-BluFi"},
            ],
        })

    camera_enable_symbol = _OPTIONAL_CAMERA_ENABLE_SYMBOLS.get(board_config)
    has_common_camera = (
        _COMMON_CAMERA_CONSTRUCTOR_RE.search(source) is not None
        and (
            camera_enable_symbol is None
            or assignments.get(camera_enable_symbol) == "y"
        )
    )
    if has_common_camera and board_config not in _DYNAMIC_CAMERA_MIRROR_BOARD_CONFIGS:
        definitions.extend((
            {"key": "camera_hmirror", "type": "boolean", "default": False},
            {"key": "camera_vflip", "type": "boolean", "default": False},
        ))

    configured_defaults = build.get("build_options", {})
    if not isinstance(configured_defaults, dict):
        raise ValueError(f"build {build.get('name')!r} build_options must be an object")
    by_key = {definition["key"]: definition for definition in definitions}
    unknown_defaults = sorted(set(configured_defaults) - set(by_key))
    if unknown_defaults:
        raise ValueError(
            f"build {build.get('name')!r} has unsupported build_options defaults: "
            + ", ".join(unknown_defaults)
        )
    for key, value in configured_defaults.items():
        by_key[key]["default"] = value

    # Validate board defaults through the same path used for caller input.
    _normalize_build_options(definitions, {})
    return definitions


def _normalize_build_options(
    definitions: list[dict[str, Any]],
    requested: object,
) -> dict[str, object]:
    if not isinstance(requested, dict):
        raise ValueError("--build-options-json must contain a JSON object")
    by_key = {definition["key"]: definition for definition in definitions}
    unknown = sorted(set(requested) - set(by_key))
    if unknown:
        raise ValueError("Unsupported build option(s): " + ", ".join(unknown))

    normalized: dict[str, object] = {}
    for key in sorted(by_key):
        definition = by_key[key]
        value = requested.get(key, definition["default"])
        if definition["type"] == "boolean":
            if not isinstance(value, bool):
                raise ValueError(f"Build option {key} must be a boolean")
        elif definition["type"] == "select":
            allowed = {choice["value"] for choice in definition["choices"]}
            if not isinstance(value, str) or value not in allowed:
                raise ValueError(
                    f"Build option {key} must be one of: {', '.join(sorted(allowed))}"
                )
        normalized[key] = value

    if normalized.get("display_style") != "default" and "multiline_chat" in normalized:
        normalized["multiline_chat"] = False
    return normalized


def _build_options_sdkconfig(
    definitions: list[dict[str, Any]],
    options: dict[str, object],
    base_assignments: dict[str, str],
) -> list[str]:
    """Expand semantic build options into a complete, mutually-exclusive fragment."""
    by_key = {definition["key"]: definition for definition in definitions}
    result: list[str] = []

    if "display_model" in options:
        selected = options["display_model"]
        for choice in by_key["display_model"]["choices"]:
            result.append(f"CONFIG_{choice['value']}={'y' if choice['value'] == selected else 'n'}")
        if isinstance(selected, str) and selected.startswith("LCD_"):
            # LCD_CUSTOM is intentionally not exposed in the cloud UI because
            # it requires source-level panel configuration, but it is still a
            # sibling in the Kconfig choice and must be disabled explicitly.
            result.append("CONFIG_LCD_CUSTOM=n")

    if "display_style" in options:
        selected = options["display_style"]
        for choice in by_key["display_style"]["choices"]:
            value = choice["value"]
            symbol = _DISPLAY_STYLE_SYMBOLS[value]
            result.append(f"{symbol}={'y' if value == selected else 'n'}")
        flash_symbols = (
            "CONFIG_FLASH_NONE_ASSETS",
            "CONFIG_FLASH_DEFAULT_ASSETS",
            "CONFIG_FLASH_CUSTOM_ASSETS",
            "CONFIG_FLASH_EXPRESSION_ASSETS",
        )
        if selected == "emote" and base_assignments.get("CONFIG_FLASH_CUSTOM_ASSETS") != "y":
            result.extend(
                f"{symbol}={'y' if symbol == 'CONFIG_FLASH_EXPRESSION_ASSETS' else 'n'}"
                for symbol in flash_symbols
            )
        elif selected != "emote" and base_assignments.get("CONFIG_FLASH_EXPRESSION_ASSETS") == "y":
            result.extend(
                f"{symbol}={'y' if symbol == 'CONFIG_FLASH_DEFAULT_ASSETS' else 'n'}"
                for symbol in flash_symbols
            )

    if "multiline_chat" in options:
        result.append(f"CONFIG_USE_MULTILINE_CHAT_MESSAGE={'y' if options['multiline_chat'] else 'n'}")

    if "aec_mode" in options:
        device = options["aec_mode"] == "device"
        result.extend((
            f"CONFIG_USE_DEVICE_AEC={'y' if device else 'n'}",
            "CONFIG_USE_SERVER_AEC=n",
        ))
        if device:
            result.append("CONFIG_USE_AUDIO_PROCESSOR=y")

    if "wifi_provisioning" in options:
        blufi = options["wifi_provisioning"] == "blufi"
        result.extend((
            f"CONFIG_USE_HOTSPOT_WIFI_PROVISIONING={'n' if blufi else 'y'}",
            f"CONFIG_USE_ESP_BLUFI_WIFI_PROVISIONING={'y' if blufi else 'n'}",
        ))

    if "camera_hmirror" in options or "camera_vflip" in options:
        result.extend((
            "CONFIG_XIAOZHI_CAMERA_MIRROR_CONFIGURED=y",
            f"CONFIG_XIAOZHI_CAMERA_HMIRROR={'y' if options.get('camera_hmirror') else 'n'}",
            f"CONFIG_XIAOZHI_CAMERA_VFLIP={'y' if options.get('camera_vflip') else 'n'}",
        ))
    return result


def _parse_version(value: str) -> tuple[int, int, int]:
    """Parse an ESP-IDF version string such as 5.5.4 or v6.0."""
    match = re.search(r"v?(\d+)\.(\d+)(?:\.(\d+))?", value)
    if not match:
        raise ValueError(f"Invalid ESP-IDF version: {value}")
    return tuple(int(part or 0) for part in match.groups())


def _detect_idf_version() -> tuple[int, int, int]:
    """Resolve the active ESP-IDF version for version-gated build variants."""
    idf_path = os.environ.get("IDF_PATH")
    if idf_path:
        version_file = Path(idf_path) / "tools/cmake/version.cmake"
        if version_file.exists():
            values: dict[str, int] = {}
            for line in version_file.read_text(encoding="utf-8").splitlines():
                match = re.match(r"set\(IDF_VERSION_(MAJOR|MINOR|PATCH)\s+(\d+)\)", line)
                if match:
                    values[match.group(1)] = int(match.group(2))
            if all(part in values for part in ("MAJOR", "MINOR", "PATCH")):
                return values["MAJOR"], values["MINOR"], values["PATCH"]

    try:
        output = subprocess.run(
            ["idf.py", "--version"],
            check=True,
            capture_output=True,
            text=True,
        ).stdout
        return _parse_version(output)
    except (FileNotFoundError, subprocess.CalledProcessError, ValueError) as error:
        raise RuntimeError(
            "ESP-IDF version was not detected. Source export.sh before running build.py."
        ) from error


def _detect_idf_version_for_listing() -> tuple[int, int, int]:
    """Detect IDF when available, otherwise list the preferred IDF 6.0 variants."""
    try:
        return _detect_idf_version()
    except RuntimeError:
        version = ".".join(str(part) for part in _DEFAULT_IDF_VERSION)
        print(
            f"[WARN] ESP-IDF is not active; listing variants for ESP-IDF {version}.",
            file=sys.stderr,
        )
        return _DEFAULT_IDF_VERSION


def _version_matches(version: tuple[int, int, int], expression: str) -> bool:
    """Evaluate a single comparison such as '<6.0' or '>=6.0.1'."""
    match = re.fullmatch(r"\s*(<=|>=|<|>|==)\s*(v?\d+\.\d+(?:\.\d+)?)\s*", expression)
    if not match:
        raise ValueError(f"Invalid ESP-IDF version expression: {expression}")
    operator, expected_text = match.groups()
    expected = _parse_version(expected_text)
    return {
        "<": version < expected,
        "<=": version <= expected,
        ">": version > expected,
        ">=": version >= expected,
        "==": version == expected,
    }[operator]


def _get_builds_for_idf(cfg: dict, idf_version: tuple[int, int, int]) -> list[dict]:
    """Return build entries whose optional ESP-IDF version rule matches."""
    builds: list[dict] = []
    for build in cfg.get("builds", []):
        _get_reported_name(build)
        expression = build.get("idf_version")
        if expression and not _version_matches(idf_version, expression):
            continue
        builds.append(dict(build))
    return builds


def _read_project_build(
    idf_version: tuple[int, int, int],
) -> tuple[dict[str, Any], dict[str, Any]]:
    """Read the single project build, keeping OTA identity in metadata."""
    with _BUILD_CONFIG.open(encoding="utf-8") as file:
        cfg = json.load(file)
    if not isinstance(cfg, dict):
        raise ValueError(f"{_BUILD_CONFIG}: expected a JSON object")
    _get_reported_type(cfg)
    if cfg.get("target") != "esp32s3":
        raise ValueError(f"{_BUILD_CONFIG}: Ruby RobotCar requires target esp32s3")
    if not isinstance(cfg.get("preview", False), bool):
        raise ValueError(f"{_BUILD_CONFIG}: preview must be a boolean")
    builds = cfg.get("builds")
    if not isinstance(builds, list) or len(builds) != 1 or not isinstance(builds[0], dict):
        raise ValueError(f"{_BUILD_CONFIG}: exactly one build entry is required")
    compatible = _get_builds_for_idf(cfg, idf_version)
    if not compatible:
        raise ValueError(f"{_BUILD_CONFIG}: build does not support ESP-IDF {idf_version}")
    build = compatible[0]
    options = build.get("sdkconfig_append", [])
    if not isinstance(options, list) or not all(isinstance(item, str) for item in options):
        raise ValueError(f"{_BUILD_CONFIG}: sdkconfig_append must be a string list")
    _sdkconfig_assignments(options)
    return cfg, build


def _collect_variants(
    idf_version: tuple[int, int, int] = (6, 0, 0),
) -> list[dict[str, Any]]:
    """Expose the single build through the existing metadata listing format."""
    cfg, build = _read_project_build(idf_version)
    target = cfg["target"]
    options = build.get("sdkconfig_append", [])
    config_symbol = _resolve_board_config(target, options)
    return [{
        "board": _PROJECT_BOARD,
        "name": _get_reported_name(build),
        "full_name": _get_release_full_name(_get_manufacturer(cfg), build),
        "type": _get_reported_type(cfg),
        "target": target,
        "config": config_symbol,
        "display_name": "Ruby RobotCar (ESP32-S3)",
        "wake_word_supported": _board_supports_wake_word(target, options),
        "build_options": _build_option_definitions(target, config_symbol, build),
    }]


def _select_variants_for_changes(
    variants: list[dict[str, Any]], changed_files: list[str]
) -> list[dict[str, Any]]:
    """Select the project build when its source or configuration changes."""
    global_paths = {
        ".github/workflows/build.yml",
        "CMakeLists.txt",
        "scripts/build_default_assets.py",
        "scripts/build.py",
        "scripts/gen_lang.py",
        "scripts/versions.py",
    }
    for raw_path in changed_files:
        path = raw_path.strip()
        if (path in global_paths or
                path.startswith(("main/", "components/", "config/", "partitions/")) or
                path.startswith("sdkconfig.defaults")):
            return variants
    return []


def _extract_board_config_from_sdkconfig_append(sdkconfig_append: list[str]) -> Optional[str]:
    """Extract explicit CONFIG_BOARD_TYPE_xxx=y from sdkconfig_append, if present."""
    pattern = re.compile(r"^(CONFIG_BOARD_TYPE_[A-Za-z0-9_]+)=y$")
    matches = []
    for item in sdkconfig_append:
        m = pattern.match(item.strip())
        if m:
            matches.append(m.group(1))
    if not matches:
        return None
    uniq = list(dict.fromkeys(matches))
    if len(uniq) > 1:
        raise ValueError(f"Multiple board type configs found in sdkconfig_append: {uniq}")
    return uniq[0]


def _board_config_symbol_exists(
    symbol: str,
    kconfig_path: Path = Path("main/Kconfig.projbuild"),
) -> bool:
    """Return whether a CONFIG_BOARD_TYPE_* symbol exists in project Kconfig."""
    if not kconfig_path.exists():
        return False
    name = symbol.removeprefix("CONFIG_")
    return bool(re.search(
        rf"^\s*config\s+{re.escape(name)}\s*$",
        kconfig_path.read_text(encoding="utf-8"),
        re.MULTILINE,
    ))


def _symbol_supports_target(symbol: str, target: str) -> bool:
    """Check whether Kconfig symbol depends on given target (e.g. esp32c5)."""
    kconfig_file = Path("main/Kconfig.projbuild")
    if not kconfig_file.exists():
        return False

    target_flag = f"IDF_TARGET_{target.upper()}"
    symbol = symbol.removeprefix("CONFIG_")
    lines = kconfig_file.read_text(encoding="utf-8").splitlines()

    in_symbol = False
    for line in lines:
        stripped = line.strip()
        if stripped.startswith("config "):
            curr_symbol = stripped.split("config ", 1)[1].strip()
            in_symbol = curr_symbol == symbol
            continue
        if in_symbol and stripped.startswith(("config ", "choice ", "endchoice", "menu ", "endmenu")):
            break
        if (
            in_symbol
            and "depends on" in stripped
            and re.search(
                rf"(?<![A-Za-z0-9_]){re.escape(target_flag)}(?![A-Za-z0-9_])",
                stripped,
            )
        ):
            return True
    return False


def _resolve_board_config(target: str, sdkconfig_append: list[str]) -> str:
    """Keep the existing Kconfig symbol without selecting a board directory."""
    explicit = _extract_board_config_from_sdkconfig_append(sdkconfig_append)
    if explicit is not None and explicit != _BOARD_CONFIG_SYMBOL:
        raise ValueError(f"{_BUILD_CONFIG}: unsupported board config {explicit}")
    if not _board_config_symbol_exists(_BOARD_CONFIG_SYMBOL):
        raise ValueError(f"Project Kconfig is missing {_BOARD_CONFIG_SYMBOL}")
    if not _symbol_supports_target(_BOARD_CONFIG_SYMBOL, target):
        raise ValueError(f"Project configuration does not support target {target!r}")
    return _BOARD_CONFIG_SYMBOL


def _apply_auto_selects(sdkconfig_append: list[str]) -> list[str]:
    """Apply hardcoded auto-select rules to sdkconfig_append."""
    items = list(sdkconfig_append)

    # Apply auto-select rules
    for key, deps in _AUTO_SELECT_RULES.items():
        for entry in sdkconfig_append:
            name, _, value = entry.partition("=")
            if name == key and value.lower().startswith("y"):
                # A board preset may explicitly disable a dependency. Kconfig's
                # select would override it, so the generated defaults fragment
                # must do the same instead of keeping the earlier value.
                items = _merge_sdkconfig_options(items, deps)
                break

    return items


def _merge_sdkconfig_options(
    base_options: list[str],
    override_options: list[str],
) -> list[str]:
    """Merge sdkconfig assignments by key, with later overrides winning."""
    keys: list[str] = []
    values: dict[str, str] = {}
    for option in (*base_options, *override_options):
        key = option.split("=", 1)[0]
        if key not in values:
            keys.append(key)
        values[key] = option
    return [values[key] for key in keys]


################################################################################
# Compile implementation
################################################################################


def _target_from_sdkconfig() -> Optional[str]:
    sdkconfig = Path("sdkconfig")
    if not sdkconfig.exists():
        return None
    match = re.search(
        r'^CONFIG_IDF_TARGET="([^"]+)"$',
        sdkconfig.read_text(encoding="utf-8"),
        re.MULTILINE,
    )
    return match.group(1) if match else None


def _target_from_cmake_cache() -> Optional[str]:
    cache = Path("build/CMakeCache.txt")
    if not cache.exists():
        return None
    match = re.search(
        r"^IDF_TARGET(?::[^=]+)?=(.+)$",
        cache.read_text(encoding="utf-8"),
        re.MULTILINE,
    )
    return match.group(1).strip() if match else None


def _configured_target() -> Optional[str]:
    """Return the current target when sdkconfig and CMake state agree."""
    sdkconfig_target = _target_from_sdkconfig()
    cache_target = _target_from_cmake_cache()
    if sdkconfig_target and cache_target and sdkconfig_target != cache_target:
        return None
    return cache_target or sdkconfig_target


def _sync_vscode_target(
    target: str,
    settings_path: Path = Path(".vscode/settings.json"),
) -> bool:
    """Keep an existing VS Code ESP-IDF target setting in sync."""
    if not settings_path.exists():
        return False

    content = settings_path.read_text(encoding="utf-8")
    pattern = re.compile(
        r'(?P<prefix>"IDF_TARGET"\s*:\s*")'
        r'(?:\\.|[^"\\])*'
        r'(?P<suffix>")'
    )
    updated, matches = pattern.subn(
        lambda match: f'{match.group("prefix")}{target}{match.group("suffix")}',
        content,
    )
    if matches == 0 or updated == content:
        return False

    settings_path.write_text(updated, encoding="utf-8")
    print(f"[INFO] Updated {settings_path} IDF_TARGET to {target}.")
    return True


def _prepare_target(target: str, preview: bool) -> None:
    """Clean only when an existing CMake build uses another target."""
    cache_target = _target_from_cmake_cache()
    current_target = _configured_target()
    if current_target == target:
        print(f"[INFO] Reusing target {target}.")
        return

    if cache_target and cache_target != target:
        print(f"[INFO] Switching target from {cache_target} to {target}.")
        _run_idf("fullclean", preview=preview)
    elif current_target:
        print(f"[INFO] Configuring target {target} (was {current_target}).")
    else:
        print(f"[INFO] Configuring target {target}.")


def _configure_build(
    target: str,
    sdkconfig_append: list[str],
    board_name: str,
    preview: bool,
) -> None:
    """Configure target, board identity and sdkconfig defaults in one CMake run."""
    sdkconfig = Path("sdkconfig")
    sdkconfig_old = Path("sdkconfig.old")
    if sdkconfig.exists():
        if sdkconfig_old.exists():
            sdkconfig_old.unlink()
        sdkconfig.replace(sdkconfig_old)

    fragment = Path("build/xiaozhi-build.sdkconfig.defaults")
    fragment.parent.mkdir(parents=True, exist_ok=True)
    fragment.write_text(
        "# Generated by scripts/build.py\n"
        + "\n".join(sdkconfig_append)
        + "\n",
        encoding="utf-8",
    )

    defaults = []
    if Path("sdkconfig.defaults").exists():
        defaults.append("sdkconfig.defaults")
    defaults.append(fragment.as_posix())
    _run_idf(
        f"-DIDF_TARGET={target}",
        f"-DSDKCONFIG_DEFAULTS={';'.join(defaults)}",
        f"-DBOARD_NAME={board_name}",
        "reconfigure",
        preview=preview,
    )
    _sync_vscode_target(target)


def _validate_configured_symbols(symbols: list[str], option_name: str) -> None:
    """Ensure Kconfig accepted every user-selected build option."""
    if not symbols:
        return
    sdkconfig = Path("sdkconfig")
    if not sdkconfig.exists():
        raise RuntimeError(
            f"Cannot validate {option_name}: sdkconfig was not generated"
        )
    configured = sdkconfig.read_text(encoding="utf-8")
    missing = [
        symbol
        for symbol in symbols
        if not re.search(rf"^{re.escape(symbol)}=y$", configured, re.MULTILINE)
    ]
    if missing:
        raise ValueError(
            f"{option_name} is incompatible with this board or ESP-IDF "
            f"configuration; Kconfig rejected: {', '.join(missing)}"
        )


def _validate_configured_options(options: list[str], option_name: str) -> None:
    """Ensure Kconfig accepted each requested y/n semantic build option."""
    if not options:
        return
    sdkconfig = Path("sdkconfig")
    if not sdkconfig.exists():
        raise RuntimeError(f"Cannot validate {option_name}: sdkconfig was not generated")
    content = sdkconfig.read_text(encoding="utf-8")
    rejected: list[str] = []
    for option in options:
        key, _, expected = option.partition("=")
        if expected == "y":
            accepted = bool(re.search(rf"^{re.escape(key)}=y$", content, re.MULTILINE))
        elif expected == "n":
            # Kconfig may omit a disabled symbol entirely when its dependency
            # is not satisfied. Absence is equivalent to "not set"; only an
            # accepted y value contradicts a requested n value.
            accepted = not bool(re.search(
                rf"^{re.escape(key)}=y$",
                content,
                re.MULTILINE,
            ))
        else:
            continue
        if not accepted:
            rejected.append(option)
    if rejected:
        raise ValueError(
            f"{option_name} is incompatible with this board or ESP-IDF "
            f"configuration; Kconfig rejected: {', '.join(rejected)}"
        )


def build_project(
    *,
    create_zip: bool = False,
    language: Optional[str] = None,
    wake_word: Optional[str] = None,
    build_options: Optional[dict[str, object]] = None,
    idf_version: tuple[int, int, int] = (6, 0, 0),
) -> None:
    """Compile the sole project build using its existing OTA identity."""
    cfg, build = _read_project_build(idf_version)
    project_version = get_project_version()
    print(f"Project Version: {project_version} ({_BUILD_CONFIG})")
    target = cfg["target"]
    reported_type = _get_reported_type(cfg)
    preview = cfg.get("preview", False)
    manufacturer = _get_manufacturer(cfg)

    name = _get_reported_name(build)
    final_name = _get_release_full_name(manufacturer, build)

    # Process sdkconfig_append
    build_sdkconfig_append = build.get("sdkconfig_append", [])
    board_type_config = _resolve_board_config(target, build_sdkconfig_append)
    sdkconfig_append = _merge_sdkconfig_options(
        build_sdkconfig_append,
        [f"{board_type_config}=y"],
    )

    option_definitions = _build_option_definitions(
        target,
        board_type_config,
        build,
    )

    user_options: list[str] = []
    validation_symbols: list[tuple[list[str], str]] = [
        ([board_type_config], "project configuration"),
    ]
    build_option_sdkconfig: list[str] = []
    selected_language = None
    selected_wake_word = None
    selected_build_options: Optional[dict[str, object]] = None
    if language is not None:
        selected_language, option = _language_sdkconfig_option(language)
        user_options.append(option)
        validation_symbols.append(
            ([option.split("=", 1)[0]], "--language")
        )
    if wake_word is not None:
        (
            selected_wake_word,
            wake_word_options,
            wake_word_symbols,
        ) = _wake_word_sdkconfig_options(wake_word, target)
        user_options.extend(wake_word_options)
        validation_symbols.append((wake_word_symbols, "--wake-word"))

    # A build entry may carry semantic defaults to preserve an existing
    # board-specific behavior after it moves out of hardcoded C++.
    if build_options is not None or "build_options" in build:
        selected_build_options = _normalize_build_options(
            option_definitions,
            build_options or {},
        )
        build_option_sdkconfig = _build_options_sdkconfig(
            option_definitions,
            selected_build_options,
            _sdkconfig_assignments(build_sdkconfig_append),
        )
        user_options.extend(build_option_sdkconfig)

    sdkconfig_append = _merge_sdkconfig_options(
        sdkconfig_append,
        user_options,
    )
    sdkconfig_append = _apply_auto_selects(sdkconfig_append)

    print("-" * 80)
    print(f"name: {final_name}")
    if final_name != name:
        print(f"reported_name: {name}")
    print(f"reported_type: {reported_type}")
    print(f"target: {target}")
    if manufacturer:
        print(f"manufacturer: {manufacturer}")
    if selected_language:
        print(f"language: {selected_language}")
    if selected_wake_word:
        print(f"wake_word: {selected_wake_word}")
    if selected_build_options is not None:
        print(
            "build_options: "
            + json.dumps(selected_build_options, ensure_ascii=False, sort_keys=True)
        )
    for item in sdkconfig_append:
        print(f"sdkconfig_append: {item}")

    _emit_build_stage("dependencies_resolving")
    os.environ.pop("IDF_TARGET", None)
    _prepare_target(target, preview)
    _configure_build(
        target,
        sdkconfig_append,
        name,
        preview,
    )
    for symbols, option_name in validation_symbols:
        _validate_configured_symbols(symbols, option_name)
    _validate_configured_options(build_option_sdkconfig, "--build-options-json")

    # build.name is the compatibility-sensitive OTA-reported board identity.
    _emit_build_stage("compiling")
    _run_idf("build", preview=preview)

    # merge-bin
    _emit_build_stage("packaging")
    merge_bin(preview)

    if create_zip:
        zip_bin(final_name, project_version)

################################################################################
# CLI entry
################################################################################


def _print_board_list(variants: list[dict[str, Any]]) -> None:
    for variant in variants:
        print(f"{variant['display_name']}: {variant['name']} ({variant['target']})")


def main(argv: Optional[list[str]] = None) -> None:
    parser = argparse.ArgumentParser(
        description="Configure and build the Ruby RobotCar firmware.",
    )
    parser.add_argument("--list-boards", action="store_true", help="Show the single project build")
    parser.add_argument(
        "--list-languages",
        action="store_true",
        help="List values accepted by --language",
    )
    parser.add_argument(
        "--list-wake-words",
        action="store_true",
        help="List wake-word models provided by the current ESP-SR component",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Output list results in JSON format",
    )
    parser.add_argument(
        "--language",
        metavar="LOCALE",
        help="Firmware language locale, for example zh-CN or en-US",
    )
    parser.add_argument(
        "--wake-word",
        metavar="MODEL",
        help=(
            "Wake-word model (for example wn9_jarvis_tts), "
            "'nihaoxiaozhi', or 'disabled'"
        ),
    )
    parser.add_argument(
        "--build-options-json",
        metavar="JSON",
        help=(
            "Project options as a JSON object. Accepted keys are "
            "reported by --list-boards --json."
        ),
    )
    parser.add_argument(
        "--zip",
        action="store_true",
        help="Also recreate releases/v<version>_<name>.zip",
    )
    parser.add_argument(
        "--select-changed",
        action="store_true",
        help="Read changed paths from stdin and output the affected project build as JSON",
    )

    cli_args = sys.argv[1:] if argv is None else argv
    args = parser.parse_args(cli_args)

    if args.select_changed:
        if (
            args.list_boards
            or args.list_languages
            or args.list_wake_words
            or args.language
            or args.wake_word
            or args.build_options_json
            or args.zip
            or args.json
        ):
            parser.error("--select-changed cannot be combined with build or list options")
        idf_version = _detect_idf_version_for_listing()
        variants = _collect_variants(idf_version=idf_version)
        selected = _select_variants_for_changes(variants, sys.stdin.read().splitlines())
        print(json.dumps(selected))
        return

    if args.list_languages:
        if (
            args.list_boards
            or args.list_wake_words
            or args.language
            or args.wake_word
            or args.build_options_json
            or args.zip
        ):
            parser.error(
                "--list-languages cannot be combined with build or other "
                "list options"
            )
        languages = _collect_languages()
        if args.json:
            print(json.dumps(languages))
        else:
            print("\n".join(languages))
        return

    if args.list_wake_words:
        if (
            args.list_boards
            or args.language
            or args.wake_word
            or args.build_options_json
            or args.zip
        ):
            parser.error(
                "--list-wake-words cannot be combined with build or other "
                "list options"
            )
        try:
            wake_words = _collect_wake_words()
        except RuntimeError as error:
            print(f"[ERROR] {error}", file=sys.stderr)
            sys.exit(1)
        if args.json:
            print(json.dumps(wake_words, ensure_ascii=False))
        else:
            _print_wake_word_list(wake_words)
        return

    if args.list_boards:
        if (
            args.list_languages
            or args.list_wake_words
            or args.zip
            or args.language
            or args.wake_word
            or args.build_options_json
        ):
            parser.error(
                "--list-boards cannot be combined with build or other "
                "list options"
            )
        idf_version = _detect_idf_version_for_listing()
        variants = _collect_variants(idf_version=idf_version)
        if args.json:
            print(json.dumps(variants))
        else:
            _print_board_list(variants)
        return

    if args.json:
        parser.error("--json is only valid with a list option")

    parsed_build_options: Optional[dict[str, object]] = None
    if args.build_options_json is not None:
        try:
            raw_build_options = json.loads(args.build_options_json)
        except json.JSONDecodeError as error:
            parser.error(f"--build-options-json is invalid JSON: {error}")
        if not isinstance(raw_build_options, dict):
            parser.error("--build-options-json must contain a JSON object")
        parsed_build_options = raw_build_options

    build_project(
        create_zip=args.zip,
        language=args.language,
        wake_word=args.wake_word,
        build_options=parsed_build_options,
        idf_version=_detect_idf_version(),
    )


if __name__ == "__main__":
    main()
