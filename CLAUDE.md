# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Git workflow

Always commit and push directly to `main`. Do not create feature branches unless explicitly asked.

## Choosing models for subagents

The owner has authorised this: pick the model and reasoning effort for each
subagent or workflow stage yourself, matched to how hard that piece of work is.
Do not ask first. Rough guide:

- **haiku**, low effort: mechanical, fully specified work. Searches, listing
  call sites, applying a known edit across files, pulling facts from CI logs,
  doc-consistency sweeps.
- **sonnet**, medium effort: routine work with clear acceptance criteria.
  Tests written to a spec, small bug fixes, UI text and layout tweaks.
- **The most capable model available (opus / fable)**, high effort: open-ended
  or risky work. Cross-core concurrency and memory ordering; OTA, boot, flash
  and NVS paths; anything that changes how the lamp looks; design decisions;
  adversarially verifying findings.
- `max` effort only when the owner asks for it.

When the choice mattered, mention it in one line in the report. This rule covers
model and effort selection only. It does not by itself authorise large
multi-agent fan-outs; ask before running one at scale.

@ARCHITECTURE.md
@CODING.md
