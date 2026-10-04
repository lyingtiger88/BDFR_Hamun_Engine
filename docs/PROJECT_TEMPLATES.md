# Hamun Project Templates

Hamun v0.5 introduces a data-driven template system and the first Windows
project launcher.

## Template discovery

The launcher scans:

```text
Templates/<TemplateFolder>/template.hamun
```

A manifest supports:

```text
id=<stable id>
name=<display name>
category=<category>
description=<description>
content=<content folder, default Content>
hidden=<true|false>
```

The selected content folder is copied into the new project. Supported text
files may contain:

```text
{{PROJECT_NAME}}
```

which is replaced during project creation.

## Current template set

v0.5 intentionally ships only one technical baseline:

- **Blank Project** — validates discovery, selection, path/name validation,
  copying and token substitution.

The final production template lineup is intentionally undecided. Candidate
categories such as blank 3D, open world, mobile, visualization, vehicle,
networked game or other presets can be added later without redesigning the
launcher.

## Launcher workflow

```text
HamunLauncher
  -> New Project
  -> Template list
  -> Template details
  -> Project name
  -> Destination
  -> Create Project
```

CI validates the same path with:

```text
HamunLauncher.exe --template-smoke-test
```

which creates and removes a temporary project.
