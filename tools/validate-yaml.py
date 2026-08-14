from pathlib import Path
import sys

import yaml


class HomeAssistantLoader(yaml.SafeLoader):
    pass


def home_assistant_tag(loader: HomeAssistantLoader, _tag_suffix: str, node):
    if isinstance(node, yaml.ScalarNode):
        return loader.construct_scalar(node)
    if isinstance(node, yaml.SequenceNode):
        return loader.construct_sequence(node)
    return loader.construct_mapping(node)


HomeAssistantLoader.add_multi_constructor("!", home_assistant_tag)

root = Path(__file__).resolve().parents[1]
files = [
    root / "home-assistant" / "compose.yaml",
    root / "home-assistant" / "config" / "configuration.yaml",
    root / "home-assistant" / "config" / "packages" / "pcbot.yaml",
    root / "home-assistant" / "config" / "ui-lovelace.yaml",
]

for path in files:
    try:
        with path.open(encoding="utf-8") as stream:
            yaml.load(stream, Loader=HomeAssistantLoader)
    except Exception as error:
        print(f"{path.relative_to(root)}: {error}", file=sys.stderr)
        raise SystemExit(1)

print(f"YAML valid: {len(files)} files")
