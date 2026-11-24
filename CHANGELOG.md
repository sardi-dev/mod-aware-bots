# Changelog

All notable changes to this project are documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project follows [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Fixed
- **The random password setting is read once** (2026-11-24)
  - Summary: `PlayerbotAIConfig.cpp` read `AiPlayerbot.RandomBotRandomPassword` twice in a row; the second read is gone.
  - Why: Leftover of the release cleanup; harmless, but it reads as a mistake.
- **Bots dive to a target deep in the water** (2026-11-23)
  - Summary: The surface swim lifted every point of a water path, the destination too, so a bot chasing a mob on a lake bed
    swam above it and could not hit it. The last point now keeps the destination's depth when it lies more than 1.5 yd
    under the surface; a destination near the surface (a swimming leader) is lifted as before.
  - Why: A melee bot hung on the surface over a Freshwater Eel it could not reach (Lake Everstill).

## [0.1.0] - 2026-11-23

### Added
- **First public release** (2026-11-23)
  - Summary: The module `mod-aware-bots-548` (player bots for SkyFire 5.4.8, client build 18414), the core patches it
    needs in `patches/`, and the optional AI sidecar `services/aware-ai`.
  - Why: The module ran privately for several weeks; this is its first public snapshot. The development history before
    this point is not published, so the entries of earlier private builds are not listed here.
